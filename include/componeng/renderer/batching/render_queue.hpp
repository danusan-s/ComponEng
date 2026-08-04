#pragma once

#include "componeng/renderer/batching/batch_map.hpp"

namespace componeng::renderer {

using InstanceData = std::vector<float>;

struct RenderBatch {
  core::HandleID meshID;
  core::HandleID materialID;
  api::VertexLayout vertexLayout;
  std::vector<InstanceData> instanceDatas;
};

struct RenderQueue {
private:
  std::vector<RenderBatch> batches;

public:
  const std::vector<RenderBatch> &getBatches() const {
    return batches;
  }

  void addBatch(const RenderBatch &&batch) {
    batches.push_back(std::move(batch));
  }

  void clear() {
    for (auto &batch : batches) {
      batch.instanceDatas.clear();
    }
    batches.clear();
  }
};

} // namespace componeng::renderer
