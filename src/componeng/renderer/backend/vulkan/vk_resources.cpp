#include "componeng/renderer/backend/vulkan/vk_resources.hpp"

#include "componeng/renderer/backend/vulkan/vk_render_device.hpp"
#include "componeng/utils/logger.hpp"

#include <algorithm>
#include <cstring>

namespace componeng::renderer::vulkan {

// VulkanBuffer

static constexpr VkBufferUsageFlags kBufferUsage =
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanCore> core)
    : m_core(std::move(core)) {
}

VulkanBuffer::~VulkanBuffer() {
  release();
}

void VulkanBuffer::setData(const void *data, size_t sizeBytes, Usage /*usage*/) {
  if (m_alloc.buffer == VK_NULL_HANDLE || m_alloc.size < sizeBytes) {
    release();
    if (!m_core->createMappedBuffer(sizeBytes, kBufferUsage, m_alloc)) {
      m_size = 0;
      return;
    }
  }
  m_size = sizeBytes;
  if (data && sizeBytes > 0)
    std::memcpy(m_alloc.mapped, data, sizeBytes);
}

void VulkanBuffer::setSubData(size_t offset, const void *data,
                              size_t sizeBytes) {
  if (!m_alloc.mapped || offset + sizeBytes > m_alloc.size) {
    LOG_ERROR("Vulkan: setSubData out of range (%zu + %zu > %llu)", offset,
              sizeBytes, static_cast<unsigned long long>(m_alloc.size));
    return;
  }
  std::memcpy(static_cast<char *>(m_alloc.mapped) + offset, data, sizeBytes);
}

