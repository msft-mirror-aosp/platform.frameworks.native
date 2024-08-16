/**
 * Copyright 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LegacyResampler"

#include <algorithm>
#include <chrono>

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <input/Resampler.h>
#include <utils/Timers.h>

using std::chrono::nanoseconds;

namespace android {

namespace {

const bool IS_DEBUGGABLE_BUILD =
#if defined(__ANDROID__)
        android::base::GetBoolProperty("ro.debuggable", false);
#else
        true;
#endif

bool debugResampling() {
    if (!IS_DEBUGGABLE_BUILD) {
        static const bool DEBUG_TRANSPORT_RESAMPLING =
                __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG "Resampling",
                                          ANDROID_LOG_INFO);
        return DEBUG_TRANSPORT_RESAMPLING;
    }
    return __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG "Resampling", ANDROID_LOG_INFO);
}

constexpr std::chrono::milliseconds RESAMPLE_LATENCY{5};

constexpr std::chrono::milliseconds RESAMPLE_MIN_DELTA{2};

constexpr std::chrono::milliseconds RESAMPLE_MAX_DELTA{20};

constexpr std::chrono::milliseconds RESAMPLE_MAX_PREDICTION{8};

inline float lerp(float a, float b, float alpha) {
    return a + alpha * (b - a);
}

PointerCoords calculateResampledCoords(const PointerCoords& a, const PointerCoords& b,
                                       float alpha) {
    // We use the value of alpha to initialize resampledCoords with the latest sample information.
    PointerCoords resampledCoords = (alpha < 1.0f) ? a : b;
    resampledCoords.isResampled = true;
    resampledCoords.setAxisValue(AMOTION_EVENT_AXIS_X, lerp(a.getX(), b.getX(), alpha));
    resampledCoords.setAxisValue(AMOTION_EVENT_AXIS_Y, lerp(a.getY(), b.getY(), alpha));
    return resampledCoords;
}
} // namespace

void LegacyResampler::updateLatestSamples(const MotionEvent& motionEvent) {
    const size_t numSamples = motionEvent.getHistorySize() + 1;
    for (size_t i = 0; i < numSamples; ++i) {
        mLatestSamples.pushBack(
                Sample{static_cast<nanoseconds>(motionEvent.getHistoricalEventTime(i)),
                       Pointer{*motionEvent.getPointerProperties(0),
                               motionEvent.getSamplePointerCoords()[i]}});
    }
}

bool LegacyResampler::canInterpolate(const InputMessage& futureSample) const {
    LOG_IF(FATAL, mLatestSamples.empty())
            << "Not resampled. mLatestSamples must not be empty to interpolate.";

    const Sample& pastSample = *(mLatestSamples.end() - 1);
    const nanoseconds delta =
            static_cast<nanoseconds>(futureSample.body.motion.eventTime) - pastSample.eventTime;
    if (delta < RESAMPLE_MIN_DELTA) {
        LOG_IF(INFO, debugResampling()) << "Not resampled. Delta is too small: " << delta << "ns.";
        return false;
    }
    return true;
}

std::optional<LegacyResampler::Sample> LegacyResampler::attemptInterpolation(
        nanoseconds resampleTime, const InputMessage& futureSample) const {
    if (!canInterpolate(futureSample)) {
        return std::nullopt;
    }
    LOG_IF(FATAL, mLatestSamples.empty())
            << "Not resampled. mLatestSamples must not be empty to interpolate.";

    const Sample& pastSample = *(mLatestSamples.end() - 1);
    const nanoseconds delta =
            static_cast<nanoseconds>(futureSample.body.motion.eventTime) - pastSample.eventTime;
    const float alpha =
            std::chrono::duration<float, std::milli>(resampleTime - pastSample.eventTime) / delta;
    const PointerCoords resampledCoords =
            calculateResampledCoords(pastSample.pointer.coords,
                                     futureSample.body.motion.pointers[0].coords, alpha);

    return Sample{resampleTime, Pointer{pastSample.pointer.properties, resampledCoords}};
}

bool LegacyResampler::canExtrapolate() const {
    if (mLatestSamples.size() < 2) {
        LOG_IF(INFO, debugResampling()) << "Not resampled. Not enough data.";
        return false;
    }

    const Sample& pastSample = *(mLatestSamples.end() - 2);
    const Sample& presentSample = *(mLatestSamples.end() - 1);

    const nanoseconds delta = presentSample.eventTime - pastSample.eventTime;
    if (delta < RESAMPLE_MIN_DELTA) {
        LOG_IF(INFO, debugResampling()) << "Not resampled. Delta is too small: " << delta << "ns.";
        return false;
    } else if (delta > RESAMPLE_MAX_DELTA) {
        LOG_IF(INFO, debugResampling()) << "Not resampled. Delta is too large: " << delta << "ns.";
        return false;
    }
    return true;
}

std::optional<LegacyResampler::Sample> LegacyResampler::attemptExtrapolation(
        nanoseconds resampleTime) const {
    if (!canExtrapolate()) {
        return std::nullopt;
    }
    LOG_IF(FATAL, mLatestSamples.size() < 2)
            << "Not resampled. mLatestSamples must have at least two samples to extrapolate.";

    const Sample& pastSample = *(mLatestSamples.end() - 2);
    const Sample& presentSample = *(mLatestSamples.end() - 1);

    const nanoseconds delta = presentSample.eventTime - pastSample.eventTime;
    // The farthest future time to which we can extrapolate. If the given resampleTime exceeds this,
    // we use this value as the resample time target.
    const nanoseconds farthestPrediction =
            presentSample.eventTime + std::min<nanoseconds>(delta / 2, RESAMPLE_MAX_PREDICTION);
    const nanoseconds newResampleTime =
            (resampleTime > farthestPrediction) ? (farthestPrediction) : (resampleTime);
    LOG_IF(INFO, debugResampling() && newResampleTime == farthestPrediction)
            << "Resample time is too far in the future. Adjusting prediction from "
            << (resampleTime - presentSample.eventTime) << " to "
            << (farthestPrediction - presentSample.eventTime) << "ns.";
    const float alpha =
            std::chrono::duration<float, std::milli>(newResampleTime - pastSample.eventTime) /
            delta;
    const PointerCoords resampledCoords =
            calculateResampledCoords(pastSample.pointer.coords, presentSample.pointer.coords,
                                     alpha);

    return Sample{newResampleTime, Pointer{presentSample.pointer.properties, resampledCoords}};
}

inline void LegacyResampler::addSampleToMotionEvent(const Sample& sample,
                                                    MotionEvent& motionEvent) {
    motionEvent.addSample(sample.eventTime.count(), &sample.pointer.coords, motionEvent.getId());
}

void LegacyResampler::resampleMotionEvent(nanoseconds resampleTime, MotionEvent& motionEvent,
                                          const InputMessage* futureSample) {
    if (mPreviousDeviceId && *mPreviousDeviceId != motionEvent.getDeviceId()) {
        mLatestSamples.clear();
    }
    mPreviousDeviceId = motionEvent.getDeviceId();

    updateLatestSamples(motionEvent);

    const std::optional<Sample> sample = (futureSample != nullptr)
            ? (attemptInterpolation(resampleTime, *futureSample))
            : (attemptExtrapolation(resampleTime));
    if (sample.has_value()) {
        addSampleToMotionEvent(*sample, motionEvent);
    }
}
} // namespace android
