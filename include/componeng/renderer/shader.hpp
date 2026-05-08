#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include <memory>

namespace componeng::renderer {

class Shader {
public:
  Shader();

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

private:
  std::unique_ptr<api::IShader> m_impl;
};

} // namespace componeng::renderer
