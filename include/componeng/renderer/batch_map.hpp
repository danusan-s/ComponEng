#include "componeng/core/logger.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/renderer/resource_manager.hpp"
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

struct DrawKey {
  MeshID meshID;
  TextureID textureID;
  ShaderID shaderID;

  bool operator==(const DrawKey &other) const {
    return shaderID == other.shaderID && textureID == other.textureID &&
           meshID == other.meshID;
  }
};

struct DrawKeyHash {
  std::size_t operator()(const DrawKey &k) const {
    uint64_t h = (uint64_t)k.shaderID;
    h = h * 16777619u ^ k.textureID;
    h = h * 16777619u ^ k.meshID;
    return (size_t)h;
  }
};

struct InstanceData {
  Mat4 modelMatrix;
  Vec3 color;
};

struct BatchData {
  std::unique_ptr<IBuffer> instanceBuffer;
  std::vector<InstanceData> instanceDatas;
};

class BatchMap {

  std::unordered_map<DrawKey, BatchData, DrawKeyHash> map_;
  IRenderDevice &device;

public:
  BatchMap(IRenderDevice &device) : device(device) {
  }
  void add(const DrawKey &key, InstanceData value) {
    auto it = map_.find(key);

    if (it == map_.end()) {
      it = map_.emplace(key, BatchData{}).first;
      BatchData &batch = it->second;
      batch.instanceBuffer = device.createBuffer();
      batch.instanceBuffer->setData(nullptr,
                                    MAX_ENTITIES * sizeof(InstanceData));
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
