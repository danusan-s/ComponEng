#pragma once

#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/backend/api/rhi/device.hpp"

#include <memory>

namespace componeng::renderer {

/**
 * @brief 2D texture: an image and the sampler that reads it.
 *
 * Three-channel 8-bit data is expanded to RGBA8 here, since the device has no
 * portable three-channel format.
 */
class Texture2D {
public:
  Texture2D() = default;

  /** Creates the GPU texture through `device` from 8-bit pixel data. */
  void generate(rhi::IDevice &device, uint32_t width, uint32_t height,
                unsigned char *data, bool alpha = false,
                const api::TextureDesc &desc = {});

  /** Creates the GPU texture through `device` from pixels in any
   *  api::TextureFormat. `pixels` holds width * height *
   *  api::textureCpuPixelBytes(format) bytes, rows tightly packed, top row
   *  first; null allocates the storage with undefined contents. */
  void create(rhi::IDevice &device, uint32_t width, uint32_t height,
              api::TextureFormat format, const void *pixels,
              const api::TextureDesc &desc = {});

  /** Overwrites a region of the texture. Returns false, changing nothing and
   *  logging an error, if the region does not lie inside the texture. */
  bool update(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
              const void *pixels);

  rhi::IImage *image() const {
    return m_image.get();
  }
  rhi::ISampler *sampler() const {
    return m_sampler.get();
  }

  uint32_t width() const;
  uint32_t height() const;

private:
  rhi::IDevice *m_device = nullptr;
  api::TextureFormat m_format = api::TextureFormat::RGBA8;
  std::unique_ptr<rhi::IImage> m_image;
  std::unique_ptr<rhi::ISampler> m_sampler;
};

} // namespace componeng::renderer
