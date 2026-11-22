#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/frame_data.hpp"
#include "componeng/renderer/shader/compiler.hpp"

#include <memory>
#include <string>
#include <vector>

namespace componeng::renderer {

/**
 * @brief A vertex and fragment shader compiled for the device's API, with
 * what they declare.
 *
 * The shader is compiled to SPIR-V once; a draw uses one of three programs
 * built from it (Variant). The vertex module is the same for all three, so
 * the depth pre-pass computes exactly the depth the colour pass does.
 *
 * Resources: set 0 is the per-frame block, owned by the renderer. Set 1 is the
 * shader's own: an optional uniform block at binding 0 holding its parameters
 * (a material fills it by member name), then samplers and storage buffers.
 */
class Shader {
public:
  explicit Shader(rhi::IDevice &device);

  /** Compiles both stages. `includeRoots` are searched for `#include`s after
   *  the including file's own directory. Errors are logged; false if either
   *  stage failed, and the shader is then not valid(). */
  bool compile(const std::string &vertexSource,
               const std::string &fragmentSource,
               const std::string &name = "shader",
               const std::vector<std::string> &includeRoots = {});

  bool valid() const {
    return m_valid;
  }

  enum class Variant {
    // The shader's own fragment stage.
    Color,
    // An empty fragment stage: the depth buffer is written by the rasteriser.
    DepthOnly,
    // The shader's fragment stage compiled with COMPONENG_DEPTH_PREPASS, for
    // a cutout material (or LOD fade) whose `discard` must cut the depth too.
    // A fragment shader should return right after its discard when the macro
    // is defined, so the pre-pass skips the lighting:
    //
    //     if (alpha < cutoff) discard;
    //     #ifdef COMPONENG_DEPTH_PREPASS
    //     return;
    //     #endif
    AlphaTestedDepth,
  };

  struct Program {
    rhi::IShaderModule *vertex = nullptr;
    rhi::IShaderModule *fragment = nullptr;
  };

  /** The modules for `variant`; the alpha-tested one is compiled on first
   *  use. Both null if the shader is not valid(). */
  Program program(Variant variant) const;

  /** Layout of set 1, shared by all three programs. */
  rhi::IBindGroupLayout *materialLayout() const {
    return m_materialLayout.get();
  }

  /** Everything the colour program declares, both stages merged. */
  const shader::Reflection &reflection() const {
    return m_reflection;
  }

  /** The uniform block at set 1, binding 0, or null. */
  const shader::ReflectedResource *materialBlock() const;

  /** The per-instance vertex inputs (locations from
   *  api::kFirstInstanceAttribLocation) packed in location order; a mat4
   *  becomes four rows sharing its name. */
  api::VertexLayout instanceLayout() const;

  /** Per-shader GPU timing: time the work submitted between begin and end
   *  against this shader. Results lag by a few frames. */
  void beginGpuTimer() const;
  void endGpuTimer() const;
  /** GPU milliseconds in the latest completed frame, negative if none yet. */
  double lastGpuTimeMs() const;

private:
  rhi::IShaderModule *module(rhi::ShaderStage stage,
                             const shader::CompiledStage &compiled,
                             std::unique_ptr<rhi::IShaderModule> &slot) const;

  rhi::IDevice *m_device;
  uint64_t m_timerId;
  bool m_valid = false;

  std::string m_name;
  std::string m_fragmentSource;
  std::vector<std::string> m_roots;
  shader::Reflection m_reflection;

  std::unique_ptr<rhi::IShaderModule> m_vertex;
  std::unique_ptr<rhi::IShaderModule> m_fragment;
  std::unique_ptr<rhi::IBindGroupLayout> m_materialLayout;
  mutable std::unique_ptr<rhi::IShaderModule> m_depthFragment;
  mutable std::unique_ptr<rhi::IShaderModule> m_alphaTestedFragment;
};

} // namespace componeng::renderer
