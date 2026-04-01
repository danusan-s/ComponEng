#pragma once

#include <string>

#include "core/types.hpp"
#include "glad/glad.h"

// General purpose shader object. Compiles from file, generates
// compile/link-time error messages and hosts several utility
// functions for easy management.
class Shader {
public:
  // state
  GLuint m_id;

  Shader() : m_id(0) {}

  // sets the current shader as active
  void use() const;

  // compiles the shader from given source code
  void compile(const char* vertexSource, const char* fragmentSource,
               const char* geometrySource = nullptr);

  // utility functions
  void setFloat(const char* name, float value, bool useShader = false) const;
  void setInteger(const char* name, int value, bool useShader = false) const;
  void setVector2f(const char* name, float x, float y,
                   bool useShader = false) const;
  void setVector2f(const char* name, const Vec2& value,
                   bool useShader = false) const;
  void setVector3f(const char* name, float x, float y, float z,
                   bool useShader = false) const;
  void setVector3f(const char* name, const Vec3& value,
                   bool useShader = false) const;
  void setVector4f(const char* name, float x, float y, float z, float w,
                   bool useShader = false) const;
  void setVector4f(const char* name, const Vec4& value,
                   bool useShader = false) const;
  void setMatrix4(const char* name, const Mat4& matrix,
                  bool useShader = false) const;

private:
  void checkCompileErrors(GLuint object, std::string type);
};
