/*
 * Copyright 2021 The Modelbox Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MODELBOX_DEVICE_ROCKCHIP_H_
#define MODELBOX_DEVICE_ROCKCHIP_H_

#include <modelbox/base/device.h>
#include <modelbox/data_context.h>
#include <modelbox/device/rockchip/rknpu_memory.h>
#include <modelbox/flow.h>

namespace modelbox {
typedef void MppBufHdl;

constexpr const char *DEVICE_TYPE = "rockchip";
constexpr const char *DEVICE_DRIVER_NAME = "device-rockchip";
constexpr const char *DEVICE_DRIVER_DESCRIPTION = "A rockchip device driver";

class RockChip : public Device {
 public:
  RockChip(const std::shared_ptr<DeviceMemoryManager> &mem_mgr);
  virtual ~RockChip();
  std::string GetType() const override;

  Status DeviceExecute(const DevExecuteCallBack &rkfun, int32_t priority,
                       size_t rkcount) override;
  bool NeedResourceNice() override;
};

class RKNPUFactory : public DeviceFactory {
 public:
  RKNPUFactory();
  virtual ~RKNPUFactory();

  std::map<std::string, std::shared_ptr<DeviceDesc>> DeviceProbe();
  std::string GetDeviceFactoryType();
  std::shared_ptr<Device> CreateDevice(const std::string &device_id);

 private:
  std::map<std::string, std::shared_ptr<DeviceDesc>> ProbeRKNNDevice();
};

class RKNPUDesc : public DeviceDesc {
 public:
  RKNPUDesc() = default;
  virtual ~RKNPUDesc() = default;
};

// use it to store the rknn device names
class RKNNDevs {
 public:
  static RKNNDevs &Instance() {
    static RKNNDevs rk_nndevs;
    return rk_nndevs;
  }

  void SetNames(std::vector<std::string> &dev_names);
  const std::vector<std::string> &GetNames();

 private:
  RKNNDevs() = default;
  virtual ~RKNNDevs() = default;
  RKNNDevs(const RKNNDevs &) = delete;
  RKNNDevs &operator=(const RKNNDevs &) = delete;

  std::vector<std::string> dev_names_;
};

}  // namespace modelbox

#endif  // MODELBOX_DEVICE_ROCKCHIP_H_