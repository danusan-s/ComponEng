#include "componeng/renderer/shader/compiler.hpp"

#include "componeng/renderer/shader/include_expander.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/Logger.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>

namespace componeng::renderer::shader {

const char *const kPrelude = R"(#ifdef VULKAN
#define BIND(s, b) set = s, binding = b
#define VERTEX_INDEX gl_VertexIndex
#define INSTANCE_INDEX (gl_InstanceIndex - gl_BaseInstance)
#else
#define BIND(s, b) binding = (s) * 8 + (b)
#define VERTEX_INDEX gl_VertexID
#define INSTANCE_INDEX gl_InstanceID
#endif
)";

namespace {

constexpr uint32_t kBindingsPerSet = 8;
constexpr uint32_t kMaxSets = 2;
constexpr int kVersion = 460;

// The GL type enums glslang reports (GL_FLOAT_VEC3 and so on).
ValueType valueTypeFromGl(int gl) {
  switch (gl) {
    case 0x1406:
      return ValueType::Float;
    case 0x8B50:
      return ValueType::Vec2;
    case 0x8B51:
      return ValueType::Vec3;
    case 0x8B52:
      return ValueType::Vec4;
    case 0x1404:
      return ValueType::Int;
    case 0x8B53:
      return ValueType::IVec2;
    case 0x8B54:
      return ValueType::IVec3;
    case 0x8B55:
      return ValueType::IVec4;
    case 0x1405:
      return ValueType::UInt;
    case 0x8B5B:
      return ValueType::Mat3;
    case 0x8B5C:
      return ValueType::Mat4;
    default:
      return ValueType::Unknown;
  }
}

constexpr int kGlSampler2D = 0x8B5E;

EShLanguage languageOf(rhi::ShaderStage stage) {
  switch (stage) {
    case rhi::ShaderStage::Vertex:
      return EShLangVertex;
    case rhi::ShaderStage::Fragment:
      return EShLangFragment;
    case rhi::ShaderStage::Compute:
      return EShLangCompute;
  }
  return EShLangVertex;
}

rhi::StageFlags flagOf(rhi::ShaderStage stage) {
  switch (stage) {
    case rhi::ShaderStage::Vertex:
      return rhi::StageFlags::Vertex;
    case rhi::ShaderStage::Fragment:
      return rhi::StageFlags::Fragment;
    case rhi::ShaderStage::Compute:
      return rhi::StageFlags::Compute;
  }
  return rhi::StageFlags::None;
}

void initGlslang() {
  static std::once_flag once;
  std::call_once(once, [] { glslang::InitializeProcess(); });
}

// A parsed and linked program. The TShader keeps pointers into `text`.
struct Program {
  std::string text;
  std::unique_ptr<glslang::TShader> shader;
  std::unique_ptr<glslang::TProgram> program;
};

bool parseAndLink(EShLanguage lang, Target target, std::string text,
                  Program &out, std::string &log) {
  out.text = std::move(text);
  out.shader = std::make_unique<glslang::TShader>(lang);
  const char *src = out.text.c_str();
  out.shader->setStrings(&src, 1);
  EShMessages messages = EShMsgSpvRules;
  if (target == Target::Vulkan) {
    out.shader->setEnvInput(glslang::EShSourceGlsl, lang,
                            glslang::EShClientVulkan, kVersion);
    out.shader->setEnvClient(glslang::EShClientVulkan,
                             glslang::EShTargetVulkan_1_3);
    out.shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
    messages = static_cast<EShMessages>(messages | EShMsgVulkanRules);
  } else {
    out.shader->setEnvInput(glslang::EShSourceGlsl, lang,
                            glslang::EShClientOpenGL, kVersion);
    out.shader->setEnvClient(glslang::EShClientOpenGL,
                             glslang::EShTargetOpenGL_450);
    out.shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
  }
  if (!out.shader->parse(GetDefaultResources(), kVersion, false, messages)) {
    log += out.shader->getInfoLog();
    return false;
  }
  out.program = std::make_unique<glslang::TProgram>();
  out.program->addShader(out.shader.get());
  if (!out.program->link(messages)) {
    log += out.program->getInfoLog();
    return false;
  }
  return true;
}

void addMembers(ReflectedResource &res, glslang::TProgram &prog, int blockIndex,
                bool buffers) {
  const int count =
      buffers ? prog.getNumBufferVariables() : prog.getNumUniformVariables();
  const std::string prefix = res.name + ".";
  for (int i = 0; i < count; ++i) {
    const glslang::TObjectReflection &v =
        buffers ? prog.getBufferVariable(i) : prog.getUniform(i);
    if (v.index != blockIndex) {
      continue;
    }
    BlockMember m;
    m.name = v.name.compare(0, prefix.size(), prefix) == 0
                 ? v.name.substr(prefix.size())
                 : v.name;
    m.type = valueTypeFromGl(v.glDefineType);
    m.offset = static_cast<uint32_t>(std::max(v.offset, 0));
    m.arraySize = static_cast<uint32_t>(std::max(v.size, 1));
    m.arrayStride = static_cast<uint32_t>(std::max(v.arrayStride, 0));
    res.members.push_back(std::move(m));
  }
}

bool placeBinding(int glBinding, const std::string &name, ReflectedResource &r,
                  std::string &log) {
  if (glBinding < 0) {
    log += "ERROR: '" + name + "' has no explicit binding; use BIND(set, n)\n";
    return false;
  }
  const uint32_t b = static_cast<uint32_t>(glBinding);
  if (b >= kBindingsPerSet * kMaxSets) {
    log += "ERROR: '" + name + "' binding is out of range (sets 0 to " +
           std::to_string(kMaxSets - 1) + ", bindings 0 to " +
           std::to_string(kBindingsPerSet - 1) + ")\n";
    return false;
  }
  r.set = b / kBindingsPerSet;
  r.binding = b % kBindingsPerSet;
  return true;
}

bool reflect(glslang::TProgram &prog, rhi::ShaderStage stage, Reflection &out,
             std::string &log) {
  prog.buildReflection(
      EShReflectionAllBlockVariables | EShReflectionIntermediateIO |
      EShReflectionAllIOVariables | EShReflectionSeparateBuffers);
  bool ok = true;

  for (int i = 0; i < prog.getNumUniformBlocks(); ++i) {
    const glslang::TObjectReflection &b = prog.getUniformBlock(i);
    ReflectedResource r;
    r.name = b.name;
    r.kind = ResourceKind::UniformBlock;
    r.stages = flagOf(stage);
    r.sizeBytes = (static_cast<uint32_t>(std::max(b.size, 0)) + 15u) & ~15u;
    ok &= placeBinding(b.getBinding(), b.name, r, log);
    addMembers(r, prog, i, false);
    out.resources.push_back(std::move(r));
  }

  for (int i = 0; i < prog.getNumBufferBlocks(); ++i) {
    const glslang::TObjectReflection &b = prog.getBufferBlock(i);
    ReflectedResource r;
    r.name = b.name;
    r.kind = ResourceKind::StorageBlock;
    r.stages = flagOf(stage);
    r.sizeBytes = static_cast<uint32_t>(std::max(b.size, 0));
    ok &= placeBinding(b.getBinding(), b.name, r, log);
    addMembers(r, prog, i, true);
    out.resources.push_back(std::move(r));
  }

  for (int i = 0; i < prog.getNumUniformVariables(); ++i) {
    const glslang::TObjectReflection &u = prog.getUniform(i);
    if (u.index >= 0) {
      continue; // a block member
    }
    if (u.glDefineType != kGlSampler2D) {
      if (valueTypeFromGl(u.glDefineType) != ValueType::Unknown) {
        log += "ERROR: loose uniform '" + u.name +
               "'; put parameters in a uniform block\n";
      } else {
        log += "ERROR: unsupported resource '" + u.name +
               "'; only uniform blocks, storage blocks and sampler2D are "
               "supported\n";
      }
      ok = false;
      continue;
    }
    ReflectedResource r;
    r.name = u.name;
    r.kind = ResourceKind::Sampler2D;
    r.stages = flagOf(stage);
    ok &= placeBinding(u.getBinding(), u.name, r, log);
    out.resources.push_back(std::move(r));
  }

  std::sort(out.resources.begin(), out.resources.end(),
            [](const ReflectedResource &a, const ReflectedResource &b) {
              return a.set != b.set ? a.set < b.set : a.binding < b.binding;
            });
  for (std::size_t i = 1; i < out.resources.size(); ++i) {
    const ReflectedResource &a = out.resources[i - 1];
    const ReflectedResource &b = out.resources[i];
    if (a.set == b.set && a.binding == b.binding) {
      log += "ERROR: '" + a.name + "' and '" + b.name + "' both use set " +
             std::to_string(a.set) + ", binding " + std::to_string(a.binding) +
             "\n";
      ok = false;
    }
  }

  if (stage == rhi::ShaderStage::Vertex) {
    struct MeshInput {
      const char *name;
      ValueType type;
    };
    static const MeshInput kMesh[3] = {{"aPos", ValueType::Vec3},
                                       {"aNormal", ValueType::Vec3},
                                       {"aUV", ValueType::Vec2}};
    for (int i = 0; i < prog.getNumPipeInputs(); ++i) {
      const glslang::TObjectReflection &a = prog.getPipeInput(i);
      if (a.name.compare(0, 3, "gl_") == 0) {
        continue;
      }
      VertexInput in;
      in.name = a.name;
      in.location = a.layoutLocation();
      in.type = valueTypeFromGl(a.glDefineType);
      if (in.location < 3 && (in.name != kMesh[in.location].name ||
                              in.type != kMesh[in.location].type)) {
        log += "ERROR: vertex input '" + in.name + "' at location " +
               std::to_string(in.location) +
               "; locations 0 to 2 are the mesh's " + "aPos, aNormal and aUV\n";
        ok = false;
      }
      out.vertexInputs.push_back(std::move(in));
    }
    std::sort(out.vertexInputs.begin(), out.vertexInputs.end(),
              [](const VertexInput &a, const VertexInput &b) {
                return a.location < b.location;
              });
  }
  return ok;
}

// The text of `source` from its `#version` line on, with everything the
// compiler adds in between.
struct Assembled {
  std::string text;
  std::vector<std::string> errors;
};

Assembled assemble(const std::string &source, const std::string &name,
                   ShaderFlag flags, const std::vector<std::string> &roots) {
  Assembled a;
  std::size_t pos = 0;
  int lineNo = 1;
  std::size_t versionBegin = std::string::npos;
  while (pos <= source.size()) {
    std::size_t end = source.find('\n', pos);
    if (end == std::string::npos) {
      end = source.size();
    }
    const std::size_t first = source.find_first_not_of(" \t", pos);
    if (first != std::string::npos && first < end &&
        source.compare(first, 8, "#version") == 0) {
      versionBegin = pos;
      break;
    }
    if (end >= source.size()) {
      break;
    }
    pos = end + 1;
    ++lineNo;
  }
  if (versionBegin == std::string::npos) {
    a.errors.push_back(name + ": missing #version 460");
    return a;
  }
  std::size_t versionEnd = source.find('\n', versionBegin);
  if (versionEnd == std::string::npos) {
    versionEnd = source.size();
  }
  const std::string versionLine =
      source.substr(versionBegin, versionEnd - versionBegin);
  if (versionLine.find("460") == std::string::npos) {
    a.errors.push_back(name + ":" + std::to_string(lineNo) +
                       ": shaders must be #version 460, found '" + versionLine +
                       "'");
    return a;
  }

  const std::string body = versionEnd < source.size()
                               ? source.substr(versionEnd + 1)
                               : std::string();
  ExpandedSource expanded = expandIncludes(body, name, roots, lineNo + 1);
  a.errors = std::move(expanded.errors);

  a.text = source.substr(0, versionBegin) + versionLine + "\n";
  a.text += "#extension GL_GOOGLE_cpp_style_line_directive : enable\n";
  a.text += kPrelude;
  if (hasFlag(flags, ShaderFlag::DepthPrepass)) {
    a.text += std::string("#define ") + kDepthPrepassMacro + " 1\n";
  }
  a.text += "#line " + std::to_string(lineNo + 1) + " \"" + name + "\"\n";
  a.text += expanded.text;
  return a;
}

} // namespace

