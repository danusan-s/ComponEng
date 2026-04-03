#pragma once

#include "renderer/api/irender_device.hpp"
#include <memory>

/**
 * @brief 2D texture wrapper with generation, configuration, and binding.
 *
 * Backed by the ITexture interface.
 */
class Texture2D {
public:
  Texture2D();

  void generate(uint32_t width, uint32_t height, unsigned char* data, bool alpha = false);
  void bind() const;

private:
  std::unique_ptr<ITexture> m_impl;
};
