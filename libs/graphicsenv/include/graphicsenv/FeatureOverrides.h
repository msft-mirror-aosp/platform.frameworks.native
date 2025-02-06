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

#include <map>
#include <string>
#include <vector>

namespace android {

class FeatureConfig {
public:
    FeatureConfig() = default;
    FeatureConfig(const FeatureConfig&) = default;
    virtual ~FeatureConfig() = default;
    std::string toString() const;

    std::string mFeatureName;
    bool mEnabled;
};

/*
 * Class for transporting OpenGL ES Feature configurations from GpuService to authorized
 * recipients.
 */
class FeatureOverrides {
public:
    FeatureOverrides() = default;
    FeatureOverrides(const FeatureOverrides&) = default;
    virtual ~FeatureOverrides() = default;
    std::string toString() const;

    std::vector<FeatureConfig> mGlobalFeatures;
    /* Key: Package Name, Value: Package's Feature Configs */
    std::map<std::string, std::vector<FeatureConfig>> mPackageFeatures;
};

} // namespace android
