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

#include <input/InputConsumerNoResampling.h>

#include <memory>
#include <optional>
#include <utility>

#include <TestInputChannel.h>
#include <TestLooper.h>
#include <android-base/logging.h>
#include <gtest/gtest.h>
#include <input/BlockingQueue.h>
#include <input/InputEventBuilders.h>
#include <utils/StrongPointer.h>

namespace android {

class InputConsumerTest : public testing::Test, public InputConsumerCallbacks {
protected:
    InputConsumerTest()
          : mClientTestChannel{std::make_shared<TestInputChannel>("TestChannel")},
            mTestLooper{std::make_shared<TestLooper>()} {
        Looper::setForThread(mTestLooper->getLooper());
        mConsumer = std::make_unique<InputConsumerNoResampling>(mClientTestChannel, mTestLooper,
                                                                *this, /*resampler=*/nullptr);
    }

    void assertOnBatchedInputEventPendingWasCalled();

    std::shared_ptr<TestInputChannel> mClientTestChannel;
    std::shared_ptr<TestLooper> mTestLooper;
    std::unique_ptr<InputConsumerNoResampling> mConsumer;

    BlockingQueue<std::unique_ptr<KeyEvent>> mKeyEvents;
    BlockingQueue<std::unique_ptr<MotionEvent>> mMotionEvents;
    BlockingQueue<std::unique_ptr<FocusEvent>> mFocusEvents;
    BlockingQueue<std::unique_ptr<CaptureEvent>> mCaptureEvents;
    BlockingQueue<std::unique_ptr<DragEvent>> mDragEvents;
    BlockingQueue<std::unique_ptr<TouchModeEvent>> mTouchModeEvents;

private:
    size_t onBatchedInputEventPendingInvocationCount{0};

    // InputConsumerCallbacks interface
    void onKeyEvent(std::unique_ptr<KeyEvent> event, uint32_t seq) override {
        mKeyEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    }
    void onMotionEvent(std::unique_ptr<MotionEvent> event, uint32_t seq) override {
        mMotionEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    }
    void onBatchedInputEventPending(int32_t pendingBatchSource) override {
        if (!mConsumer->probablyHasInput()) {
            ADD_FAILURE() << "should deterministically have input because there is a batch";
        }
        ++onBatchedInputEventPendingInvocationCount;
    };
    void onFocusEvent(std::unique_ptr<FocusEvent> event, uint32_t seq) override {
        mFocusEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    };
    void onCaptureEvent(std::unique_ptr<CaptureEvent> event, uint32_t seq) override {
        mCaptureEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    };
    void onDragEvent(std::unique_ptr<DragEvent> event, uint32_t seq) override {
        mDragEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    }
    void onTouchModeEvent(std::unique_ptr<TouchModeEvent> event, uint32_t seq) override {
        mTouchModeEvents.push(std::move(event));
        mConsumer->finishInputEvent(seq, true);
    };
};

void InputConsumerTest::assertOnBatchedInputEventPendingWasCalled() {
    ASSERT_GT(onBatchedInputEventPendingInvocationCount, 0UL)
            << "onBatchedInputEventPending has not been called.";
    --onBatchedInputEventPendingInvocationCount;
}

TEST_F(InputConsumerTest, MessageStreamBatchedInMotionEvent) {
    mClientTestChannel->enqueueMessage(
            InputMessageBuilder{InputMessage::Type::MOTION, /*seq=*/0}.build());
    mClientTestChannel->enqueueMessage(
            InputMessageBuilder{InputMessage::Type::MOTION, /*seq=*/1}.build());
    mClientTestChannel->enqueueMessage(
            InputMessageBuilder{InputMessage::Type::MOTION, /*seq=*/2}.build());

    mClientTestChannel->assertNoSentMessages();

    mTestLooper->invokeCallback(mClientTestChannel->getFd(), ALOOPER_EVENT_INPUT);

    assertOnBatchedInputEventPendingWasCalled();

    mConsumer->consumeBatchedInputEvents(std::nullopt);

    std::unique_ptr<MotionEvent> batchedMotionEvent = mMotionEvents.pop();
    ASSERT_NE(batchedMotionEvent, nullptr);

    mClientTestChannel->assertFinishMessage(/*seq=*/0, /*handled=*/true);
    mClientTestChannel->assertFinishMessage(/*seq=*/1, /*handled=*/true);
    mClientTestChannel->assertFinishMessage(/*seq=*/2, /*handled=*/true);

    EXPECT_EQ(batchedMotionEvent->getHistorySize() + 1, 3UL);
}
} // namespace android