VkBufferView VulkanBuffer::view(api::BufferTextureFormat format) {
  const int index = static_cast<int>(format);
  if (m_views[index] == VK_NULL_HANDLE) {
    VkBufferViewCreateInfo vi{VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    vi.buffer = m_alloc.buffer;
    vi.range = VK_WHOLE_SIZE;
    switch (format) {
      case api::BufferTextureFormat::R32F:
        vi.format = VK_FORMAT_R32_SFLOAT;
        break;
      case api::BufferTextureFormat::RG32F:
        vi.format = VK_FORMAT_R32G32_SFLOAT;
        break;
      case api::BufferTextureFormat::RGBA32F:
        vi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    }
    vkCreateBufferView(m_core->device, &vi, nullptr, &m_views[index]);
  }
  return m_views[index];
}

void VulkanBuffer::release() {
  if (m_alloc.buffer == VK_NULL_HANDLE && m_alloc.memory == VK_NULL_HANDLE)
    return;
  // The current frame's commands may still name this buffer.
  VulkanCore *core = m_core.get();
  VulkanCore::Allocation alloc = m_alloc;
  VkBufferView views[3] = {m_views[0], m_views[1], m_views[2]};
  core->defer([core, alloc, views]() mutable {
    for (VkBufferView v : views)
      if (v != VK_NULL_HANDLE)
        vkDestroyBufferView(core->device, v, nullptr);
    core->destroyAllocation(alloc);
  });
  m_alloc = VulkanCore::Allocation{};
  std::fill(std::begin(m_views), std::end(m_views), VkBufferView{});
  m_size = 0;
}

// VulkanTexture

namespace {

VkFormat toVkFormat(api::TextureFormat f, bool srgb) {
  switch (f) {
    case api::TextureFormat::R8:
      return VK_FORMAT_R8_UNORM;
    case api::TextureFormat::RG8:
      return VK_FORMAT_R8G8_UNORM;
    case api::TextureFormat::RGB8:
    case api::TextureFormat::RGBA8:
      return srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    case api::TextureFormat::R16F:
      return VK_FORMAT_R16_SFLOAT;
    case api::TextureFormat::RG16F:
      return VK_FORMAT_R16G16_SFLOAT;
    case api::TextureFormat::RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case api::TextureFormat::R32F:
      return VK_FORMAT_R32_SFLOAT;
    case api::TextureFormat::RG32F:
      return VK_FORMAT_R32G32_SFLOAT;
    case api::TextureFormat::RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  return VK_FORMAT_R8G8B8A8_UNORM;
}

VkSamplerAddressMode toVk(api::TextureWrap w) {
  switch (w) {
    case api::TextureWrap::ClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case api::TextureWrap::Repeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case api::TextureWrap::MirroredRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkFilter toVk(api::TextureFilter f) {
  return f == api::TextureFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

} // namespace

VulkanTexture::VulkanTexture(VulkanRenderDevice *device,
                             std::shared_ptr<VulkanCore> core)
    : m_device(device), m_core(std::move(core)) {
}

VulkanTexture::~VulkanTexture() {
  release();
}

void VulkanTexture::generate(uint32_t width, uint32_t height,
                             const unsigned char *data, bool alpha,
                             const api::TextureDesc &desc) {
  create(width, height,
         alpha ? api::TextureFormat::RGBA8 : api::TextureFormat::RGB8, data,
         desc);
}

void VulkanTexture::create(uint32_t width, uint32_t height,
                           api::TextureFormat format, const void *pixels,
                           const api::TextureDesc &desc) {
  release();
  m_width = width;
  m_height = height;
  m_expandRgb = format == api::TextureFormat::RGB8;
  m_cpuPixelBytes = api::textureCpuPixelBytes(format);
  m_format = toVkFormat(format, desc.srgb);

  VkFormatProperties fp;
  vkGetPhysicalDeviceFormatProperties(m_core->physical, m_format, &fp);
  const VkFormatFeatureFlags feats = fp.optimalTilingFeatures;
  const bool canLinear = feats & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
  const bool canBlit = canLinear && (feats & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                       (feats & VK_FORMAT_FEATURE_BLIT_DST_BIT);

  m_mipLevels = 1;
  if (desc.mipmaps && canBlit) {
    uint32_t m = std::max(width, height);
    while (m > 1) {
      m >>= 1;
      ++m_mipLevels;
    }
  }

  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = m_format;
  ii.extent = {width, height, 1};
  ii.mipLevels = m_mipLevels;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(m_core->device, &ii, nullptr, &m_image) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: failed to create a %ux%u texture", width, height);
    m_image = VK_NULL_HANDLE;
    return;
  }

  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(m_core->device, m_image, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = m_core->findMemoryType(
      req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX ||
      vkAllocateMemory(m_core->device, &ai, nullptr, &m_memory) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: out of memory for a %ux%u texture", width, height);
    vkDestroyImage(m_core->device, m_image, nullptr);
    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    return;
  }
  vkBindImageMemory(m_core->device, m_image, m_memory, 0);

  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = m_image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = m_format;
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mipLevels, 0, 1};
  vkCreateImageView(m_core->device, &vi, nullptr, &m_view);

  VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  si.magFilter = canLinear ? toVk(desc.magFilter) : VK_FILTER_NEAREST;
  si.minFilter = canLinear ? toVk(desc.minFilter) : VK_FILTER_NEAREST;
  si.mipmapMode = (canLinear && desc.minFilter == api::TextureFilter::Linear)
                      ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                      : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  si.addressModeU = si.addressModeV = si.addressModeW = toVk(desc.wrap);
  si.maxLod = static_cast<float>(m_mipLevels);
  si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  if (m_core->anisotropy && desc.anisotropy > 1.0f) {
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy =
        std::min(desc.anisotropy, m_core->props.limits.maxSamplerAnisotropy);
  }
  vkCreateSampler(m_core->device, &si, nullptr, &m_sampler);

  if (pixels) {
    upload(0, 0, width, height, pixels, /*fromUndefined=*/true);
  } else {
    VkCommandBuffer cmd = m_core->beginOneShot();
    transitionImage(cmd, m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, m_mipLevels,
                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT);
    m_core->endOneShot(cmd);
  }
}

void VulkanTexture::update(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height, const void *pixels) {
  if (m_image == VK_NULL_HANDLE || !pixels)
    return;
  if (x + width > m_width || y + height > m_height) {
    LOG_ERROR("Vulkan: texture update region lies outside the texture");
    return;
  }
  upload(x, y, width, height, pixels, /*fromUndefined=*/false);
}

void VulkanTexture::upload(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           const void *pixels, bool fromUndefined) {
  const size_t storedBpp = m_expandRgb ? 4 : m_cpuPixelBytes;
  const size_t bytes = static_cast<size_t>(w) * h * storedBpp;

  VulkanCore::Allocation staging;
  if (!m_core->createMappedBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  staging))
    return;
  if (m_expandRgb) {
    const auto *src = static_cast<const unsigned char *>(pixels);
    auto *dst = static_cast<unsigned char *>(staging.mapped);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
      dst[i * 4 + 0] = src[i * 3 + 0];
      dst[i * 4 + 1] = src[i * 3 + 1];
      dst[i * 4 + 2] = src[i * 3 + 2];
      dst[i * 4 + 3] = 255;
    }
  } else {
    std::memcpy(staging.mapped, pixels, bytes);
  }

  VkCommandBuffer cmd = m_core->beginOneShot();
  transitionImage(cmd, m_image,
                  fromUndefined ? VK_IMAGE_LAYOUT_UNDEFINED
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, m_mipLevels,
                  fromUndefined ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
                                : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  fromUndefined ? 0 : VK_ACCESS_2_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageOffset = {static_cast<int32_t>(x), static_cast<int32_t>(y), 0};
  region.imageExtent = {w, h, 1};
  vkCmdCopyBufferToImage(cmd, staging.buffer, m_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  const auto T = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  const auto W = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  const auto R = VK_ACCESS_2_TRANSFER_READ_BIT;
  const auto F = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  const auto S = VK_ACCESS_2_SHADER_READ_BIT;

  int32_t mw = static_cast<int32_t>(m_width);
  int32_t mh = static_cast<int32_t>(m_height);
  for (uint32_t i = 1; i < m_mipLevels; ++i) {
    transitionImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1, T, W, T, R);
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
    blit.srcOffsets[1] = {mw, mh, 1};
    mw = std::max(mw / 2, 1);
    mh = std::max(mh / 2, 1);
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
    blit.dstOffsets[1] = {mw, mh, 1};
    vkCmdBlitImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);
  }
  if (m_mipLevels > 1) {
    transitionImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, m_mipLevels - 1,
                    T, R, F, S);
  }
  transitionImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_mipLevels - 1, 1,
                  T, W, F, S);
  m_core->endOneShot(cmd);
  m_core->destroyAllocation(staging);
}

void VulkanTexture::bind() const {
  m_device->bindTexture(this);
}

void VulkanTexture::release() {
  if (m_image == VK_NULL_HANDLE)
    return;
  VulkanCore *core = m_core.get();
  VkImage image = m_image;
  VkDeviceMemory memory = m_memory;
  VkImageView view = m_view;
  VkSampler sampler = m_sampler;
  core->defer([core, image, memory, view, sampler]() {
    vkDestroySampler(core->device, sampler, nullptr);
    vkDestroyImageView(core->device, view, nullptr);
    vkDestroyImage(core->device, image, nullptr);
    vkFreeMemory(core->device, memory, nullptr);
  });
  m_image = VK_NULL_HANDLE;
  m_memory = VK_NULL_HANDLE;
  m_view = VK_NULL_HANDLE;
  m_sampler = VK_NULL_HANDLE;
}

// VulkanMesh

VulkanMesh::VulkanMesh(VulkanRenderDevice *device,
                       std::shared_ptr<VulkanCore> core)
    : m_device(device), m_core(std::move(core)) {
}

VulkanMesh::~VulkanMesh() {
  release();
}

void VulkanMesh::upload(const float *vertices, size_t vertexCount,
                        const uint32_t *indices, size_t indexCount,
                        const api::VertexLayout &layout) {
  release();
  m_layout = layout;
  m_indexCount = indexCount;
  m_vertices = std::make_unique<VulkanBuffer>(m_core);
  m_vertices->setData(vertices, vertexCount * layout.stride,
                      api::IBuffer::Usage::Static);
  m_indices = std::make_unique<VulkanBuffer>(m_core);
  m_indices->setData(indices, indexCount * sizeof(uint32_t),
                     api::IBuffer::Usage::Static);
}

void VulkanMesh::bind() const {
  m_device->bindMesh(this);
}

void VulkanMesh::release() {
  m_vertices.reset();
  m_indices.reset();
  m_indexCount = 0;
}

} // namespace componeng::renderer::vulkan
