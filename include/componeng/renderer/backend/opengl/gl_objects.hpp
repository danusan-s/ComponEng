#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "glad/glad.h"

#include <array>
#include <cstdint>
#include <vector>

namespace componeng::renderer::opengl {

/** The internal format glTextureStorage2D takes for a GPU format. */
GLenum toGLFormat(rhi::Format format);

/** How tightly packed CPU pixels are read by glTextureSubImage2D. */
struct PixelFormat {
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  uint32_t components = 4;
};
PixelFormat pixelFormatOf(rhi::Format format);

/** glCreateBuffers + immutable storage; writes are glNamedBufferSubData. */
class GLBuffer final : public rhi::IBuffer {
public:
  explicit GLBuffer(const rhi::BufferDesc &desc);
  ~GLBuffer() override;

  GLBuffer(const GLBuffer &) = delete;
  GLBuffer &operator=(const GLBuffer &) = delete;

  uint64_t size() const override {
    return m_size;
  }
  void write(uint64_t offset, const void *data, uint64_t bytes) override;

  GLuint handle() const {
    return m_handle;
  }

private:
  GLuint m_handle = 0;
  uint64_t m_size = 0;
};

/** A 2D texture. PerFrame images use one copy: GL has no swapchain to rotate. */
class GLImage final : public rhi::IImage {
public:
  explicit GLImage(const rhi::ImageDesc &desc);
  ~GLImage() override;

  GLImage(const GLImage &) = delete;
  GLImage &operator=(const GLImage &) = delete;

  rhi::Extent2D extent() const override {
    return m_extent;
  }
  rhi::Format format() const override {
    return m_format;
  }
  uint32_t mipLevels() const override {
    return m_mipLevels;
  }

  GLuint handle() const {
    return m_handle;
  }

private:
  GLuint m_handle = 0;
  rhi::Extent2D m_extent;
  rhi::Format m_format = rhi::Format::RGBA8;
  uint32_t m_mipLevels = 1;
};

class GLSampler final : public rhi::ISampler {
public:
  explicit GLSampler(const rhi::SamplerDesc &desc);
  ~GLSampler() override;

  GLSampler(const GLSampler &) = delete;
  GLSampler &operator=(const GLSampler &) = delete;

  GLuint handle() const {
    return m_handle;
  }

private:
  GLuint m_handle = 0;
};

/** SPIR-V words kept for glShaderBinary at pipeline creation, where the
 *  pipeline's specialisation constants are applied. */
class GLShaderModule final : public rhi::IShaderModule {
public:
  explicit GLShaderModule(const rhi::ShaderModuleDesc &desc);

  rhi::ShaderStage stage() const {
    return m_stage;
  }
  const std::vector<uint32_t> &code() const {
    return m_code;
  }

private:
  rhi::ShaderStage m_stage = rhi::ShaderStage::Vertex;
  std::vector<uint32_t> m_code;
};

/** Only what bindGroup needs to know which GL namespace an entry belongs to. */
class GLBindGroupLayout final : public rhi::IBindGroupLayout {
public:
  explicit GLBindGroupLayout(const rhi::BindGroupLayoutDesc &desc)
      : m_bindings(desc.bindings) {
  }

  const std::vector<rhi::BindingDesc> &bindings() const {
    return m_bindings;
  }

private:
  std::vector<rhi::BindingDesc> m_bindings;
};

/** Resolved handles; bound with GL bindings set * 8 + binding. */
class GLBindGroup final : public rhi::IBindGroup {
public:
  GLBindGroup(const rhi::BindGroupDesc &desc);

  struct Entry {
    uint32_t binding = 0;
    rhi::BindingType type = rhi::BindingType::UniformBuffer;
    GLuint buffer = 0;
    GLuint image = 0;
    GLuint sampler = 0;
    GLenum imageFormat = GL_RGBA8;
    uint64_t offset = 0;
    uint64_t range = 0;
  };

  const std::vector<Entry> &entries() const {
    return m_entries;
  }

private:
  std::vector<Entry> m_entries;
};

/** A linked program, its VAO and the state applied on bind. */
class GLGraphicsPipeline final : public rhi::IGraphicsPipeline {
public:
  explicit GLGraphicsPipeline(const rhi::GraphicsPipelineDesc &desc);
  ~GLGraphicsPipeline() override;

  GLGraphicsPipeline(const GLGraphicsPipeline &) = delete;
  GLGraphicsPipeline &operator=(const GLGraphicsPipeline &) = delete;

  bool valid() const {
    return m_program != 0;
  }
  GLuint program() const {
    return m_program;
  }
  GLuint vao() const {
    return m_vao;
  }
  const rhi::RenderState &state() const {
    return m_state;
  }
  /** Stride of a vertex buffer binding, 0 if the pipeline has none there. */
  uint32_t stride(uint32_t slot) const {
    return slot < m_strides.size() ? m_strides[slot] : 0;
  }

private:
  GLuint m_program = 0;
  GLuint m_vao = 0;
  rhi::RenderState m_state;
  std::array<uint32_t, 16> m_strides{};
};

class GLComputePipeline final : public rhi::IComputePipeline {
public:
  explicit GLComputePipeline(const rhi::ComputePipelineDesc &desc);
  ~GLComputePipeline() override;

  GLComputePipeline(const GLComputePipeline &) = delete;
  GLComputePipeline &operator=(const GLComputePipeline &) = delete;

  bool valid() const {
    return m_program != 0;
  }
  GLuint program() const {
    return m_program;
  }

private:
  GLuint m_program = 0;
};

} // namespace componeng::renderer::opengl
