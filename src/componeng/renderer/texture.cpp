#include "componeng/renderer/opengl/gl_render_device.hpp"
#include "componeng/renderer/texture.hpp"

Texture2D::Texture2D() {
}

void Texture2D::generate(uint32_t width, uint32_t height, unsigned char* data, bool alpha) {
  m_impl = std::make_unique<GLTexture>();
  m_impl->generate(width, height, data, alpha);
}

void Texture2D::bind() const {
  m_impl->bind();
}
