#include "componeng/renderer/asset/texture.hpp"

#include "componeng/utils/logger.hpp"

#include <vector>

namespace componeng::renderer {

namespace {

rhi::Format gpuFormat(api::TextureFormat f, bool srgb) {
  switch (f) {
    case api::TextureFormat::R8:
      return rhi::Format::R8;
    case api::TextureFormat::RG8:
      return rhi::Format::RG8;
    case api::TextureFormat::RGB8:
    case api::TextureFormat::RGBA8:
      return srgb ? rhi::Format::RGBA8Srgb : rhi::Format::RGBA8;
    case api::TextureFormat::R16F:
      return rhi::Format::R16F;
    case api::TextureFormat::RG16F:
      return rhi::Format::RG16F;
    case api::TextureFormat::RGBA16F:
      return rhi::Format::RGBA16F;
    case api::TextureFormat::R32F:
      return rhi::Format::R32F;
    case api::TextureFormat::RG32F:
      return rhi::Format::RG32F;
    case api::TextureFormat::RGBA32F:
      return rhi::Format::RGBA32F;
  }
  return rhi::Format::RGBA8;
}

rhi::Filter filterOf(api::TextureFilter f) {
  return f == api::TextureFilter::Nearest ? rhi::Filter::Nearest
                                          : rhi::Filter::Linear;
}

rhi::WrapMode wrapOf(api::TextureWrap w) {
  switch (w) {
    case api::TextureWrap::ClampToEdge:
      return rhi::WrapMode::ClampToEdge;
    case api::TextureWrap::Repeat:
      return rhi::WrapMode::Repeat;
    case api::TextureWrap::MirroredRepeat:
      return rhi::WrapMode::MirroredRepeat;
  }
  return rhi::WrapMode::Repeat;
}

// Three bytes per pixel in, four out with an opaque alpha.
std::vector<unsigned char> expandRgb(const unsigned char *src, size_t pixels) {
  std::vector<unsigned char> out(pixels * 4);
  for (size_t i = 0; i < pixels; ++i) {
    out[i * 4 + 0] = src[i * 3 + 0];
    out[i * 4 + 1] = src[i * 3 + 1];
    out[i * 4 + 2] = src[i * 3 + 2];
    out[i * 4 + 3] = 255;
  }
  return out;
}

} // namespace

void Texture2D::generate(rhi::IDevice &device, uint32_t width, uint32_t height,
                         unsigned char *data, bool alpha,
                         const api::TextureDesc &desc) {
  create(device, width, height,
         alpha ? api::TextureFormat::RGBA8 : api::TextureFormat::RGB8, data,
         desc);
}

void Texture2D::create(rhi::IDevice &device, uint32_t width, uint32_t height,
                       api::TextureFormat format, const void *pixels,
                       const api::TextureDesc &desc) {
  m_device = &device;
  m_format = format;

  rhi::ImageDesc id;
  id.extent = {width, height};
  id.format = gpuFormat(format, desc.srgb);
  id.usage = rhi::ImageUsage::Sampled | rhi::ImageUsage::TransferDst |
             rhi::ImageUsage::TransferSrc;
  id.mipLevels = desc.mipmaps ? 0 : 1;
  m_image = device.createImage(id);

  rhi::SamplerDesc sd;
  sd.minFilter = filterOf(desc.minFilter);
  sd.magFilter = filterOf(desc.magFilter);
  sd.mipFilter = sd.minFilter;
  sd.wrapU = sd.wrapV = wrapOf(desc.wrap);
  sd.anisotropy = desc.anisotropy;
  m_sampler = device.createSampler(sd);

  if (!pixels) {
    return;
  }
  if (format == api::TextureFormat::RGB8) {
    const std::vector<unsigned char> rgba =
        expandRgb(static_cast<const unsigned char *>(pixels),
                  static_cast<size_t>(width) * height);
    device.uploadImage(*m_image, {0, 0, width, height}, rgba.data());
  } else {
    device.uploadImage(*m_image, {0, 0, width, height}, pixels);
  }
}

bool Texture2D::update(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       const void *pixels) {
  if (!m_image || width == 0 || height == 0 || x > this->width() ||
      y > this->height() || width > this->width() - x ||
      height > this->height() - y) {
    LOG_ERROR("Texture update region (%u,%u %ux%u) is outside the %ux%u "
              "texture",
              x, y, width, height, this->width(), this->height());
    return false;
  }
  if (m_format == api::TextureFormat::RGB8) {
    const std::vector<unsigned char> rgba =
        expandRgb(static_cast<const unsigned char *>(pixels),
                  static_cast<size_t>(width) * height);
    m_device->uploadImage(
        *m_image,
        {static_cast<int32_t>(x), static_cast<int32_t>(y), width, height},
        rgba.data());
  } else {
    m_device->uploadImage(
        *m_image,
        {static_cast<int32_t>(x), static_cast<int32_t>(y), width, height},
        pixels);
  }
  return true;
}

uint32_t Texture2D::width() const {
  return m_image ? m_image->extent().width : 0;
}

uint32_t Texture2D::height() const {
  return m_image ? m_image->extent().height : 0;
}

} // namespace componeng::renderer
