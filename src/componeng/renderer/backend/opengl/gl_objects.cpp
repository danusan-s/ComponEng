#include "componeng/renderer/backend/opengl/gl_objects.hpp"

#include "componeng/utils/logger.hpp"

#include <algorithm>
#include <cmath>

namespace componeng::renderer::opengl {

namespace {

GLenum stageEnum(rhi::ShaderStage stage) {
  switch (stage) {
    case rhi::ShaderStage::Vertex:
      return GL_VERTEX_SHADER;
    case rhi::ShaderStage::Fragment:
      return GL_FRAGMENT_SHADER;
    case rhi::ShaderStage::Compute:
      return GL_COMPUTE_SHADER;
  }
  return GL_VERTEX_SHADER;
}

GLuint compileStage(const GLShaderModule &module,
                    const std::vector<rhi::SpecConstant> &spec) {
  if (module.code().empty()) {
    return 0;
  }
  const GLuint shader = glCreateShader(stageEnum(module.stage()));
  glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V,
                 module.code().data(),
                 static_cast<GLsizei>(module.code().size() *
                                      sizeof(uint32_t)));

  std::vector<GLuint> ids;
  std::vector<GLuint> values;
  ids.reserve(spec.size());
  values.reserve(spec.size());
  for (const rhi::SpecConstant &c : spec) {
    ids.push_back(c.id);
    values.push_back(c.value);
  }
  glSpecializeShader(shader, "main", static_cast<GLuint>(ids.size()),
                     ids.data(), values.data());

  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    LOG_ERROR("OpenGL: %s SPIR-V failed to specialize:\n%s",
              module.stage() == rhi::ShaderStage::Vertex     ? "vertex"
              : module.stage() == rhi::ShaderStage::Fragment ? "fragment"
                                                             : "compute",
              log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint linkProgram(const GLShaderModule *vertex, const GLShaderModule *fragment,
                   const GLShaderModule *compute,
                   const std::vector<rhi::SpecConstant> &spec,
                   const char *debugName) {
  const GLuint program = glCreateProgram();
  GLuint stages[3] = {};
  const GLShaderModule *modules[3] = {vertex, fragment, compute};
  for (int i = 0; i < 3; ++i) {
    if (!modules[i]) {
      continue;
    }
    stages[i] = compileStage(*modules[i], spec);
    if (stages[i] == 0) {
      for (GLuint s : stages) {
        if (s != 0) {
          glDeleteShader(s);
        }
      }
      glDeleteProgram(program);
      return 0;
    }
    glAttachShader(program, stages[i]);
  }

  glLinkProgram(program);
  for (GLuint s : stages) {
    if (s != 0) {
      glDeleteShader(s);
    }
  }

  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048] = {};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    LOG_ERROR("OpenGL: shader program%s%s failed to link:\n%s",
              debugName ? " " : "", debugName ? debugName : "", log);
    glDeleteProgram(program);
    return 0;
  }
  if (debugName) {
    glObjectLabel(GL_PROGRAM, program, -1, debugName);
  }
  return program;
}

struct AttribFormat {
  GLint components;
  GLenum type;
};

AttribFormat attribFormatOf(rhi::Format format) {
  switch (format) {
    case rhi::Format::R32F:
      return {1, GL_FLOAT};
    case rhi::Format::RG32F:
      return {2, GL_FLOAT};
    case rhi::Format::RGB32F:
      return {3, GL_FLOAT};
    case rhi::Format::RGBA32F:
      return {4, GL_FLOAT};
    default:
      return {4, GL_FLOAT};
  }
}

uint32_t fullMipChain(rhi::Extent2D extent) {
  const uint32_t largest = std::max(extent.width, extent.height);
  if (largest == 0) {
    return 1;
  }
  return static_cast<uint32_t>(std::floor(std::log2(
                                   static_cast<float>(largest)))) +
         1;
}

GLint wrapOf(rhi::WrapMode wrap) {
  switch (wrap) {
    case rhi::WrapMode::Repeat:
      return GL_REPEAT;
    case rhi::WrapMode::MirroredRepeat:
      return GL_MIRRORED_REPEAT;
    case rhi::WrapMode::ClampToEdge:
      break;
  }
  return GL_CLAMP_TO_EDGE;
}

GLint minFilterOf(const rhi::SamplerDesc &desc) {
  // A Nearest mip filter means "do not use the chain" (the scene sampler asks
  // for exactly that); otherwise combine the two filters.
  if (desc.mipFilter == rhi::Filter::Nearest) {
    return desc.minFilter == rhi::Filter::Nearest ? GL_NEAREST : GL_LINEAR;
  }
  if (desc.minFilter == rhi::Filter::Nearest) {
    return GL_NEAREST_MIPMAP_LINEAR;
  }
  return GL_LINEAR_MIPMAP_LINEAR;
}

float maxAnisotropy() {
  static const float value = [] {
    GLfloat maxValue = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxValue);
    return std::max(1.0f, maxValue);
  }();
  return value;
}

} // namespace

