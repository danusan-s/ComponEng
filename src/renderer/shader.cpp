#include "core/logger.hpp"
#include "renderer/shader.hpp"

void Shader::use() const {
  glUseProgram(this->m_id);
}

void Shader::compile(const char* vertexSource, const char* fragmentSource,
                     const char* geometrySource) {
  if (this->m_id != 0) {
    glDeleteProgram(this->m_id);
    this->m_id = 0;
  }

  GLuint sVertex, sFragment, gShader;
  sVertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(sVertex, 1, &vertexSource, NULL);
  glCompileShader(sVertex);
  checkCompileErrors(sVertex, "VERTEX");
  sFragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(sFragment, 1, &fragmentSource, NULL);
  glCompileShader(sFragment);
  checkCompileErrors(sFragment, "FRAGMENT");
  if (geometrySource != nullptr) {
    gShader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(gShader, 1, &geometrySource, NULL);
    glCompileShader(gShader);
    checkCompileErrors(gShader, "GEOMETRY");
  }
  this->m_id = glCreateProgram();
  glAttachShader(this->m_id, sVertex);
  glAttachShader(this->m_id, sFragment);
  if (geometrySource != nullptr)
    glAttachShader(this->m_id, gShader);
  glLinkProgram(this->m_id);
  checkCompileErrors(this->m_id, "PROGRAM");
  glDeleteShader(sVertex);
  glDeleteShader(sFragment);
  if (geometrySource != nullptr)
    glDeleteShader(gShader);
}

void Shader::setFloat(const char* name, float value, bool useShader) const {
  if (useShader)
    this->use();
  glUniform1f(glGetUniformLocation(this->m_id, name), value);
}
void Shader::setInteger(const char* name, int value, bool useShader) const {
  if (useShader)
    this->use();
  glUniform1i(glGetUniformLocation(this->m_id, name), value);
}
void Shader::setVector2f(const char* name, float x, float y,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform2f(glGetUniformLocation(this->m_id, name), x, y);
}
void Shader::setVector2f(const char* name, const Vec2& value,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform2f(glGetUniformLocation(this->m_id, name), value.x, value.y);
}
void Shader::setVector3f(const char* name, const Vec3& value,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform3f(glGetUniformLocation(this->m_id, name), value.x, value.y, value.z);
}
void Shader::setVector3f(const char* name, float x, float y, float z,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform3f(glGetUniformLocation(this->m_id, name), x, y, z);
}
void Shader::setMatrix4(const char* name, const Mat4& matrix,
                        bool useShader) const {
  if (useShader)
    this->use();
  glUniformMatrix4fv(glGetUniformLocation(this->m_id, name), 1, false,
                     value_ptr(matrix));
}
void Shader::setVector4f(const char* name, float x, float y, float z, float w,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform4f(glGetUniformLocation(this->m_id, name), x, y, z, w);
}
void Shader::setVector4f(const char* name, const Vec4& value,
                         bool useShader) const {
  if (useShader)
    this->use();
  glUniform4f(glGetUniformLocation(this->m_id, name), value.x, value.y, value.z,
              value.w);
}

void Shader::checkCompileErrors(GLuint object, std::string type) {
  int success;
  char infoLog[1024];
  if (type != "PROGRAM") {
    glGetShaderiv(object, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(object, 1024, NULL, infoLog);
      LOG_ERROR("ERROR::SHADER: Compile-time error: Type: %s\n%s", type.c_str(),
          infoLog);
    }
  } else {
    glGetProgramiv(object, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(object, 1024, NULL, infoLog);
      LOG_ERROR("ERROR::Shader: Link-time error: Type: %s\n%s", type.c_str(),
          infoLog);
    }
  }
}
