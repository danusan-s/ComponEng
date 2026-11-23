#include "componeng/renderer/draw/pipeline_cache.hpp"

namespace componeng::renderer {

namespace {

template <typename T> void add(std::string &key, T v) {
  key.append(reinterpret_cast<const char *>(&v), sizeof(v));
}

rhi::Format attributeFormat(uint32_t components) {
  switch (components) {
    case 1:
      return rhi::Format::R32F;
    case 2:
      return rhi::Format::RG32F;
    case 3:
      return rhi::Format::RGB32F;
    default:
      return rhi::Format::RGBA32F;
  }
}

std::string keyOf(const PipelineRequest &r) {
  std::string key;
  add(key, r.vertex);
  add(key, r.fragment);
  add(key, r.frameLayout);
  add(key, r.materialLayout);
  add(key, static_cast<uint8_t>(r.state.cull));
  add(key, r.state.depthTest);
  add(key, r.state.depthWrite);
  add(key, static_cast<uint8_t>(r.state.depthFunc));
  add(key, static_cast<uint8_t>(r.state.blend));
  add(key, r.state.colorWrite);
  add(key, static_cast<uint8_t>(r.colorFormat));
  add(key, static_cast<uint8_t>(r.depthFormat));
  add(key, r.meshVertices);
  if (r.instanceLayout) {
    add(key, r.instanceLayout->stride);
    for (const api::VertexAttribute &a : r.instanceLayout->attributes) {
      add(key, a.location);
      add(key, a.offset);
      add(key, a.componentCount);
    }
  }
  return key;
}

} // namespace

rhi::IGraphicsPipeline *PipelineCache::get(rhi::IDevice &device,
                                           const PipelineRequest &request) {
  const std::string key = keyOf(request);
  auto found = m_pipelines.find(key);
  if (found != m_pipelines.end()) {
    return found->second.get();
  }

  rhi::GraphicsPipelineDesc d;
  d.vertex = request.vertex;
  d.fragment = request.fragment;
  d.state = request.state;
  d.colorFormat = request.colorFormat;
  d.depthFormat = request.depthFormat;
  d.setLayouts = {request.frameLayout, request.materialLayout};
  if (request.meshVertices) {
    d.vertexInput.bindings.push_back({0, 32, rhi::VertexInputRate::PerVertex});
    d.vertexInput.attributes = {{0, 0, rhi::Format::RGB32F, 0},
                                {1, 0, rhi::Format::RGB32F, 12},
                                {2, 0, rhi::Format::RG32F, 24}};
  }
  if (request.instanceLayout && !request.instanceLayout->attributes.empty()) {
    d.vertexInput.bindings.push_back(
        {1, request.instanceLayout->stride, rhi::VertexInputRate::PerInstance});
    for (const api::VertexAttribute &a : request.instanceLayout->attributes) {
      d.vertexInput.attributes.push_back({static_cast<uint32_t>(a.location), 1,
                                          attributeFormat(a.componentCount),
                                          a.offset});
    }
  }
  auto pipeline = device.createGraphicsPipeline(d);
  rhi::IGraphicsPipeline *out = pipeline.get();
  m_pipelines.emplace(key, std::move(pipeline));
  return out;
}

} // namespace componeng::renderer
