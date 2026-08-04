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

class BatchMap {
  std::unordered_map<DrawKey, std::vector<ecs::EntityID>, DrawKeyHash> map_;

public:
  BatchMap() = default;

  void add(const DrawKey &key, ecs::EntityID value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      it->second.push_back(value);
    } else {
      map_[key] = {value};
    }
  }

  void add(core::HandleID meshID, core::HandleID materialID,
           ecs::EntityID value) {
    DrawKey key{meshID, materialID};
    add(key, std::move(value));
  }

  void clear() {
    for (auto &pair : map_) {
      pair.second.clear();
    }
  }

  auto &getMap() {
    return map_;
  }
};

} // namespace componeng::renderer
