#pragma once

#include "componeng/renderer/backend/api/rhi/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace componeng::renderer::rhi {

/**
 * Destroying any object in this file is always safe: the backend retires it
 * after the frames that might still use it. Callers never wait for the GPU.
 */

struct BufferDesc {
  uint64_t size = 0;
  BufferUsage usage = BufferUsage::None;
  MemoryKind memory = MemoryKind::Upload;
  const char *debugName = nullptr;
};

class IBuffer {
public:
  virtual ~IBuffer() = default;

  virtual uint64_t size() const = 0;

  /** Upload memory only. Rewriting every frame must not stall the CPU. */
  virtual void write(uint64_t offset, const void *data, uint64_t bytes) = 0;
};

enum class ImageFlags : uint8_t {
  None = 0,
  // Contents are undefined at the start of every frame and not read by a later
  // one (a scene colour or depth target). The backend may keep one copy per
  // frame in flight; callers never see which.
  PerFrame = 1,
};

struct ImageDesc {
  Extent2D extent;
  Format format = Format::RGBA8;
  ImageUsage usage = ImageUsage::Sampled;
  uint32_t mipLevels = 1; // 0: the full chain
  ImageFlags flags = ImageFlags::None;
  const char *debugName = nullptr;
};

class IImage {
public:
  virtual ~IImage() = default;

  virtual Extent2D extent() const = 0;
  virtual Format format() const = 0;
  virtual uint32_t mipLevels() const = 0;
};

struct SamplerDesc {
  Filter minFilter = Filter::Linear;
  Filter magFilter = Filter::Linear;
  Filter mipFilter = Filter::Linear;
  WrapMode wrapU = WrapMode::ClampToEdge;
  WrapMode wrapV = WrapMode::ClampToEdge;
  // 1 is off; clamped to what the device supports.
  float anisotropy = 1.0f;
};

class ISampler {
public:
  virtual ~ISampler() = default;
};

/** SPIR-V words as produced by the shader compile module. */
struct ShaderModuleDesc {
  ShaderStage stage = ShaderStage::Vertex;
  const uint32_t *code = nullptr;
  size_t wordCount = 0;
  const char *debugName = nullptr;
};

class IShaderModule {
public:
  virtual ~IShaderModule() = default;
};

enum class BindingType : uint8_t {
  UniformBuffer,
  StorageBuffer,
  CombinedImageSampler,
  StorageImage,
};

struct BindingDesc {
  // Logical binding inside its set, 0 to 7.
  uint32_t binding = 0;
  BindingType type = BindingType::UniformBuffer;
  StageFlags stages = StageFlags::AllGraphics;
};

struct BindGroupLayoutDesc {
  std::vector<BindingDesc> bindings;
};

class IBindGroupLayout {
public:
  virtual ~IBindGroupLayout() = default;
};

struct BindGroupEntry {
  uint32_t binding = 0;
  // A buffer entry sets `buffer`; an image entry sets `image` and `sampler`.
  IBuffer *buffer = nullptr;
  uint64_t offset = 0;
  uint64_t range = 0; // 0: from offset to the end
  IImage *image = nullptr;
  ISampler *sampler = nullptr;
};

struct BindGroupDesc {
  IBindGroupLayout *layout = nullptr;
  std::vector<BindGroupEntry> entries;
};

/** Immutable once created. To change what is bound, create a new group. The
 *  contents of a bound buffer may change freely. */
class IBindGroup {
public:
  virtual ~IBindGroup() = default;
};

struct VertexBindingDesc {
  uint32_t binding = 0;
  uint32_t stride = 0;
  VertexInputRate rate = VertexInputRate::PerVertex;
};

struct VertexAttributeDesc {
  uint32_t location = 0;
  uint32_t binding = 0;
  Format format = Format::RGB32F;
  uint32_t offset = 0;
};

struct VertexInputDesc {
  std::vector<VertexBindingDesc> bindings;
  std::vector<VertexAttributeDesc> attributes;
};

struct SpecConstant {
  uint32_t id = 0;
  uint32_t value = 0;
};

struct GraphicsPipelineDesc {
  IShaderModule *vertex = nullptr;
  // Never null: a depth-only draw uses an empty fragment module.
  IShaderModule *fragment = nullptr;
  // Empty for a draw with no vertex buffers (a full-screen triangle).
  VertexInputDesc vertexInput;
  RenderState state;
  Format colorFormat = Format::RGBA8;
  // Undefined: no depth attachment.
  Format depthFormat = Format::D32F;
  // Index is the set number.
  std::vector<IBindGroupLayout *> setLayouts;
  std::vector<SpecConstant> specConstants;
  const char *debugName = nullptr;
};

class IGraphicsPipeline {
public:
  virtual ~IGraphicsPipeline() = default;
};

struct ComputePipelineDesc {
  IShaderModule *compute = nullptr;
  std::vector<IBindGroupLayout *> setLayouts;
  std::vector<SpecConstant> specConstants;
  const char *debugName = nullptr;
};

class IComputePipeline {
public:
  virtual ~IComputePipeline() = default;
};

} // namespace componeng::renderer::rhi
