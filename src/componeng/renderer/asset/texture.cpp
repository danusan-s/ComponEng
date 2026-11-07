#include "componeng/renderer/asset/texture.hpp"

namespace componeng::renderer {

void Texture2D::generate(api::IRenderDevice &device, uint32_t width,
                         uint32_t height, unsigned char *data, bool alpha,
                         const api::TextureDesc &desc) {
  m_impl = device.createTexture();
  m_impl->generate(width, height, data, alpha, desc);
}

void Texture2D::bind() const {
  m_impl->bind();
}

} // namespace componeng::renderer
