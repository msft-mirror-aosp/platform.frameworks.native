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

#define LOG_TAG "TestInputChannel"
#define ATRACE_TAG ATRACE_TAG_INPUT

#include <TestInputChannel.h>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <binder/IBinder.h>
#include <utils/StrongPointer.h>

namespace android {

namespace {
constexpr int FAKE_FD{-1};
} // namespace

// --- TestInputChannel ---

TestInputChannel::TestInputChannel(const std::string& name)
      : InputChannel{name, base::unique_fd(FAKE_FD), sp<BBinder>::make()} {}

void TestInputChannel::enqueueMessage(const InputMessage& message) {
    mReceivedMessages.push(message);
}

status_t TestInputChannel::sendMessage(const InputMessage* message) {
    LOG_IF(FATAL, message == nullptr)
            << "TestInputChannel " << getName() << ". No message was passed to sendMessage.";

    mSentMessages.push(*message);
    return OK;
}

base::Result<InputMessage> TestInputChannel::receiveMessage() {
    if (mReceivedMessages.empty()) {
        return base::Error(WOULD_BLOCK);
    }
    InputMessage message = mReceivedMessages.front();
    mReceivedMessages.pop();
    return message;
}

bool TestInputChannel::probablyHasInput() const {
    return !mReceivedMessages.empty();
}

void TestInputChannel::assertFinishMessage(uint32_t seq, bool handled) {
    ASSERT_FALSE(mSentMessages.empty())
            << "TestInputChannel " << getName() << ". Cannot assert. mSentMessages is empty.";

    const InputMessage& finishMessage = mSentMessages.front();

    EXPECT_EQ(finishMessage.header.seq, seq)
            << "TestInputChannel " << getName()
            << ". Sequence mismatch. Message seq: " << finishMessage.header.seq
            << " Expected seq: " << seq;

    EXPECT_EQ(finishMessage.body.finished.handled, handled)
            << "TestInputChannel " << getName()
            << ". Handled value mismatch. Message val: " << std::boolalpha
            << finishMessage.body.finished.handled << "Expected val: " << handled
            << std::noboolalpha;
    mSentMessages.pop();
}

void TestInputChannel::assertNoSentMessages() const {
    ASSERT_TRUE(mSentMessages.empty());
}
} // namespace android