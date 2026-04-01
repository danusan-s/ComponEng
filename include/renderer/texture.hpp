#pragma once

#include "glad/glad.h"

// Texture2D is able to store and configure a texture in OpenGL.
// It also hosts utility functions for easy management.
class Texture2D {
public:
  GLuint m_id;
  GLuint m_width;
  GLuint m_height;
  GLuint m_internalFormat;
  GLuint m_imageFormat;
  GLuint m_wrapS;
  GLuint m_wrapT;
  GLuint m_filterMin;
  GLuint m_filterMax;

  Texture2D();

  void generate(GLuint width, GLuint height, unsigned char* data);
  void bind() const;
};
