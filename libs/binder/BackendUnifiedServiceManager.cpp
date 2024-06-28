/*
 * Copyright (C) 2024 The Android Open Source Project
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
#include "BackendUnifiedServiceManager.h"

namespace android {

using AidlServiceManager = android::os::IServiceManager;

BackendUnifiedServiceManager::BackendUnifiedServiceManager(const sp<AidlServiceManager>& impl)
      : mTheRealServiceManager(impl) {}

sp<AidlServiceManager> BackendUnifiedServiceManager::getImpl() {
    return mTheRealServiceManager;
}
binder::Status BackendUnifiedServiceManager::getService(const ::std::string& name,
                                                        sp<IBinder>* _aidl_return) {
    return mTheRealServiceManager->getService(name, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::checkService(const ::std::string& name,
                                                          sp<IBinder>* _aidl_return) {
    return mTheRealServiceManager->checkService(name, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::addService(const ::std::string& name,
                                                        const sp<IBinder>& service,
                                                        bool allowIsolated, int32_t dumpPriority) {
    return mTheRealServiceManager->addService(name, service, allowIsolated, dumpPriority);
}
binder::Status BackendUnifiedServiceManager::listServices(
        int32_t dumpPriority, ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->listServices(dumpPriority, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::registerForNotifications(
        const ::std::string& name, const sp<os::IServiceCallback>& callback) {
    return mTheRealServiceManager->registerForNotifications(name, callback);
}
binder::Status BackendUnifiedServiceManager::unregisterForNotifications(
        const ::std::string& name, const sp<os::IServiceCallback>& callback) {
    return mTheRealServiceManager->unregisterForNotifications(name, callback);
}
binder::Status BackendUnifiedServiceManager::isDeclared(const ::std::string& name,
                                                        bool* _aidl_return) {
    return mTheRealServiceManager->isDeclared(name, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::getDeclaredInstances(
        const ::std::string& iface, ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->getDeclaredInstances(iface, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::updatableViaApex(
        const ::std::string& name, ::std::optional<::std::string>* _aidl_return) {
    return mTheRealServiceManager->updatableViaApex(name, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::getUpdatableNames(
        const ::std::string& apexName, ::std::vector<::std::string>* _aidl_return) {
    return mTheRealServiceManager->getUpdatableNames(apexName, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::getConnectionInfo(
        const ::std::string& name, ::std::optional<os::ConnectionInfo>* _aidl_return) {
    return mTheRealServiceManager->getConnectionInfo(name, _aidl_return);
}
binder::Status BackendUnifiedServiceManager::registerClientCallback(
        const ::std::string& name, const sp<IBinder>& service,
        const sp<os::IClientCallback>& callback) {
    return mTheRealServiceManager->registerClientCallback(name, service, callback);
}
binder::Status BackendUnifiedServiceManager::tryUnregisterService(const ::std::string& name,
                                                                  const sp<IBinder>& service) {
    return mTheRealServiceManager->tryUnregisterService(name, service);
}
binder::Status BackendUnifiedServiceManager::getServiceDebugInfo(
        ::std::vector<os::ServiceDebugInfo>* _aidl_return) {
    return mTheRealServiceManager->getServiceDebugInfo(_aidl_return);
}

[[clang::no_destroy]] static std::once_flag gUSmOnce;
[[clang::no_destroy]] static sp<BackendUnifiedServiceManager> gUnifiedServiceManager;

sp<BackendUnifiedServiceManager> getBackendUnifiedServiceManager() {
    std::call_once(gUSmOnce, []() {
#if defined(__BIONIC__) && !defined(__ANDROID_VNDK__)
        /* wait for service manager */ {
            using std::literals::chrono_literals::operator""s;
            using android::base::WaitForProperty;
            while (!WaitForProperty("servicemanager.ready", "true", 1s)) {
                ALOGE("Waited for servicemanager.ready for a second, waiting another...");
            }
        }
#endif

        sp<AidlServiceManager> sm = nullptr;
        while (sm == nullptr) {
            sm = interface_cast<AidlServiceManager>(
                    ProcessState::self()->getContextObject(nullptr));
            if (sm == nullptr) {
                ALOGE("Waiting 1s on context object on %s.",
                      ProcessState::self()->getDriverName().c_str());
                sleep(1);
            }
        }

        gUnifiedServiceManager = sp<BackendUnifiedServiceManager>::make(sm);
    });

    return gUnifiedServiceManager;
}

} // namespace android