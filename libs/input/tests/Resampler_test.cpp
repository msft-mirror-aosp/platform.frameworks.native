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

#include <input/Resampler.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>

#include <input/Input.h>
#include <input/InputEventBuilders.h>
#include <input/InputTransport.h>
#include <utils/Timers.h>

namespace android {

namespace {

using namespace std::literals::chrono_literals;

constexpr float EPSILON = MotionEvent::ROUNDING_PRECISION;

struct Pointer {
    int32_t id{0};
    ToolType toolType{ToolType::FINGER};
    float x{0.0f};
    float y{0.0f};
    bool isResampled{false};
    /**
     * Converts from Pointer to PointerCoords. Enables calling LegacyResampler methods and
     * assertions only with the relevant data for tests.
     */
    operator PointerCoords() const;
};

Pointer::operator PointerCoords() const {
    PointerCoords pointerCoords;
    pointerCoords.setAxisValue(AMOTION_EVENT_AXIS_X, x);
    pointerCoords.setAxisValue(AMOTION_EVENT_AXIS_Y, y);
    pointerCoords.isResampled = isResampled;
    return pointerCoords;
}

struct InputSample {
    std::chrono::milliseconds eventTime{0};
    std::vector<Pointer> pointers{};
    /**
     * Converts from InputSample to InputMessage. Enables calling LegacyResampler methods only with
     * the relevant data for tests.
     */
    operator InputMessage() const;
};

InputSample::operator InputMessage() const {
    InputMessage message;
    message.header.type = InputMessage::Type::MOTION;
    message.body.motion.pointerCount = pointers.size();
    message.body.motion.eventTime = static_cast<std::chrono::nanoseconds>(eventTime).count();
    message.body.motion.source = AINPUT_SOURCE_CLASS_POINTER;
    message.body.motion.downTime = 0;
    const uint32_t pointerCount = message.body.motion.pointerCount;
    for (uint32_t i = 0; i < pointerCount; ++i) {
        message.body.motion.pointers[i].properties.id = pointers[i].id;
        message.body.motion.pointers[i].properties.toolType = pointers[i].toolType;
        message.body.motion.pointers[i].coords.setAxisValue(AMOTION_EVENT_AXIS_X, pointers[i].x);
        message.body.motion.pointers[i].coords.setAxisValue(AMOTION_EVENT_AXIS_Y, pointers[i].y);
        message.body.motion.pointers[i].coords.isResampled = pointers[i].isResampled;
    }
    return message;
}

struct InputStream {
    std::vector<InputSample> samples{};
    int32_t action{0};
    DeviceId deviceId{0};
    /**
     * Converts from InputStream to MotionEvent. Enables calling LegacyResampler methods only with
     * the relevant data for tests.
     */
    operator MotionEvent() const;
};

InputStream::operator MotionEvent() const {
    const InputSample& firstSample{*samples.begin()};
    MotionEventBuilder motionEventBuilder =
            MotionEventBuilder(action, AINPUT_SOURCE_CLASS_POINTER)
                    .downTime(0)
                    .eventTime(static_cast<std::chrono::nanoseconds>(firstSample.eventTime).count())
                    .deviceId(deviceId);
    for (const Pointer& pointer : firstSample.pointers) {
        const PointerBuilder pointerBuilder =
                PointerBuilder(pointer.id, pointer.toolType).x(pointer.x).y(pointer.y);
        motionEventBuilder.pointer(pointerBuilder);
    }
    MotionEvent motionEvent = motionEventBuilder.build();
    const size_t numSamples = samples.size();
    for (size_t i = 1; i < numSamples; ++i) {
        std::vector<PointerCoords> pointersCoords{samples[i].pointers.begin(),
                                                  samples[i].pointers.end()};
        motionEvent.addSample(static_cast<std::chrono::nanoseconds>(samples[i].eventTime).count(),
                              pointersCoords.data(), motionEvent.getId());
    }
    return motionEvent;
}

} // namespace

class ResamplerTest : public testing::Test {
protected:
    ResamplerTest() : mResampler(std::make_unique<LegacyResampler>()) {}

