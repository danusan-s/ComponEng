#include "renderer/opengl/gl_texture.hpp"

GLTexture::GLTexture() : m_id(0) {
}

GLTexture::~GLTexture() {
  release();
}

void GLTexture::generate(uint32_t width, uint32_t height,
                         const unsigned char* data, bool alpha) {
  m_width = width;
  m_height = height;

  if (m_id == 0) {
    glGenTextures(1, &m_id);
  }
  glBindTexture(GL_TEXTURE_2D, m_id);

  GLenum internalFormat = alpha ? GL_RGBA : GL_RGB;
  GLenum imageFormat = alpha ? GL_RGBA : GL_RGB;

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
               imageFormat, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::bind() const {
  glBindTexture(GL_TEXTURE_2D, m_id);
}

void GLTexture::release() {
  if (m_id != 0) {
    glDeleteTextures(1, &m_id);
    m_id = 0;
  }
}
