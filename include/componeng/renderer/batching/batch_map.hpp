#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace componeng::renderer {

struct DrawKey {
  core::HandleID meshID;
  core::HandleID materialID;

  bool operator==(const DrawKey &other) const {
    return materialID == other.materialID && meshID == other.meshID;
  }
};

// hash combine algo so it works on both 32 and 64 bit platforms
struct DrawKeyHash {
  std::size_t operator()(const DrawKey &k) const {
    std::size_t h = std::hash<uint32_t>{}(k.meshID);
    h ^= std::hash<uint32_t>{}(k.materialID) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

using InstanceData = api::InstanceData;

struct BatchData {
  std::unique_ptr<api::IBuffer> instanceBuffer;
  std::vector<InstanceData> instanceDatas;
};

class BatchMap {

  std::unordered_map<DrawKey, BatchData, DrawKeyHash> map_;
  api::IRenderDevice &device;

public:
  BatchMap(api::IRenderDevice &device) : device(device) {
  }
  void add(const DrawKey &key, InstanceData &&value) {
    auto it = map_.find(key);

    if (it == map_.end()) {
      it = map_.emplace(key, BatchData{}).first;
      BatchData &batch = it->second;
      batch.instanceBuffer = device.createBuffer();
      batch.instanceBuffer->setData(nullptr,
                                    ecs::MAX_ENTITIES * sizeof(InstanceData));
      device.setupInstanceAttributes(*batch.instanceBuffer);
    };

    it->second.instanceDatas.push_back(std::move(value));
  }

  void clear() {
    for (auto &pair : map_) {
      pair.second.instanceDatas.clear();
    }
  }

  auto &getMap() const {
    return map_;
  }
};

} // namespace componeng::renderer
