#pragma once

#include "core/types.hpp"
#include "renderer/api/irender_device.hpp"
#include <memory>

/**
 * @brief Shader program wrapper.
 *
 * Compiles vertex/fragment/geometry shaders from source strings and provides
 * typed uniform setters. Backed by the IShader interface.
 */
class Shader {
public:
  Shader();

  void use() const;
  void compile(const char* vertexSource, const char* fragmentSource,
               const char* geometrySource = nullptr);

  void setFloat(const char* name, float value) const;
  void setInteger(const char* name, int value) const;
  void setVector2f(const char* name, float x, float y) const;
  void setVector2f(const char* name, const Vec2& value) const;
  void setVector3f(const char* name, float x, float y, float z) const;
  void setVector3f(const char* name, const Vec3& value) const;
  void setVector4f(const char* name, float x, float y, float z, float w) const;
  void setVector4f(const char* name, const Vec4& value) const;
  void setMatrix4(const char* name, const Mat4& matrix) const;

private:
  std::unique_ptr<IShader> m_impl;
};
