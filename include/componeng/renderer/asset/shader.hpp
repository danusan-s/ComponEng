#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/frame_data.hpp"

#include <memory>

namespace componeng::renderer {

class Shader {
public:
  /** Creates the backend shader through `device`. */
  explicit Shader(api::IRenderDevice &device);

  void use() const;
  void compile(const char *vertexSource, const char *fragmentSource,
               const char *geometrySource = nullptr);

  void setFloat(const char *name, float value) const;
  void setInteger(const char *name, int value) const;
  void setVector2f(const char *name, float x, float y) const;
  void setVector2f(const char *name, const core::Vec2 &value) const;
  void setVector3f(const char *name, float x, float y, float z) const;
  void setVector3f(const char *name, const core::Vec3 &value) const;
  void setVector4f(const char *name, float x, float y, float z, float w) const;
  void setVector4f(const char *name, const core::Vec4 &value) const;
  void setMatrix4(const char *name, const core::Mat4 &matrix) const;

  /** Attaches the shader's uniform block `blockName` to uniform-buffer
   *  binding point `binding`. False if the shader declares no such block. */
  bool bindUniformBlock(const char *blockName, uint32_t binding) const;

  /** True if the compiled shader declares the FrameData uniform block, in
   *  which case it reads per-frame values from the shared buffer and the
   *  RenderSystem does not send them as loose uniforms. */
  bool usesFrameData() const {
    return m_usesFrameData;
  }

  /** Per-shader GPU timing (see IShader::beginGpuTimer). Time the work
   *  submitted between begin and end against this shader; call
   *  collectGpuTimer() once per frame to close it and read finished results. */
  void beginGpuTimer() const;
  void endGpuTimer() const;
  void collectGpuTimer() const;
  /** GPU milliseconds this shader used in the latest completed frame, or
   *  negative if no result is available yet. Lags by a few frames. */
  double lastGpuTimeMs() const;

  api::VertexLayout reflectInstanceLayout() const;
  std::vector<core::Name> reflectActiveUniformNames() const;

private:
  std::unique_ptr<api::IShader> m_impl;
  bool m_usesFrameData = false;
};

} // namespace componeng::renderer
