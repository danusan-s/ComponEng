#pragma once

#include "componeng/renderer/backend/api/rhi/types.hpp"
#include "componeng/renderer/shader/reflection.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace componeng::renderer::shader {

/** Which API's SPIR-V to produce. Vulkan: Vulkan 1.3 semantics, SPIR-V 1.6.
 *  OpenGL: OpenGL 4.5 semantics, SPIR-V 1.0 (what ARB_gl_spirv takes). */
enum class Target { Vulkan, OpenGL };

/** Variant flags, applied to a stage as `#define` lines. The vertex stage of
 *  a program is compiled without flags so every variant shares it. */
enum class ShaderFlag : uint32_t {
  None = 0,
  DepthPrepass = 1, // COMPONENG_DEPTH_PREPASS
};

constexpr ShaderFlag operator|(ShaderFlag a, ShaderFlag b) {
  return static_cast<ShaderFlag>(std::to_underlying(a) |
                                 std::to_underlying(b));
}
constexpr bool hasFlag(ShaderFlag value, ShaderFlag flag) {
  return (std::to_underlying(value) & std::to_underlying(flag)) != 0;
}

struct CompiledStage {
  bool ok = false;
  // Compiler messages; they name included files by path.
  std::string log;
  std::vector<uint32_t> spirv;
  // This stage alone; combine stages with mergeReflection.
  Reflection reflection;
};

/**
 * Compiles one shader stage to SPIR-V and reflects it.
 *
 * `source` must start with `#version 460`. The compiler adds the prelude (the
 * BIND, VERTEX_INDEX and INSTANCE_INDEX macros, see kPrelude), the flag
 * defines, and expands `#include "x"` lines against `roots`. `name` is used in
 * messages.
 *
 * Besides glslang's own errors it rejects: loose non-opaque uniforms, any
 * resource that is not a uniform block, storage block or sampler2D, a binding
 * above 7 or a set above 1, two resources sharing a (set, binding), and vertex
 * inputs at locations 0 to 2 that are not the mesh's aPos, aNormal, aUV.
 *
 * Reflection always comes from an OpenGL-semantics parse of the same text, so
 * a raw `set = N` (which GL SPIR-V cannot express) fails here for both
 * targets, and the logical (set, binding) is the same whatever `target` is.
 */
CompiledStage compile(rhi::ShaderStage stage, const std::string &source,
                      const std::string &name, Target target, ShaderFlag flags,
                      const std::vector<std::string> &roots = {});

/** The text injected after `#version`. Exposed for tests and docs. */
extern const char *const kPrelude;

/** Name of the macro ShaderFlag::DepthPrepass defines. */
inline constexpr const char *kDepthPrepassMacro = "COMPONENG_DEPTH_PREPASS";

} // namespace componeng::renderer::shader