CompiledStage compile(rhi::ShaderStage stage, const std::string &source,
                      const std::string &name, Target target, ShaderFlag flags,
                      const std::vector<std::string> &roots) {
  initGlslang();
  CompiledStage result;

  Assembled assembled = assemble(source, name, flags, roots);
  if (!assembled.errors.empty()) {
    for (const std::string &e : assembled.errors) {
      result.log += "ERROR: " + e + "\n";
    }
    return result;
  }

  const EShLanguage lang = languageOf(stage);

  // Reflect from an OpenGL-semantics parse whatever the target is; see the
  // header for why.
  Program glProgram;
  if (!parseAndLink(lang, Target::OpenGL, assembled.text, glProgram,
                    result.log)) {
    return result;
  }
  if (!reflect(*glProgram.program, stage, result.reflection, result.log)) {
    return result;
  }

  Program vkProgram;
  Program *emit = &glProgram;
  if (target == Target::Vulkan) {
    if (!parseAndLink(lang, Target::Vulkan, assembled.text, vkProgram,
                      result.log)) {
      return result;
    }
    emit = &vkProgram;
  }

  spv::SpvBuildLogger logger;
  glslang::SpvOptions options;
  glslang::GlslangToSpv(*emit->program->getIntermediate(lang), result.spirv,
                        &logger, &options);
  result.log += logger.getAllMessages();
  result.ok = !result.spirv.empty();
  return result;
}

} // namespace componeng::renderer::shader
