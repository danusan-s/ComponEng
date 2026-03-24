#ifndef SHADER_H
#define SHADER_H

#include <string>

#include "glad/glad.h"
#include "core/types.hpp"

// General purpose shader object. Compiles from file, generates
// compile/link-time error messages and hosts several utility
// functions for easy management.
class Shader {
public:
  // state
  unsigned int ID;
  // constructor
  Shader() : ID(0) {
  }
  // sets the current shader as active
  void Use() const;
  // compiles the shader from given source code
  void Compile(const char *vertexSource, const char *fragmentSource,
               const char *geometrySource =
                   nullptr); // note: geometry source code is optional
  // utility functions
  void SetFloat(const char *name, float value, bool useShader = false) const;
  void SetInteger(const char *name, int value, bool useShader = false) const;
  void SetVector2f(const char *name, float x, float y,
                   bool useShader = false) const;
  void SetVector2f(const char *name, const Vec2 &value,
                   bool useShader = false) const;
  void SetVector3f(const char *name, float x, float y, float z,
                   bool useShader = false) const;
  void SetVector3f(const char *name, const Vec3 &value,
                   bool useShader = false) const;
  void SetVector4f(const char *name, float x, float y, float z, float w,
                   bool useShader = false) const;
  void SetVector4f(const char *name, const Vec4 &value,
                   bool useShader = false) const;
  void SetMatrix4(const char *name, const Mat4 &matrix,
                  bool useShader = false) const;

private:
  // checks if compilation or linking failed and if so, print the error logs
  void checkCompileErrors(unsigned int object, std::string type);
};

#endif
