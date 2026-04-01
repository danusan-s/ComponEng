#include "renderer/texture.hpp"

Texture2D::Texture2D()
    : m_id(0), m_width(0), m_height(0), m_internalFormat(GL_RGB), m_imageFormat(GL_RGB),
      m_wrapS(GL_CLAMP_TO_EDGE), m_wrapT(GL_CLAMP_TO_EDGE),
      m_filterMin(GL_LINEAR_MIPMAP_LINEAR), m_filterMax(GL_LINEAR) {
}

void Texture2D::generate(GLuint width, GLuint height,
                         unsigned char* data) {
  this->m_width = width;
  this->m_height = height;
  if (this->m_id == 0)
    glGenTextures(1, &this->m_id);
  glBindTexture(GL_TEXTURE_2D, this->m_id);

  glTexImage2D(GL_TEXTURE_2D, 0, this->m_internalFormat, width, height, 0,
               this->m_imageFormat, GL_UNSIGNED_BYTE, data);

  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, this->m_wrapS);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, this->m_wrapT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->m_filterMin);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->m_filterMax);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::bind() const {
  glBindTexture(GL_TEXTURE_2D, this->m_id);
}
