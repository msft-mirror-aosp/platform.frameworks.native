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

#pragma once

#include <com_android_graphics_libgui_flags.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/Surface.h>

#define WB_CAMERA3_AND_PROCESSORS_WITH_DEPENDENCIES                  \
    (COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_CAMERA3_AND_PROCESSORS) && \
     COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_CONSUMER_BASE_OWNS_BQ) &&  \
     COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_PLATFORM_API_IMPROVEMENTS))

#define WB_LIBCAMERASERVICE_WITH_DEPENDENCIES       \
    (WB_CAMERA3_AND_PROCESSORS_WITH_DEPENDENCIES && \
     COM_ANDROID_GRAPHICS_LIBGUI_FLAGS(WB_LIBCAMERASERVICE))

#if WB_LIBCAMERASERVICE_WITH_DEPENDENCIES
typedef android::Surface SurfaceType;
#else
typedef android::IGraphicBufferProducer SurfaceType;
#endif