GLenum toGLFormat(rhi::Format format) {
  switch (format) {
    case rhi::Format::R8:
      return GL_R8;
    case rhi::Format::RG8:
      return GL_RG8;
    case rhi::Format::RGBA8:
    case rhi::Format::BGRA8:
      return GL_RGBA8;
    case rhi::Format::RGBA8Srgb:
      return GL_SRGB8_ALPHA8;
    case rhi::Format::R16F:
      return GL_R16F;
    case rhi::Format::RG16F:
      return GL_RG16F;
    case rhi::Format::RGBA16F:
      return GL_RGBA16F;
    case rhi::Format::R32F:
      return GL_R32F;
    case rhi::Format::RG32F:
      return GL_RG32F;
    case rhi::Format::RGB32F:
      return GL_RGB32F;
    case rhi::Format::RGBA32F:
      return GL_RGBA32F;
    case rhi::Format::D32F:
      return GL_DEPTH_COMPONENT32F;
    case rhi::Format::Undefined:
      break;
  }
  return GL_RGBA8;
}

PixelFormat pixelFormatOf(rhi::Format format) {
  switch (format) {
    case rhi::Format::R8:
      return {GL_RED, GL_UNSIGNED_BYTE, 1};
    case rhi::Format::RG8:
      return {GL_RG, GL_UNSIGNED_BYTE, 2};
    case rhi::Format::RGBA8:
    case rhi::Format::RGBA8Srgb:
      return {GL_RGBA, GL_UNSIGNED_BYTE, 4};
    case rhi::Format::BGRA8:
      return {GL_BGRA, GL_UNSIGNED_BYTE, 4};
    case rhi::Format::R16F:
      return {GL_RED, GL_FLOAT, 1};
    case rhi::Format::RG16F:
      return {GL_RG, GL_FLOAT, 2};
    case rhi::Format::RGBA16F:
      return {GL_RGBA, GL_FLOAT, 4};
    case rhi::Format::R32F:
      return {GL_RED, GL_FLOAT, 1};
    case rhi::Format::RG32F:
      return {GL_RG, GL_FLOAT, 2};
    case rhi::Format::RGB32F:
      return {GL_RGB, GL_FLOAT, 3};
    case rhi::Format::RGBA32F:
      return {GL_RGBA, GL_FLOAT, 4};
    case rhi::Format::D32F:
      return {GL_DEPTH_COMPONENT, GL_FLOAT, 1};
    case rhi::Format::Undefined:
      break;
  }
  return {GL_RGBA, GL_UNSIGNED_BYTE, 4};
}

GLBuffer::GLBuffer(const rhi::BufferDesc &desc) : m_size(desc.size) {
  glCreateBuffers(1, &m_handle);
  // Every kind is dynamic: uploads are glNamedBufferSubData, and a GpuOnly
  // buffer filled once at load time does not benefit from the stricter flag.
  glNamedBufferStorage(m_handle,
                       static_cast<GLsizeiptr>(std::max<uint64_t>(desc.size, 1)),
                       nullptr, GL_DYNAMIC_STORAGE_BIT);
  if (desc.debugName) {
    glObjectLabel(GL_BUFFER, m_handle, -1, desc.debugName);
  }
}

GLBuffer::~GLBuffer() {
  if (m_handle != 0) {
    glDeleteBuffers(1, &m_handle);
  }
}

void GLBuffer::write(uint64_t offset, const void *data, uint64_t bytes) {
  if (bytes == 0) {
    return;
  }
  glNamedBufferSubData(m_handle, static_cast<GLintptr>(offset),
                       static_cast<GLsizeiptr>(bytes), data);
}

GLImage::GLImage(const rhi::ImageDesc &desc)
    : m_extent(desc.extent), m_format(desc.format) {
  m_mipLevels = desc.mipLevels != 0 ? desc.mipLevels : fullMipChain(desc.extent);
  const GLsizei width = static_cast<GLsizei>(std::max(desc.extent.width, 1u));
  const GLsizei height = static_cast<GLsizei>(std::max(desc.extent.height, 1u));
  glCreateTextures(GL_TEXTURE_2D, 1, &m_handle);
  glTextureStorage2D(m_handle, static_cast<GLsizei>(m_mipLevels),
                     toGLFormat(desc.format), width, height);
  // Immutable storage already defines the chain; pinning MAX_LEVEL makes a
  // mipmap min filter complete even on drivers that are literal about it.
  glTextureParameteri(m_handle, GL_TEXTURE_MAX_LEVEL,
                      static_cast<GLint>(m_mipLevels) - 1);
  if (desc.debugName) {
    glObjectLabel(GL_TEXTURE, m_handle, -1, desc.debugName);
  }
}

GLImage::~GLImage() {
  if (m_handle != 0) {
    glDeleteTextures(1, &m_handle);
  }
}

