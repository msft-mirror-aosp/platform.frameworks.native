
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

#include <cutils/trace.h>
#include <stdint.h>

#ifndef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#endif

#define SFTRACE_ENABLED() ATRACE_ENABLED()
#define SFTRACE_BEGIN(name) ATRACE_BEGIN(name)
#define SFTRACE_END() ATRACE_END()
#define SFTRACE_ASYNC_BEGIN(name, cookie) ATRACE_ASYNC_BEGIN(name, cookie)
#define SFTRACE_ASYNC_END(name, cookie) ATRACE_ASYNC_END(name, cookie)
#define SFTRACE_ASYNC_FOR_TRACK_BEGIN(track_name, name, cookie) \
    ATRACE_ASYNC_FOR_TRACK_BEGIN(track_name, name, cookie)
#define SFTRACE_ASYNC_FOR_TRACK_END(track_name, cookie) \
    ATRACE_ASYNC_FOR_TRACK_END(track_name, cookie)
#define SFTRACE_INSTANT(name) ATRACE_INSTANT(name)
#define SFTRACE_INSTANT_FOR_TRACK(trackName, name) ATRACE_INSTANT_FOR_TRACK(trackName, name)
#define SFTRACE_INT(name, value) ATRACE_INT(name, value)
#define SFTRACE_INT64(name, value) ATRACE_INT64(name, value)

// SFTRACE_NAME traces from its location until the end of its enclosing scope.
#define _PASTE(x, y) x##y
#define PASTE(x, y) _PASTE(x, y)
#define SFTRACE_NAME(name) ::android::ScopedTrace PASTE(___tracer, __LINE__)(ATRACE_TAG, name)

// SFTRACE_CALL is an ATRACE_NAME that uses the current function name.
#define SFTRACE_CALL() SFTRACE_NAME(__FUNCTION__)

#define SFTRACE_FORMAT(fmt, ...)                                                \
    TraceUtils::TraceEnder traceEnder =                                         \
            (CC_UNLIKELY(ATRACE_ENABLED()) &&                                   \
                     (TraceUtils::atraceFormatBegin(fmt, ##__VA_ARGS__), true), \
             TraceUtils::TraceEnder())

#define SFTRACE_FORMAT_INSTANT(fmt, ...) \
    (CC_UNLIKELY(ATRACE_ENABLED()) && (TraceUtils::instantFormat(fmt, ##__VA_ARGS__), true))

#define ALOGE_AND_TRACE(fmt, ...)                   \
    do {                                            \
        ALOGE(fmt, ##__VA_ARGS__);                  \
        SFTRACE_FORMAT_INSTANT(fmt, ##__VA_ARGS__); \
    } while (false)

namespace android {

class TraceUtils {
public:
    class TraceEnder {
    public:
        ~TraceEnder() { ATRACE_END(); }
    };

    static void atraceFormatBegin(const char* fmt, ...) {
        const int BUFFER_SIZE = 256;
        va_list ap;
        char buf[BUFFER_SIZE];

        va_start(ap, fmt);
        vsnprintf(buf, BUFFER_SIZE, fmt, ap);
        va_end(ap);

        SFTRACE_BEGIN(buf);
    }

    static void instantFormat(const char* fmt, ...) {
        const int BUFFER_SIZE = 256;
        va_list ap;
        char buf[BUFFER_SIZE];

        va_start(ap, fmt);
        vsnprintf(buf, BUFFER_SIZE, fmt, ap);
        va_end(ap);

        SFTRACE_INSTANT(buf);
    }
};

class ScopedTrace {
public:
    inline ScopedTrace(uint64_t tag, const char* name) : mTag(tag) { atrace_begin(mTag, name); }

    inline ~ScopedTrace() { atrace_end(mTag); }

private:
    uint64_t mTag;
};

} // namespace android