    ~ResamplerTest() override {}

    void SetUp() override {}

    void TearDown() override {}

    std::unique_ptr<Resampler> mResampler;

    MotionEvent buildMotionEvent(const int32_t action, const nsecs_t eventTime,
                                 const std::vector<PointerBuilder>& pointers);

    InputMessage createMessage(const uint32_t pointerCount, const nsecs_t eventTime,
                               const int32_t action,
                               const std::vector<PointerProperties>& properties,
                               const std::vector<PointerCoords>& coords);

    /**
     * Checks that beforeCall and afterCall are equal except for the mutated attributes by addSample
     * member function.
     * @param beforeCall MotionEvent before passing it to resampleMotionEvent
     * @param afterCall MotionEvent after passing it to resampleMotionEvent
     */
    void assertMotionEventMetaDataDidNotMutate(const MotionEvent& beforeCall,
                                               const MotionEvent& afterCall);

    /**
     * Asserts the MotionEvent is resampled by checking an increment in history size and that the
     * resampled coordinates are near the expected ones.
     */
    void assertMotionEventIsResampledAndCoordsNear(const MotionEvent& original,
                                                   const MotionEvent& resampled,
                                                   const PointerCoords& expectedCoords);

    void assertMotionEventIsNotResampled(const MotionEvent& original,
                                         const MotionEvent& notResampled);
};

MotionEvent ResamplerTest::buildMotionEvent(const int32_t action, const nsecs_t eventTime,
                                            const std::vector<PointerBuilder>& pointerBuilders) {
    MotionEventBuilder motionEventBuilder = MotionEventBuilder(action, AINPUT_SOURCE_CLASS_POINTER)
                                                    .downTime(0)
                                                    .eventTime(eventTime);
    for (const PointerBuilder& pointerBuilder : pointerBuilders) {
        motionEventBuilder.pointer(pointerBuilder);
    }
    return motionEventBuilder.build();
}

InputMessage ResamplerTest::createMessage(const uint32_t pointerCount, const nsecs_t eventTime,
                                          const int32_t action,
                                          const std::vector<PointerProperties>& properties,
                                          const std::vector<PointerCoords>& coords) {
    InputMessage message;
    message.header.type = InputMessage::Type::MOTION;
    message.body.motion.pointerCount = pointerCount;
    message.body.motion.eventTime = eventTime;
    message.body.motion.source = AINPUT_SOURCE_CLASS_POINTER;
    message.body.motion.downTime = 0;
    for (uint32_t i = 0; i < pointerCount; ++i) {
        message.body.motion.pointers[i].properties = properties[i];
        message.body.motion.pointers[i].coords = coords[i];
    }
    return message;
}

void ResamplerTest::assertMotionEventMetaDataDidNotMutate(const MotionEvent& beforeCall,
                                                          const MotionEvent& afterCall) {
    EXPECT_EQ(beforeCall.getDeviceId(), afterCall.getDeviceId());
    EXPECT_EQ(beforeCall.getAction(), afterCall.getAction());
    EXPECT_EQ(beforeCall.getActionButton(), afterCall.getActionButton());
    EXPECT_EQ(beforeCall.getButtonState(), afterCall.getButtonState());
    EXPECT_EQ(beforeCall.getFlags(), afterCall.getFlags());
    EXPECT_EQ(beforeCall.getEdgeFlags(), afterCall.getEdgeFlags());
    EXPECT_EQ(beforeCall.getClassification(), afterCall.getClassification());
    EXPECT_EQ(beforeCall.getPointerCount(), afterCall.getPointerCount());
    EXPECT_EQ(beforeCall.getMetaState(), afterCall.getMetaState());
    EXPECT_EQ(beforeCall.getSource(), afterCall.getSource());
    EXPECT_EQ(beforeCall.getXPrecision(), afterCall.getXPrecision());
    EXPECT_EQ(beforeCall.getYPrecision(), afterCall.getYPrecision());
    EXPECT_EQ(beforeCall.getDownTime(), afterCall.getDownTime());
    EXPECT_EQ(beforeCall.getDisplayId(), afterCall.getDisplayId());
}

void ResamplerTest::assertMotionEventIsResampledAndCoordsNear(const MotionEvent& original,
                                                              const MotionEvent& resampled,
                                                              const PointerCoords& expectedCoords) {
    assertMotionEventMetaDataDidNotMutate(original, resampled);
    const size_t originalSampleSize = original.getHistorySize() + 1;
    const size_t resampledSampleSize = resampled.getHistorySize() + 1;
    EXPECT_EQ(originalSampleSize + 1, resampledSampleSize);
    const PointerCoords& resampledCoords =
            resampled.getSamplePointerCoords()[resampled.getHistorySize()];
    EXPECT_TRUE(resampledCoords.isResampled);
    EXPECT_NEAR(expectedCoords.getX(), resampledCoords.getX(), EPSILON);
    EXPECT_NEAR(expectedCoords.getY(), resampledCoords.getY(), EPSILON);
}

void ResamplerTest::assertMotionEventIsNotResampled(const MotionEvent& original,
                                                    const MotionEvent& notResampled) {
    assertMotionEventMetaDataDidNotMutate(original, notResampled);
    const size_t originalSampleSize = original.getHistorySize() + 1;
    const size_t notResampledSampleSize = notResampled.getHistorySize() + 1;
    EXPECT_EQ(originalSampleSize, notResampledSampleSize);
}

TEST_F(ResamplerTest, SinglePointerNotEnoughDataToResample) {
    MotionEvent motionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE,
                        .deviceId = 0};
    const MotionEvent originalMotionEvent = motionEvent;
    mResampler->resampleMotionEvent(11ms, motionEvent, nullptr);
    assertMotionEventIsNotResampled(originalMotionEvent, motionEvent);
}

