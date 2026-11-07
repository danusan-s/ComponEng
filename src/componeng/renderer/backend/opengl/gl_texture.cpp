#include "componeng/renderer/backend/opengl/gl_texture.hpp"

#include <algorithm>
#include <cstring>

namespace componeng::renderer::opengl {

namespace {

// GL_EXT_texture_filter_anisotropic (and the identical ARB variant promoted
// into core in 4.6). Not part of the generated 3.3 loader, so the enums are
// spelled out and support is probed once.
constexpr GLenum kTextureMaxAnisotropy = 0x84FE;
constexpr GLenum kMaxTextureMaxAnisotropy = 0x84FF;

// Largest supported anisotropy, or 1 when the extension is absent.
float maxAnisotropy() {
  static const float value = [] {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
      const char *name =
          reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i));
      if (name &&
          (std::strcmp(name, "GL_EXT_texture_filter_anisotropic") == 0 ||
           std::strcmp(name, "GL_ARB_texture_filter_anisotropic") == 0)) {
        GLfloat maxValue = 1.0f;
        glGetFloatv(kMaxTextureMaxAnisotropy, &maxValue);
        return std::max(1.0f, maxValue);
      }
    }
    return 1.0f;
  }();
  return value;
}

GLint toGLWrap(api::TextureWrap wrap) {
  switch (wrap) {
    case api::TextureWrap::Repeat:
      return GL_REPEAT;
    case api::TextureWrap::MirroredRepeat:
      return GL_MIRRORED_REPEAT;
    case api::TextureWrap::ClampToEdge:
      break;
  }
  return GL_CLAMP_TO_EDGE;
}

GLint toGLMinFilter(api::TextureFilter filter, bool mipmaps) {
  if (filter == api::TextureFilter::Nearest) {
    return mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
  }
  return mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
}

GLint toGLMagFilter(api::TextureFilter filter) {
  return filter == api::TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
}

} // namespace

GLTexture::~GLTexture() {
  release();
}

void GLTexture::generate(uint32_t width, uint32_t height,
                         const unsigned char *data, bool alpha,
                         const api::TextureDesc &desc) {
  m_width = width;
  m_height = height;

  if (m_id == 0) {
    glGenTextures(1, &m_id);
  }
  glBindTexture(GL_TEXTURE_2D, m_id);

  GLint internalFormat;
  if (desc.srgb) {
    internalFormat = alpha ? GL_SRGB8_ALPHA8 : GL_SRGB8;
  } else {
    internalFormat = alpha ? GL_RGBA8 : GL_RGB8;
  }
  const GLenum imageFormat = alpha ? GL_RGBA : GL_RGB;

  // Rows of tightly packed RGB are 3 * width bytes, which is not a multiple of
  // GL's default 4-byte row alignment for most widths; without this such
  // images upload skewed.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, imageFormat,
               GL_UNSIGNED_BYTE, data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  if (desc.mipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGLWrap(desc.wrap));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGLWrap(desc.wrap));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  toGLMinFilter(desc.minFilter, desc.mipmaps));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  toGLMagFilter(desc.magFilter));

  if (desc.anisotropy > 1.0f && maxAnisotropy() > 1.0f) {
    glTexParameterf(GL_TEXTURE_2D, kTextureMaxAnisotropy,
                    std::min(desc.anisotropy, maxAnisotropy()));
  }

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

} // namespace componeng::renderer::opengl
