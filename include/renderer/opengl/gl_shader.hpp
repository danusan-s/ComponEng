#pragma once

#include "glad/glad.h"
#include "renderer/api/irender_device.hpp"

class GLShader : public IShader {
public:
  GLShader();
  ~GLShader() override;

  void compile(const char* vertexSource, const char* fragmentSource,
               const char* geometrySource = nullptr) override;
  void use() const override;
  void release() override;

  void setFloat(const char* name, float value) const override;
  void setInteger(const char* name, int value) const override;
  void setVector2f(const char* name, float x, float y) const override;
  void setVector3f(const char* name, float x, float y, float z) const override;
  void setVector4f(const char* name, float x, float y, float z,
                   float w) const override;
  void setMatrix4(const char* name, const float* matrix) const override;

  GLuint handle() const { return m_id; }

private:
  void checkCompileErrors(GLuint object, const std::string& type);

  GLuint m_id = 0;
};
