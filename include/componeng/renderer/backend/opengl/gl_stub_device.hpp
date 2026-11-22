#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"

namespace componeng::renderer::opengl {

/**
 * Stands in for the OpenGL backend while it is ported to the rhi API (phase 4
 * of dev_docs/upgrade_opengl.md). It builds, and refuses to start with a
 * message that says so; nothing else on it is ever called.
 */
class GLStubDevice final : public rhi::IDevice {
public:
  void init(void *windowHandle) override;

  rhi::Backend backend() const override {
    return rhi::Backend::OpenGL;
  }
  rhi::Format backbufferFormat() const override {
    return rhi::Format::RGBA8;
  }
  bool frameOpen() const override {
    return false;
  }
  bool beginFrame() override {
    return false;
  }
  void present(void *) override {
  }
  void setWindowViewport(int, int, int, int) override {
  }
  rhi::Extent2D windowViewportExtent() const override {
    return {};
  }

  std::unique_ptr<rhi::IBuffer> createBuffer(const rhi::BufferDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IImage> createImage(const rhi::ImageDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::ISampler>
  createSampler(const rhi::SamplerDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IShaderModule>
  createShaderModule(const rhi::ShaderModuleDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IBindGroupLayout>
  createBindGroupLayout(const rhi::BindGroupLayoutDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IBindGroup>
  createBindGroup(const rhi::BindGroupDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IGraphicsPipeline>
  createGraphicsPipeline(const rhi::GraphicsPipelineDesc &) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IComputePipeline>
  createComputePipeline(const rhi::ComputePipelineDesc &) override {
    return nullptr;
  }

  void uploadBuffer(rhi::IBuffer &, uint64_t, const void *, uint64_t) override {
  }
  void uploadImage(rhi::IImage &, const rhi::Rect &, const void *) override {
  }

  void beginRendering(const rhi::RenderingDesc &) override {
  }
  void endRendering() override {
  }
  void setViewport(const rhi::Rect &) override {
  }
  void setScissor(const rhi::Rect &) override {
  }
  void bindPipeline(rhi::IGraphicsPipeline &) override {
  }
  void bindGroup(uint32_t, rhi::IBindGroup &) override {
  }
  void bindVertexBuffer(uint32_t, rhi::IBuffer &, uint64_t) override {
  }
  void bindIndexBuffer(rhi::IBuffer &, uint64_t, rhi::IndexType) override {
  }
  void draw(uint32_t, uint32_t, uint32_t, uint32_t) override {
  }
  void drawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {
  }
  void drawIndexedIndirect(rhi::IBuffer &, uint64_t, uint32_t,
                           uint32_t) override {
  }
  void bindComputePipeline(rhi::IComputePipeline &) override {
  }
  void dispatch(uint32_t, uint32_t, uint32_t) override {
  }
  void memoryBarrier(rhi::Access, rhi::Access) override {
  }
  void copyBuffer(rhi::IBuffer &, rhi::IBuffer &, uint64_t, uint64_t,
                  uint64_t) override {
  }
  int checkErrors() const override {
    return 0;
  }
};

} // namespace componeng::renderer::opengl
