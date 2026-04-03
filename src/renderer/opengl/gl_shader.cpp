#include "core/logger.hpp"
#include "renderer/opengl/gl_shader.hpp"

GLShader::GLShader() : m_id(0) {
}

GLShader::~GLShader() {
  release();
}

void GLShader::compile(const char* vertexSource, const char* fragmentSource,
                       const char* geometrySource) {
  if (m_id != 0) {
    glDeleteProgram(m_id);
    m_id = 0;
  }

  GLuint sVertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(sVertex, 1, &vertexSource, nullptr);
  glCompileShader(sVertex);
  checkCompileErrors(sVertex, "VERTEX");

  GLuint sFragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(sFragment, 1, &fragmentSource, nullptr);
  glCompileShader(sFragment);
  checkCompileErrors(sFragment, "FRAGMENT");

  GLuint gShader = 0;
  if (geometrySource != nullptr) {
    gShader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(gShader, 1, &geometrySource, nullptr);
    glCompileShader(gShader);
    checkCompileErrors(gShader, "GEOMETRY");
  }

  m_id = glCreateProgram();
  glAttachShader(m_id, sVertex);
  glAttachShader(m_id, sFragment);
  if (geometrySource != nullptr)
    glAttachShader(m_id, gShader);
  glLinkProgram(m_id);
  checkCompileErrors(m_id, "PROGRAM");

  glDeleteShader(sVertex);
  glDeleteShader(sFragment);
  if (geometrySource != nullptr)
    glDeleteShader(gShader);
}

void GLShader::use() const {
  glUseProgram(m_id);
}

void GLShader::release() {
  if (m_id != 0) {
    glDeleteProgram(m_id);
    m_id = 0;
  }
}

void GLShader::setFloat(const char* name, float value) const {
  glUniform1f(glGetUniformLocation(m_id, name), value);
}

void GLShader::setInteger(const char* name, int value) const {
  glUniform1i(glGetUniformLocation(m_id, name), value);
}

void GLShader::setVector2f(const char* name, float x, float y) const {
  glUniform2f(glGetUniformLocation(m_id, name), x, y);
}

void GLShader::setVector3f(const char* name, float x, float y, float z) const {
  glUniform3f(glGetUniformLocation(m_id, name), x, y, z);
}

void GLShader::setVector4f(const char* name, float x, float y, float z,
                           float w) const {
  glUniform4f(glGetUniformLocation(m_id, name), x, y, z, w);
}

void GLShader::setMatrix4(const char* name, const float* matrix) const {
  glUniformMatrix4fv(glGetUniformLocation(m_id, name), 1, false, matrix);
}

void GLShader::checkCompileErrors(GLuint object, const std::string& type) {
  int success;
  char infoLog[1024];
  if (type != "PROGRAM") {
    glGetShaderiv(object, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(object, 1024, nullptr, infoLog);
      LOG_ERROR("ERROR::SHADER: Compile-time error: Type: %s\n%s", type.c_str(),
                infoLog);
    }
  } else {
    glGetProgramiv(object, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(object, 1024, nullptr, infoLog);
      LOG_ERROR("ERROR::Shader: Link-time error: Type: %s\n%s", type.c_str(),
                infoLog);
    }
  }
}
