#pragma once

#include "componeng/renderer/batching/batch_map.hpp"

namespace componeng::renderer {

struct RenderBatch {
  DrawKey key;
  BatchData data;
};

struct RenderQueue {
  std::unordered_map<DrawKey, BatchData, DrawKeyHash> batches;
};

} // namespace componeng::renderer