GLSampler::GLSampler(const rhi::SamplerDesc &desc) {
  glCreateSamplers(1, &m_handle);
  glSamplerParameteri(m_handle, GL_TEXTURE_MIN_FILTER, minFilterOf(desc));
  glSamplerParameteri(m_handle, GL_TEXTURE_MAG_FILTER,
                      desc.magFilter == rhi::Filter::Nearest ? GL_NEAREST
                                                             : GL_LINEAR);
  glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_S, wrapOf(desc.wrapU));
  glSamplerParameteri(m_handle, GL_TEXTURE_WRAP_T, wrapOf(desc.wrapV));
  if (desc.anisotropy > 1.0f) {
    glSamplerParameterf(m_handle, GL_TEXTURE_MAX_ANISOTROPY,
                        std::min(desc.anisotropy, maxAnisotropy()));
  }
}

GLSampler::~GLSampler() {
  if (m_handle != 0) {
    glDeleteSamplers(1, &m_handle);
  }
}

GLShaderModule::GLShaderModule(const rhi::ShaderModuleDesc &desc)
    : m_stage(desc.stage) {
  if (desc.code && desc.wordCount > 0) {
    m_code.assign(desc.code, desc.code + desc.wordCount);
  }
}

GLBindGroup::GLBindGroup(const rhi::BindGroupDesc &desc) {
  if (!desc.layout) {
    LOG_ERROR("OpenGL: bind group created without a layout");
    return;
  }
  const auto &bindings =
      static_cast<const GLBindGroupLayout *>(desc.layout)->bindings();
  const auto findType = [&bindings](uint32_t binding) -> const rhi::BindingDesc * {
    for (const rhi::BindingDesc &b : bindings) {
      if (b.binding == binding) {
        return &b;
      }
    }
    return nullptr;
  };

  for (const rhi::BindGroupEntry &entry : desc.entries) {
    const rhi::BindingDesc *binding = findType(entry.binding);
    if (!binding) {
      LOG_ERROR("OpenGL: bind group has binding %u, which its layout does not",
                entry.binding);
      continue;
    }
    Entry e;
    e.binding = entry.binding;
    e.type = binding->type;
    if (binding->type == rhi::BindingType::UniformBuffer ||
        binding->type == rhi::BindingType::StorageBuffer) {
      if (!entry.buffer) {
        LOG_ERROR("OpenGL: buffer binding %u has no buffer", entry.binding);
        continue;
      }
      auto *buffer = static_cast<GLBuffer *>(entry.buffer);
      e.buffer = buffer->handle();
      e.offset = entry.offset;
      const uint64_t available =
          buffer->size() > entry.offset ? buffer->size() - entry.offset : 0;
      e.range = entry.range != 0 ? std::min(entry.range, available) : available;
      if (e.range == 0) {
        LOG_ERROR("OpenGL: buffer binding %u is empty", entry.binding);
        continue;
      }
    } else {
      if (!entry.image) {
        LOG_ERROR("OpenGL: image binding %u has no image", entry.binding);
        continue;
      }
      auto *image = static_cast<GLImage *>(entry.image);
      e.image = image->handle();
      e.imageFormat = toGLFormat(image->format());
      if (entry.sampler) {
        e.sampler = static_cast<GLSampler *>(entry.sampler)->handle();
      }
    }
    m_entries.push_back(e);
  }
}

GLGraphicsPipeline::GLGraphicsPipeline(const rhi::GraphicsPipelineDesc &desc)
    : m_state(desc.state) {
  if (!desc.vertex || !desc.fragment) {
    LOG_ERROR("OpenGL: graphics pipeline needs a vertex and fragment module");
    return;
  }
  m_program = linkProgram(static_cast<const GLShaderModule *>(desc.vertex),
                          static_cast<const GLShaderModule *>(desc.fragment),
                          nullptr, desc.specConstants, desc.debugName);
  if (m_program == 0) {
    return;
  }

  glCreateVertexArrays(1, &m_vao);
  for (const rhi::VertexBindingDesc &binding : desc.vertexInput.bindings) {
    if (binding.binding < m_strides.size()) {
      m_strides[binding.binding] = binding.stride;
    }
    glVertexArrayBindingDivisor(
        m_vao, binding.binding,
        binding.rate == rhi::VertexInputRate::PerInstance ? 1 : 0);
  }
  for (const rhi::VertexAttributeDesc &attribute : desc.vertexInput.attributes) {
    const AttribFormat format = attribFormatOf(attribute.format);
    glEnableVertexArrayAttrib(m_vao, attribute.location);
    glVertexArrayAttribFormat(m_vao, attribute.location, format.components,
                              format.type, GL_FALSE, attribute.offset);
    glVertexArrayAttribBinding(m_vao, attribute.location, attribute.binding);
  }
}

GLGraphicsPipeline::~GLGraphicsPipeline() {
  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
  }
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

GLComputePipeline::GLComputePipeline(const rhi::ComputePipelineDesc &desc) {
  if (!desc.compute) {
    LOG_ERROR("OpenGL: compute pipeline needs a compute module");
    return;
  }
  m_program = linkProgram(nullptr, nullptr,
                          static_cast<const GLShaderModule *>(desc.compute),
                          desc.specConstants, desc.debugName);
}

GLComputePipeline::~GLComputePipeline() {
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

} // namespace componeng::renderer::opengl
