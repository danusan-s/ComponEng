#include "componeng/renderer/asset/shader.hpp"

#include "componeng/utils/logger.hpp"

#include <algorithm>
#include <atomic>

namespace componeng::renderer {

namespace {

constexpr const char *kEmptyFragment =
    "#version 460\nlayout(location = 0) out vec4 unused;\nvoid main() {}\n";

shader::Target targetFor(rhi::Backend backend) {
  return backend == rhi::Backend::Vulkan ? shader::Target::Vulkan
                                         : shader::Target::OpenGL;
}

void logFailure(const std::string &name, const char *stage,
                const std::string &log) {
  LOG_ERROR("Shader '%s' (%s) failed to compile:\n%s", name.c_str(), stage,
            log.c_str());
}

uint64_t nextTimerId() {
  static std::atomic<uint64_t> next{1};
  return next++;
}

} // namespace

Shader::Shader(rhi::IDevice &device)
    : m_device(&device), m_timerId(nextTimerId()) {
}

rhi::IShaderModule *
Shader::module(rhi::ShaderStage stage, const shader::CompiledStage &compiled,
               std::unique_ptr<rhi::IShaderModule> &slot) const {
  rhi::ShaderModuleDesc d;
  d.stage = stage;
  d.code = compiled.spirv.data();
  d.wordCount = compiled.spirv.size();
  d.debugName = m_name.c_str();
  slot = m_device->createShaderModule(d);
  return slot.get();
}

bool Shader::compile(const std::string &vertexSource,
                     const std::string &fragmentSource, const std::string &name,
                     const std::vector<std::string> &includeRoots) {
  m_valid = false;
  m_name = name;
  m_fragmentSource = fragmentSource;
  m_roots = includeRoots;
  m_vertex.reset();
  m_fragment.reset();
  m_materialLayout.reset();
  m_depthFragment.reset();
  m_alphaTestedFragment.reset();
  m_reflection = shader::Reflection{};

  const shader::Target target = targetFor(m_device->backend());
  const shader::CompiledStage vs =
      shader::compile(rhi::ShaderStage::Vertex, vertexSource, name + ".vert",
                      target, shader::ShaderFlag::None, includeRoots);
  const shader::CompiledStage fs = shader::compile(
      rhi::ShaderStage::Fragment, fragmentSource, name + ".frag", target,
      shader::ShaderFlag::None, includeRoots);
  if (!vs.ok) {
    logFailure(name, "vertex", vs.log);
  }
  if (!fs.ok) {
    logFailure(name, "fragment", fs.log);
  }
  if (!vs.ok || !fs.ok) {
    return false;
  }

  std::string error;
  if (!shader::mergeReflection({&vs.reflection, &fs.reflection}, m_reflection,
                               error)) {
    LOG_ERROR("Shader '%s': %s", name.c_str(), error.c_str());
    return false;
  }

  const shader::ReflectedResource *frame =
      m_reflection.find(kFrameDataBlockName);
  if (frame && frame->sizeBytes != sizeof(FrameData)) {
    LOG_ERROR("Shader '%s': its FrameData block is %u bytes, but "
              "renderer/frame_data.hpp has %zu; they have drifted apart",
              name.c_str(), frame->sizeBytes, sizeof(FrameData));
  }

  module(rhi::ShaderStage::Vertex, vs, m_vertex);
  module(rhi::ShaderStage::Fragment, fs, m_fragment);
  m_materialLayout =
      m_device->createBindGroupLayout(shader::setLayout(m_reflection, 1));
  m_valid = true;
  return true;
}

Shader::Program Shader::program(Variant variant) const {
  if (!m_valid) {
    return {};
  }
  const shader::Target target = targetFor(m_device->backend());
  switch (variant) {
    case Variant::Color:
      return {m_vertex.get(), m_fragment.get()};
    case Variant::DepthOnly: {
      if (!m_depthFragment) {
        const shader::CompiledStage empty = shader::compile(
            rhi::ShaderStage::Fragment, kEmptyFragment, "depth_only.frag",
            target, shader::ShaderFlag::None);
        module(rhi::ShaderStage::Fragment, empty, m_depthFragment);
      }
      return {m_vertex.get(), m_depthFragment.get()};
    }
    case Variant::AlphaTestedDepth: {
      if (!m_alphaTestedFragment) {
        const shader::CompiledStage fs = shader::compile(
            rhi::ShaderStage::Fragment, m_fragmentSource, m_name + ".frag",
            target, shader::ShaderFlag::DepthPrepass, m_roots);
        if (!fs.ok) {
          logFailure(m_name, "alpha-tested depth fragment", fs.log);
          return {};
        }
        module(rhi::ShaderStage::Fragment, fs, m_alphaTestedFragment);
      }
      return {m_vertex.get(), m_alphaTestedFragment.get()};
    }
  }
  return {};
}

const shader::ReflectedResource *Shader::materialBlock() const {
  for (const shader::ReflectedResource &r : m_reflection.resources) {
    if (r.set == 1 && r.binding == 0 &&
        r.kind == shader::ResourceKind::UniformBlock) {
      return &r;
    }
  }
  return nullptr;
}

api::VertexLayout Shader::instanceLayout() const {
  api::VertexLayout layout;
  uint32_t offset = 0;
  for (const shader::VertexInput &in : m_reflection.vertexInputs) {
    if (in.location < api::kFirstInstanceAttribLocation) {
      continue;
    }
    uint32_t components = 0;
    bool matrix = false;
    switch (in.type) {
      case shader::ValueType::Float:
        components = 1;
        break;
      case shader::ValueType::Vec2:
        components = 2;
        break;
      case shader::ValueType::Vec3:
        components = 3;
        break;
      case shader::ValueType::Vec4:
        components = 4;
        break;
      case shader::ValueType::Mat4:
        components = 4;
        matrix = true;
        break;
      default:
        LOG_ERROR("Shader '%s': per-instance input '%s' must be a float "
                  "vector or a mat4",
                  m_name.c_str(), in.name.c_str());
        continue;
    }
    for (int row = 0; row < (matrix ? 4 : 1); ++row) {
      api::VertexAttribute a;
      a.name = core::Name(in.name.c_str());
      a.offset = offset;
      a.componentCount = components;
      a.matrixRow = matrix ? row : -1;
      a.location = static_cast<int>(in.location) + row;
      layout.attributes.push_back(a);
      offset += components * sizeof(float);
    }
  }
  layout.stride = offset;
  return layout;
}

void Shader::beginGpuTimer() const {
  m_device->beginShaderTimer(m_timerId);
}

void Shader::endGpuTimer() const {
  m_device->endShaderTimer(m_timerId);
}

double Shader::lastGpuTimeMs() const {
  return m_device->shaderTimeMs(m_timerId);
}

} // namespace componeng::renderer
