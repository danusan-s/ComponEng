#pragma once

#include "componeng/renderer/backend/api/irender_device.hpp"

#include <memory>

namespace componeng::renderer {

/**
 * @brief 2D texture wrapper with generation, configuration, and binding.
 *
 * Backed by the ITexture interface.
 */
class Texture2D {
public:
  Texture2D() = default;

  /** Creates the GPU texture through `device` from 8-bit pixel data. */
  void generate(api::IRenderDevice &device, uint32_t width, uint32_t height,
                unsigned char *data, bool alpha = false,
                const api::TextureDesc &desc = {});
  void bind() const;

private:
  std::unique_ptr<api::ITexture> m_impl;
};

} // namespace componeng::renderer
