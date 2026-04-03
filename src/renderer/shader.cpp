#include "core/logger.hpp"
#include "renderer/opengl/gl_render_device.hpp"
#include "renderer/shader.hpp"

Shader::Shader() {
  GLRenderDevice tempDevice;
  m_impl = tempDevice.createShader();
}

void Shader::use() const {
  m_impl->use();
}

void Shader::compile(const char* vertexSource, const char* fragmentSource,
                     const char* geometrySource) {
  m_impl->compile(vertexSource, fragmentSource, geometrySource);
}

void Shader::setFloat(const char* name, float value) const {
  m_impl->setFloat(name, value);
}

void Shader::setInteger(const char* name, int value) const {
  m_impl->setInteger(name, value);
}

void Shader::setVector2f(const char* name, float x, float y) const {
  m_impl->setVector2f(name, x, y);
}

void Shader::setVector2f(const char* name, const Vec2& value) const {
  m_impl->setVector2f(name, value.x, value.y);
}

void Shader::setVector3f(const char* name, float x, float y, float z) const {
  m_impl->setVector3f(name, x, y, z);
}

void Shader::setVector3f(const char* name, const Vec3& value) const {
  m_impl->setVector3f(name, value.x, value.y, value.z);
}

void Shader::setVector4f(const char* name, float x, float y, float z,
                         float w) const {
  m_impl->setVector4f(name, x, y, z, w);
}

void Shader::setVector4f(const char* name, const Vec4& value) const {
  m_impl->setVector4f(name, value.x, value.y, value.z, value.w);
}

void Shader::setMatrix4(const char* name, const Mat4& matrix) const {
  m_impl->setMatrix4(name, &matrix[0][0]);
}
