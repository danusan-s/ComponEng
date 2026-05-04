#include "componeng/core/logger.hpp"
#include "componeng/renderer/opengl/gl_shader.hpp"
#include <fstream>
#include <sstream>

GLShader::GLShader() : m_id(0) {
}

GLShader::~GLShader() {
  release();
}

void GLShader::loadGLSL(const char* vertexSource, const char* fragmentSource,
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

void GLShader::loadSPIRV(const char* vertexPath, const char* fragmentPath,
                         const char* geometryPath) {
  // OpenGL doesn't natively support SPIR-V without GL_ARB_gl_spirv extension.
  // Fall back to loading GLSL source by changing .spv extension to .vert/.frag
  auto stripExt = [](const char* path) -> std::string {
    std::string s(path);
    auto dot = s.find_last_of('.');
    if (dot != std::string::npos) {
      // Remove .spv suffix
      if (s.size() >= 4 && s.substr(dot) == ".spv") {
        s.erase(dot);
      }
    }
    return s;
  };

  std::string vertPath = stripExt(vertexPath);
  std::string fragPath = stripExt(fragmentPath);
  std::string geomPath;
  if (geometryPath != nullptr) {
    geomPath = stripExt(geometryPath);
  }

  auto readFile = [](const std::string& path) -> std::string {
    std::ifstream file(path);
    if (!file.is_open()) {
      LOG_ERROR("Failed to open shader file: %s", path.c_str());
      return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
  };

  std::string vertSrc = readFile(vertPath);
  std::string fragSrc = readFile(fragPath);
  std::string geomSrc;
  if (geometryPath != nullptr && !geomPath.empty()) {
    geomSrc = readFile(geomPath);
  }

  if (!vertSrc.empty() && !fragSrc.empty()) {
    loadGLSL(vertSrc.c_str(), fragSrc.c_str(),
             geometryPath != nullptr && !geomSrc.empty() ? geomSrc.c_str() : nullptr);
  }
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
