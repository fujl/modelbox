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

#include "modelbox/device/rockchip/device_rockchip.h"

#include <stdio.h>

#include "modelbox/base/log.h"
#include "modelbox/base/os.h"
#include "modelbox/device/rockchip/rknpu_memory.h"
// -- only linux: get free memory
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <sys/sysinfo.h>

#include "rknn_api.h"

const std::string LIB_RKNN_API_PATH = "/usr/local/rk_drivers/librknn_api.so";
const std::string RKNN_PROXY_PATH = "/usr/rk_bins/npu_transfer_proxy";

namespace modelbox {
RockChip::RockChip(const std::shared_ptr<DeviceMemoryManager> &mem_mgr)
    : Device(mem_mgr) {}

RockChip::~RockChip() {}

std::string RockChip::GetType() const { return DEVICE_TYPE; }

Status RockChip::DeviceExecute(const DevExecuteCallBack &rkfun,
                               int32_t priority, size_t rkcount) {
  if (0 == rkcount) {
    return STATUS_OK;
  }

  for (size_t i = 0; i < rkcount; ++i) {
    auto status = rkfun(i);
    if (!status) {
      MBLOG_WARN << "executor rkfunc failed: " << status
                 << " stack trace:" << GetStackTrace();
      return status;
    }
  }

  return STATUS_OK;
};

bool RockChip::NeedResourceNice() { return true; }

RKNPUFactory::RKNPUFactory() {}
RKNPUFactory::~RKNPUFactory() {}

std::map<std::string, std::shared_ptr<DeviceDesc>>
RKNPUFactory::ProbeRKNNDevice() {
  std::map<std::string, std::shared_ptr<DeviceDesc>> device_desc_map;

  void *handler = dlopen(LIB_RKNN_API_PATH.c_str(), RTLD_LAZY | RTLD_LOCAL);
  if (handler == nullptr) {
    MBLOG_ERROR << "dlopen " << LIB_RKNN_API_PATH << " failed.";
    return device_desc_map;
  }

  std::shared_ptr<rknn_devices_id> dev_ids =
      std::make_shared<rknn_devices_id>();
  typedef int (*find_device_func)(rknn_devices_id *);
  auto find_device =
      reinterpret_cast<find_device_func>(dlsym(handler, "rknn_find_devices"));

  if (find_device(dev_ids.get()) != RKNN_SUCC ||
      dev_ids.get()->n_devices == 0) {
    MBLOG_ERROR << "find none rknn device";
    dlclose(handler);
    return device_desc_map;
  }

  std::vector<std::string> rknn_devs;
  for (size_t i = 0; i < dev_ids.get()->n_devices; i++) {
    rknn_devs.push_back(std::string(dev_ids.get()->ids[i]));
  }
  dlclose(handler);

  struct sysinfo s_info;
  sysinfo(&s_info);
  for (size_t i = 0; i < rknn_devs.size(); i++) {
    auto device_desc = std::make_shared<RKNPUDesc>();
    device_desc->SetDeviceDesc("This is a rknpu device description.");
    // inference module will bind all rknpu device to one
    auto id_str = std::to_string(i);
    device_desc->SetDeviceId(id_str);
    device_desc->SetDeviceMemory(GetBytesReadable(s_info.totalram));
    device_desc->SetDeviceType(DEVICE_TYPE);
    device_desc_map.insert(std::make_pair(id_str, device_desc));
  }

  RKNNDevs::Instance().SetNames(rknn_devs);

  return device_desc_map;
}

std::map<std::string, std::shared_ptr<DeviceDesc>> RKNPUFactory::DeviceProbe() {
  if (access(RKNN_PROXY_PATH.c_str(), F_OK) == 0) {
    MBLOG_INFO << "use rknpu for inference.";
    return ProbeRKNNDevice();
  }

  MBLOG_INFO << "use rknpu2 for inference.";
  std::map<std::string, std::shared_ptr<DeviceDesc>> device_desc_map;
  struct sysinfo s_info;
  sysinfo(&s_info);

  auto device_desc = std::make_shared<RKNPUDesc>();
  device_desc->SetDeviceDesc("This is a rknpu2 device description.");
  auto id_str = std::to_string(0);
  device_desc->SetDeviceId(id_str);
  device_desc->SetDeviceMemory(GetBytesReadable(s_info.totalram));
  device_desc->SetDeviceType(DEVICE_TYPE);
  device_desc_map.insert(std::make_pair(id_str, device_desc));
  return device_desc_map;
}

std::string RKNPUFactory::GetDeviceFactoryType() { return DEVICE_TYPE; }

std::shared_ptr<Device> RKNPUFactory::CreateDevice(
    const std::string &device_id) {
  auto mem_mgr = std::make_shared<RKNPUMemoryManager>(device_id);
  auto status = mem_mgr->Init();
  if (!status) {
    StatusError = status;
    return nullptr;
  }

  return std::make_shared<RockChip>(mem_mgr);
}

void RKNNDevs::SetNames(std::vector<std::string> &dev_names) {
  dev_names_.swap(dev_names);
}

const std::vector<std::string> &RKNNDevs::GetNames() { return dev_names_; }

}  // namespace modelbox