TEST_F(ResamplerTest, SinglePointerDifferentDeviceIdBetweenMotionEvents) {
    MotionEvent motionFromFirstDevice =
            InputStream{{{4ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {8ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE,
                        .deviceId = 0};
    mResampler->resampleMotionEvent(10ms, motionFromFirstDevice, nullptr);
    MotionEvent motionFromSecondDevice =
            InputStream{{{11ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE,
                        .deviceId = 1};
    const MotionEvent originalMotionEvent = motionFromSecondDevice;
    mResampler->resampleMotionEvent(12ms, motionFromSecondDevice, nullptr);
    // The MotionEvent should not be resampled because the second event came from a different device
    // than the previous event.
    assertMotionEventIsNotResampled(originalMotionEvent, motionFromSecondDevice);
}

// Increments of 16 ms for display refresh rate
// Increments of 6 ms for input frequency
// Resampling latency is known to be 5 ms
// Therefore, first resampling time will be 11 ms

/**
 * Timeline
 * ----+----------------------+---------+---------+---------+----------
 *     0ms                   10ms      11ms      15ms      16ms
 *    DOWN                   MOVE       |        MSG        |
 *                                  resample              frame
 * Resampling occurs at 11ms. It is possible to interpolate because there is a sample available
 * after the resample time. It is assumed that the InputMessage frequency is 100Hz, and the frame
 * frequency is 60Hz. This means the time between InputMessage samples is 10ms, and the time between
 * frames is ~16ms. Resample time is frameTime - RESAMPLE_LATENCY. The resampled sample must be the
 * last one in the batch to consume.
 */
TEST_F(ResamplerTest, SinglePointerSingleSampleInterpolation) {
    MotionEvent motionEvent =
            InputStream{{{10ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};
    const InputMessage futureSample =
            InputSample{15ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(11ms, motionEvent, &futureSample);

    assertMotionEventIsResampledAndCoordsNear(originalMotionEvent, motionEvent,
                                              Pointer{.id = 0,
                                                      .x = 1.2f,
                                                      .y = 1.2f,
                                                      .isResampled = true});
}

TEST_F(ResamplerTest, SinglePointerDeltaTooSmallInterpolation) {
    MotionEvent motionEvent =
            InputStream{{{10ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};
    const InputMessage futureSample =
            InputSample{11ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(10'500'000ns, motionEvent, &futureSample);

    assertMotionEventIsNotResampled(originalMotionEvent, motionEvent);
}

/**
 * Tests extrapolation given two MotionEvents with a single sample.
 */
TEST_F(ResamplerTest, SinglePointerSingleSampleExtrapolation) {
    MotionEvent previousMotionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    mResampler->resampleMotionEvent(10ms, previousMotionEvent, nullptr);

    MotionEvent motionEvent =
            InputStream{{{10ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(11ms, motionEvent, nullptr);

    assertMotionEventIsResampledAndCoordsNear(originalMotionEvent, motionEvent,
                                              Pointer{.id = 0,
                                                      .x = 1.0f,
                                                      .y = 1.0f,
                                                      .isResampled = true});
    // Integrity of the whole motionEvent
    // History size should increment by 1
    // Check if the resampled value is the last one
    // Check if the resampleTime is correct
    // Check if the PointerCoords are consistent with the other computations
}

TEST_F(ResamplerTest, SinglePointerMultipleSampleInterpolation) {
    MotionEvent motionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {10ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};
    const InputMessage futureSample =
            InputSample{15ms, {{.id = 0, .x = 3.0f, .y = 3.0f, .isResampled = false}}};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(11ms, motionEvent, &futureSample);

    assertMotionEventIsResampledAndCoordsNear(originalMotionEvent, motionEvent,
                                              Pointer{.id = 0,
                                                      .x = 2.2f,
                                                      .y = 2.2f,
                                                      .isResampled = true});
}

TEST_F(ResamplerTest, SinglePointerMultipleSampleExtrapolation) {
    MotionEvent motionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {10ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(11ms, motionEvent, nullptr);

    assertMotionEventIsResampledAndCoordsNear(originalMotionEvent, motionEvent,
                                              Pointer{.id = 0,
                                                      .x = 2.2f,
                                                      .y = 2.2f,
                                                      .isResampled = true});
}

TEST_F(ResamplerTest, SinglePointerDeltaTooSmallExtrapolation) {
    MotionEvent motionEvent =
            InputStream{{{9ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {10ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(11ms, motionEvent, nullptr);

    assertMotionEventIsNotResampled(originalMotionEvent, motionEvent);
}

TEST_F(ResamplerTest, SinglePointerDeltaTooLargeExtrapolation) {
    MotionEvent motionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {26ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(27ms, motionEvent, nullptr);

    assertMotionEventIsNotResampled(originalMotionEvent, motionEvent);
}

TEST_F(ResamplerTest, SinglePointerResampleTimeTooFarExtrapolation) {
    MotionEvent motionEvent =
            InputStream{{{5ms, {{.id = 0, .x = 1.0f, .y = 1.0f, .isResampled = false}}},
                         {25ms, {{.id = 0, .x = 2.0f, .y = 2.0f, .isResampled = false}}}},
                        AMOTION_EVENT_ACTION_MOVE};

    const MotionEvent originalMotionEvent = motionEvent;

    mResampler->resampleMotionEvent(43ms, motionEvent, nullptr);

    assertMotionEventIsResampledAndCoordsNear(originalMotionEvent, motionEvent,
                                              Pointer{.id = 0,
                                                      .x = 2.4f,
                                                      .y = 2.4f,
                                                      .isResampled = true});
}
} // namespace android
