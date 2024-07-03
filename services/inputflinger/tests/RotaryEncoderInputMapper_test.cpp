/*
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

#include "RotaryEncoderInputMapper.h"

#include <list>
#include <string>
#include <tuple>
#include <variant>

#include <android-base/logging.h>
#include <gtest/gtest.h>
#include <input/DisplayViewport.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <utils/Timers.h>

#include "InputMapperTest.h"
#include "InputReaderBase.h"
#include "InterfaceMocks.h"
#include "NotifyArgs.h"
#include "TestEventMatchers.h"
#include "ui/Rotation.h"

#define TAG "RotaryEncoderInputMapper_test"

namespace android {

using testing::AllOf;
using testing::Return;
using testing::VariantWith;
constexpr ui::LogicalDisplayId DISPLAY_ID = ui::LogicalDisplayId::DEFAULT;
constexpr ui::LogicalDisplayId SECONDARY_DISPLAY_ID = ui::LogicalDisplayId{DISPLAY_ID.val() + 1};
constexpr int32_t DISPLAY_WIDTH = 480;
constexpr int32_t DISPLAY_HEIGHT = 800;

namespace {

DisplayViewport createViewport() {
    DisplayViewport v;
    v.orientation = ui::Rotation::Rotation0;
    v.logicalRight = DISPLAY_HEIGHT;
    v.logicalBottom = DISPLAY_WIDTH;
    v.physicalRight = DISPLAY_HEIGHT;
    v.physicalBottom = DISPLAY_WIDTH;
    v.deviceWidth = DISPLAY_HEIGHT;
    v.deviceHeight = DISPLAY_WIDTH;
    v.isActive = true;
    return v;
}

DisplayViewport createPrimaryViewport() {
    DisplayViewport v = createViewport();
    v.displayId = DISPLAY_ID;
    v.uniqueId = "local:1";
    return v;
}

DisplayViewport createSecondaryViewport() {
    DisplayViewport v = createViewport();
    v.displayId = SECONDARY_DISPLAY_ID;
    v.uniqueId = "local:2";
    v.type = ViewportType::EXTERNAL;
    return v;
}

/**
 * A fake InputDeviceContext that allows the associated viewport to be specified for the mapper.
 *
 * This is currently necessary because InputMapperUnitTest doesn't register the mappers it creates
 * with the InputDevice object, meaning that InputDevice::isIgnored becomes true, and the input
 * device doesn't set its associated viewport when it's configured.
 *
 * TODO(b/319217713): work out a way to avoid this fake.
 */
class ViewportFakingInputDeviceContext : public InputDeviceContext {
public:
    ViewportFakingInputDeviceContext(InputDevice& device, int32_t eventHubId,
                                     std::optional<DisplayViewport> viewport)
          : InputDeviceContext(device, eventHubId), mAssociatedViewport(viewport) {}

    ViewportFakingInputDeviceContext(InputDevice& device, int32_t eventHubId)
          : ViewportFakingInputDeviceContext(device, eventHubId, createPrimaryViewport()) {}

    std::optional<DisplayViewport> getAssociatedViewport() const override {
        return mAssociatedViewport;
    }

    void setViewport(const std::optional<DisplayViewport>& viewport) {
        mAssociatedViewport = viewport;
    }

private:
    std::optional<DisplayViewport> mAssociatedViewport;
};

} // namespace

/**
 * Unit tests for RotaryEncoderInputMapper.
 */
class RotaryEncoderInputMapperTest : public InputMapperUnitTest {
protected:
    void SetUp() override { SetUpWithBus(BUS_USB); }
    void SetUpWithBus(int bus) override {
        InputMapperUnitTest::SetUpWithBus(bus);

        EXPECT_CALL(mMockEventHub, hasRelativeAxis(EVENTHUB_ID, REL_WHEEL))
                .WillRepeatedly(Return(true));
        EXPECT_CALL(mMockEventHub, hasRelativeAxis(EVENTHUB_ID, REL_HWHEEL))
                .WillRepeatedly(Return(false));
    }
};

TEST_F(RotaryEncoderInputMapperTest, ConfigureDisplayIdWithAssociatedViewport) {
    DisplayViewport primaryViewport = createPrimaryViewport();
    DisplayViewport secondaryViewport = createSecondaryViewport();
    mReaderConfiguration.setDisplayViewports({primaryViewport, secondaryViewport});

    // Set up the secondary display as the associated viewport of the mapper.
    createDevice();
    ViewportFakingInputDeviceContext deviceContext(*mDevice, EVENTHUB_ID, secondaryViewport);
    mMapper = createInputMapper<RotaryEncoderInputMapper>(deviceContext, mReaderConfiguration);

    std::list<NotifyArgs> args;
    // Ensure input events are generated for the secondary display.
    args += process(ARBITRARY_TIME, EV_REL, REL_WHEEL, 1);
    args += process(ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    EXPECT_THAT(args,
                ElementsAre(VariantWith<NotifyMotionArgs>(
                        AllOf(WithMotionAction(AMOTION_EVENT_ACTION_SCROLL),
                              WithSource(AINPUT_SOURCE_ROTARY_ENCODER),
                              WithDisplayId(SECONDARY_DISPLAY_ID)))));
}

TEST_F(RotaryEncoderInputMapperTest, ConfigureDisplayIdNoAssociatedViewport) {
    // Set up the default display.
    mFakePolicy->clearViewports();
    mFakePolicy->addDisplayViewport(createPrimaryViewport());

    // Set up the mapper with no associated viewport.
    createDevice();
    mMapper = createInputMapper<RotaryEncoderInputMapper>(*mDeviceContext, mReaderConfiguration);

    // Ensure input events are generated without display ID
    std::list<NotifyArgs> args;
    args += process(ARBITRARY_TIME, EV_REL, REL_WHEEL, 1);
    args += process(ARBITRARY_TIME, EV_SYN, SYN_REPORT, 0);
    EXPECT_THAT(args,
                ElementsAre(VariantWith<NotifyMotionArgs>(
                        AllOf(WithMotionAction(AMOTION_EVENT_ACTION_SCROLL),
                              WithSource(AINPUT_SOURCE_ROTARY_ENCODER),
                              WithDisplayId(ui::LogicalDisplayId::INVALID)))));
}

} // namespace android