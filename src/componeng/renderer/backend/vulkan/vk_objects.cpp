#include "componeng/renderer/backend/vulkan/vk_objects.hpp"

#include "componeng/utils/logger.hpp"

#include <algorithm>
#include <cstring>

namespace componeng::renderer::vulkan {

VkFormat toVkFormat(rhi::Format f) {
  switch (f) {
    case rhi::Format::R8:
      return VK_FORMAT_R8_UNORM;
    case rhi::Format::RG8:
      return VK_FORMAT_R8G8_UNORM;
    case rhi::Format::RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case rhi::Format::RGBA8Srgb:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case rhi::Format::BGRA8:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case rhi::Format::R16F:
      return VK_FORMAT_R16_SFLOAT;
    case rhi::Format::RG16F:
      return VK_FORMAT_R16G16_SFLOAT;
    case rhi::Format::RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case rhi::Format::R32F:
      return VK_FORMAT_R32_SFLOAT;
    case rhi::Format::RG32F:
      return VK_FORMAT_R32G32_SFLOAT;
    case rhi::Format::RGB32F:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case rhi::Format::RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhi::Format::D32F:
      return VK_FORMAT_D32_SFLOAT;
    case rhi::Format::Undefined:
      return VK_FORMAT_UNDEFINED;
  }
  return VK_FORMAT_UNDEFINED;
}

uint32_t bytesPerPixel(rhi::Format f) {
  switch (f) {
    case rhi::Format::R8:
      return 1;
    case rhi::Format::RG8:
    case rhi::Format::R16F:
      return 2;
    case rhi::Format::RGBA8:
    case rhi::Format::RGBA8Srgb:
    case rhi::Format::BGRA8:
    case rhi::Format::RG16F:
    case rhi::Format::R32F:
    case rhi::Format::D32F:
      return 4;
    case rhi::Format::RGBA16F:
    case rhi::Format::RG32F:
      return 8;
    case rhi::Format::RGB32F:
      return 12;
    case rhi::Format::RGBA32F:
      return 16;
    case rhi::Format::Undefined:
      return 0;
  }
  return 0;
}

namespace {

VkBufferUsageFlags toVkUsage(rhi::BufferUsage u) {
  VkBufferUsageFlags f =
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (hasFlag(u, rhi::BufferUsage::Vertex))
    f |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (hasFlag(u, rhi::BufferUsage::Index))
    f |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (hasFlag(u, rhi::BufferUsage::Uniform))
    f |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if (hasFlag(u, rhi::BufferUsage::Storage))
    f |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  if (hasFlag(u, rhi::BufferUsage::Indirect))
    f |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  return f;
}

VkDescriptorType toVk(rhi::BindingType t) {
  switch (t) {
    case rhi::BindingType::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case rhi::BindingType::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case rhi::BindingType::CombinedImageSampler:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case rhi::BindingType::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

VkShaderStageFlags toVk(rhi::StageFlags s) {
  VkShaderStageFlags f = 0;
  if (hasFlag(s, rhi::StageFlags::Vertex))
    f |= VK_SHADER_STAGE_VERTEX_BIT;
  if (hasFlag(s, rhi::StageFlags::Fragment))
    f |= VK_SHADER_STAGE_FRAGMENT_BIT;
  if (hasFlag(s, rhi::StageFlags::Compute))
    f |= VK_SHADER_STAGE_COMPUTE_BIT;
  return f;
}

VkFilter toVk(rhi::Filter f) {
  return f == rhi::Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerAddressMode toVk(rhi::WrapMode w) {
  switch (w) {
    case rhi::WrapMode::ClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case rhi::WrapMode::Repeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case rhi::WrapMode::MirroredRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

} // namespace

// VulkanBuffer

VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanCore> core,
                           const rhi::BufferDesc &desc)
    : m_core(std::move(core)), m_usage(toVkUsage(desc.usage)),
      m_size(std::max<uint64_t>(desc.size, 16)) {
  m_shadowed = desc.memory == rhi::MemoryKind::Upload;
  if (!m_core->createMappedBuffer(m_size, m_usage, m_current.alloc)) {
    m_size = 0;
    return;
  }
  std::memset(m_current.alloc.mapped, 0, static_cast<size_t>(m_size));
  if (m_shadowed) {
    m_shadow.assign(static_cast<size_t>(m_current.alloc.size), 0);
  }
}

VulkanBuffer::~VulkanBuffer() {
  destroyBacking(m_current);
  for (Backing &b : m_retired)
    destroyBacking(b);
}

void VulkanBuffer::destroyBacking(Backing &backing) {
  if (backing.alloc.buffer == VK_NULL_HANDLE &&
      backing.alloc.allocation == nullptr)
    return;
  // Frames still in flight may name it: free it once they are done.
  VulkanCore *core = m_core.get();
  Backing doomed = backing;
  core->defer(
      [core, doomed]() mutable { core->destroyAllocation(doomed.alloc); });
  backing = Backing{};
}

bool VulkanBuffer::prepareWrite() {
  Backing &cur = m_current;
  if (cur.alloc.buffer == VK_NULL_HANDLE)
    return false;
  const bool inFlight = cur.lastUse != 0 &&
                        cur.lastUse < m_core->recordingSerial &&
                        cur.lastUse > m_core->completedSerial;
  if (!inFlight)
    return false;
  if (!m_shadowed) {
    m_core->waitSerial(cur.lastUse); // no shadow to refill a swap from
    return false;
  }

  // A backing the GPU has finished with, or a new one.
  Backing next;
  for (size_t i = 0; i < m_retired.size(); ++i) {
    if (m_retired[i].lastUse <= m_core->completedSerial &&
        m_retired[i].alloc.size == cur.alloc.size) {
      next = m_retired[i];
      m_retired.erase(m_retired.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  if (next.alloc.buffer == VK_NULL_HANDLE &&
      !m_core->createMappedBuffer(cur.alloc.size, m_usage, next.alloc)) {
    m_core->waitSerial(cur.lastUse);
    return false;
  }
  next.lastUse = 0;
  m_retired.push_back(cur);
  m_current = next;
  ++m_generation;
  return true;
}

void VulkanBuffer::fill(uint64_t offset, const void *data, uint64_t bytes) {
  if (!m_current.alloc.mapped || offset + bytes > m_size) {
    LOG_ERROR("Vulkan: buffer write out of range (%llu + %llu > %llu)",
              static_cast<unsigned long long>(offset),
              static_cast<unsigned long long>(bytes),
              static_cast<unsigned long long>(m_size));
    return;
  }
  if (m_shadowed)
    std::memcpy(m_shadow.data() + offset, data, static_cast<size_t>(bytes));
  if (prepareWrite())
    std::memcpy(m_current.alloc.mapped, m_shadow.data(), m_shadow.size());
  else
    std::memcpy(static_cast<char *>(m_current.alloc.mapped) + offset, data,
                static_cast<size_t>(bytes));
}

void VulkanBuffer::write(uint64_t offset, const void *data, uint64_t bytes) {
  fill(offset, data, bytes);
}

// VulkanImage

VulkanImage::VulkanImage(std::shared_ptr<VulkanCore> core,
                         const rhi::ImageDesc &desc)
    : m_core(std::move(core)), m_desc(desc),
      m_vkFormat(toVkFormat(desc.format)),
      m_perFrame(desc.flags == rhi::ImageFlags::PerFrame) {
  const bool depth = hasFlag(desc.usage, rhi::ImageUsage::DepthAttachment);
  m_aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  const uint32_t w = desc.extent.width;
  const uint32_t h = desc.extent.height;

  VkFormatProperties fp;
  vkGetPhysicalDeviceFormatProperties(m_core->physical, m_vkFormat, &fp);
  const VkFormatFeatureFlags feats = fp.optimalTilingFeatures;
  m_canLinear =
      (feats & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
  const bool canBlit = m_canLinear &&
                       (feats & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                       (feats & VK_FORMAT_FEATURE_BLIT_DST_BIT);

  uint32_t full = 1;
  for (uint32_t m = std::max(w, h); m > 1; m >>= 1)
    ++full;
  m_mipLevels = desc.mipLevels == 0 ? full : std::min(desc.mipLevels, full);
  if (!canBlit && m_mipLevels > 1)
    m_mipLevels = 1;

  VkImageUsageFlags usage = 0;
  if (hasFlag(desc.usage, rhi::ImageUsage::Sampled))
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  if (hasFlag(desc.usage, rhi::ImageUsage::ColorAttachment))
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (depth)
    usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  if (hasFlag(desc.usage, rhi::ImageUsage::Storage))
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  if (hasFlag(desc.usage, rhi::ImageUsage::TransferSrc))
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (hasFlag(desc.usage, rhi::ImageUsage::TransferDst))
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  const uint32_t copies = m_perFrame ? kFramesInFlight : 1;
  for (uint32_t i = 0; i < copies; ++i) {
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_vkFormat;
    ii.extent = {w, h, 1};
    ii.mipLevels = m_mipLevels;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Attachments are large and long-lived: give them their own memory.
    const bool attachment =
        hasFlag(desc.usage, rhi::ImageUsage::ColorAttachment) || depth;
    if (!m_core->createImage(ii, attachment, m_slots[i].alloc)) {
      LOG_ERROR("Vulkan: failed to create a %ux%u image", w, h);
      return;
    }
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = m_slots[i].alloc.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_vkFormat;
    vi.subresourceRange = {m_aspect, 0, m_mipLevels, 0, 1};
    if (vkCreateImageView(m_core->device, &vi, nullptr, &m_slots[i].view) !=
        VK_SUCCESS)
      return;
  }
  m_valid = true;
}

VulkanImage::~VulkanImage() {
  VulkanCore *core = m_core.get();
  for (Slot &s : m_slots) {
    if (s.alloc.image == VK_NULL_HANDLE)
      continue;
    ImageAlloc image = s.alloc;
    VkImageView view = s.view;
    core->defer([core, image, view]() mutable {
      if (view)
        vkDestroyImageView(core->device, view, nullptr);
      core->destroyImage(image);
    });
  }
}

// VulkanSampler

VulkanSampler::VulkanSampler(std::shared_ptr<VulkanCore> core,
                             const rhi::SamplerDesc &desc)
    : m_core(std::move(core)), m_desc(desc) {
  m_sampler = create(false);
}

VkSampler VulkanSampler::create(bool nearestOnly) const {
  VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  si.magFilter = nearestOnly ? VK_FILTER_NEAREST : toVk(m_desc.magFilter);
  si.minFilter = nearestOnly ? VK_FILTER_NEAREST : toVk(m_desc.minFilter);
  si.mipmapMode = (!nearestOnly && m_desc.mipFilter == rhi::Filter::Linear)
                      ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                      : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  si.addressModeU = toVk(m_desc.wrapU);
  si.addressModeV = toVk(m_desc.wrapV);
  si.addressModeW = toVk(m_desc.wrapV);
  // The image view limits the mip chain.
  si.maxLod = VK_LOD_CLAMP_NONE;
  si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  if (!nearestOnly && m_core->anisotropy && m_desc.anisotropy > 1.0f) {
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy =
        std::min(m_desc.anisotropy, m_core->props.limits.maxSamplerAnisotropy);
  }
  VkSampler s = VK_NULL_HANDLE;
  vkCreateSampler(m_core->device, &si, nullptr, &s);
  return s;
}

VkSampler VulkanSampler::handle(bool linearFilterable) {
  const bool wantsLinear = m_desc.minFilter == rhi::Filter::Linear ||
                           m_desc.magFilter == rhi::Filter::Linear;
  if (linearFilterable || !wantsLinear)
    return m_sampler;
  if (m_nearest == VK_NULL_HANDLE)
    m_nearest = create(true);
  return m_nearest;
}

VulkanSampler::~VulkanSampler() {
  VulkanCore *core = m_core.get();
  VkSampler a = m_sampler;
  VkSampler b = m_nearest;
  core->defer([core, a, b]() {
    if (a)
      vkDestroySampler(core->device, a, nullptr);
    if (b)
      vkDestroySampler(core->device, b, nullptr);
  });
}

// VulkanShaderModule

VulkanShaderModule::VulkanShaderModule(std::shared_ptr<VulkanCore> core,
                                       const rhi::ShaderModuleDesc &desc)
    : m_core(std::move(core)), m_stage(desc.stage) {
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = desc.wordCount * sizeof(uint32_t);
  ci.pCode = desc.code;
  if (vkCreateShaderModule(m_core->device, &ci, nullptr, &m_module) !=
      VK_SUCCESS) {
    LOG_ERROR("Vulkan: vkCreateShaderModule failed");
    m_module = VK_NULL_HANDLE;
  }
}

VulkanShaderModule::~VulkanShaderModule() {
  // Only pipeline creation reads a module, and that is synchronous.
  if (m_module)
    vkDestroyShaderModule(m_core->device, m_module, nullptr);
}

// LayoutData

LayoutData::LayoutData(std::shared_ptr<VulkanCore> core,
                       const rhi::BindGroupLayoutDesc &desc)
    : m_core(std::move(core)), m_bindings(desc.bindings) {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  for (const rhi::BindingDesc &b : desc.bindings) {
    VkDescriptorSetLayoutBinding lb{};
    lb.binding = b.binding;
    lb.descriptorType = toVk(b.type);
    lb.descriptorCount = 1;
    lb.stageFlags = toVk(b.stages);
    bindings.push_back(lb);
  }
  VkDescriptorSetLayoutCreateInfo ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  ci.bindingCount = static_cast<uint32_t>(bindings.size());
  ci.pBindings = bindings.data();
  if (vkCreateDescriptorSetLayout(m_core->device, &ci, nullptr, &m_layout) !=
      VK_SUCCESS) {
    LOG_ERROR("Vulkan: vkCreateDescriptorSetLayout failed");
    m_layout = VK_NULL_HANDLE;
  }
}

LayoutData::~LayoutData() {
  VulkanCore *core = m_core.get();
  std::vector<VkDescriptorPool> pools = std::move(m_pools);
  VkDescriptorSetLayout layout = m_layout;
  core->defer([core, pools, layout]() {
    for (VkDescriptorPool p : pools)
      vkDestroyDescriptorPool(core->device, p, nullptr);
    if (layout)
      vkDestroyDescriptorSetLayout(core->device, layout, nullptr);
  });
}

VkDescriptorPool LayoutData::makePool() {
  constexpr uint32_t kGroupsPerPool = 64;
  const uint32_t sets = kGroupsPerPool * kFramesInFlight;
  std::vector<VkDescriptorPoolSize> sizes;
  for (const rhi::BindingDesc &b : m_bindings) {
    const VkDescriptorType type = toVk(b.type);
    auto it = std::find_if(sizes.begin(), sizes.end(),
                           [&](const auto &s) { return s.type == type; });
    if (it == sizes.end())
      sizes.push_back({type, sets});
    else
      it->descriptorCount += sets;
  }
  VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  ci.maxSets = sets;
  ci.poolSizeCount = static_cast<uint32_t>(sizes.size());
  ci.pPoolSizes = sizes.empty() ? nullptr : sizes.data();
  VkDescriptorPool pool = VK_NULL_HANDLE;
  if (vkCreateDescriptorPool(m_core->device, &ci, nullptr, &pool) != VK_SUCCESS)
    return VK_NULL_HANDLE;
  m_pools.push_back(pool);
  return pool;
}

bool LayoutData::allocate(std::array<VkDescriptorSet, kFramesInFlight> &sets,
                          VkDescriptorPool &pool) {
  std::array<VkDescriptorSetLayout, kFramesInFlight> layouts;
  layouts.fill(m_layout);
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (m_pools.empty() || attempt == 1) {
      if (!makePool())
        return false;
    }
    VkDescriptorPool candidate = m_pools.back();
    VkDescriptorSetAllocateInfo ai{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = candidate;
    ai.descriptorSetCount = kFramesInFlight;
    ai.pSetLayouts = layouts.data();
    const VkResult r =
        vkAllocateDescriptorSets(m_core->device, &ai, sets.data());
    if (r == VK_SUCCESS) {
      pool = candidate;
      return true;
    }
    if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL)
      return false;
  }
  return false;
}

void LayoutData::release(
    const std::array<VkDescriptorSet, kFramesInFlight> &sets,
    VkDescriptorPool pool) {
  VulkanCore *core = m_core.get();
  // The pool belongs to this layout, which the group keeps alive until here.
  core->defer([core, sets, pool]() {
    vkFreeDescriptorSets(core->device, pool, kFramesInFlight, sets.data());
  });
}

// VulkanBindGroup

VulkanBindGroup::VulkanBindGroup(std::shared_ptr<VulkanCore> core,
                                 std::shared_ptr<LayoutData> layout,
                                 const rhi::BindGroupDesc &desc)
    : m_core(std::move(core)), m_layout(std::move(layout)) {
  if (!m_layout->valid() || !m_layout->allocate(m_sets, m_pool)) {
    LOG_ERROR("Vulkan: could not allocate descriptor sets");
    return;
  }
  for (const rhi::BindGroupEntry &e : desc.entries) {
    Entry entry;
    entry.binding = e.binding;
    entry.buffer = static_cast<VulkanBuffer *>(e.buffer);
    entry.image = static_cast<VulkanImage *>(e.image);
    entry.sampler = static_cast<VulkanSampler *>(e.sampler);
    entry.offset = e.offset;
    entry.range = e.range;
    for (const rhi::BindingDesc &b : m_layout->bindings()) {
      if (b.binding == e.binding)
        entry.type = b.type;
    }
    m_entries.push_back(entry);
  }
  for (uint32_t slot = 0; slot < kFramesInFlight; ++slot)
    for (Entry &e : m_entries)
      write(slot, e);
  m_valid = true;
}

VulkanBindGroup::~VulkanBindGroup() {
  if (m_pool != VK_NULL_HANDLE)
    m_layout->release(m_sets, m_pool);
}

void VulkanBindGroup::write(uint32_t slot, Entry &e) {
  VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  w.dstSet = m_sets[slot];
  w.dstBinding = e.binding;
  w.descriptorCount = 1;
  w.descriptorType = toVk(e.type);
  VkDescriptorBufferInfo bi{};
  VkDescriptorImageInfo ii{};
  if (e.buffer) {
    uint64_t range = e.range ? e.range : e.buffer->size() - e.offset;
    if (e.type == rhi::BindingType::UniformBuffer)
      range =
          std::min<uint64_t>(range, m_core->props.limits.maxUniformBufferRange);
    bi = {e.buffer->handle(), e.offset, range};
    w.pBufferInfo = &bi;
    e.written[slot] = e.buffer->generation();
  } else if (e.image && e.sampler) {
    VulkanImage::Slot &s = e.image->slot(slot);
    ii = {e.sampler->handle(e.image->canLinearFilter()), s.view,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    w.pImageInfo = &ii;
  } else {
    return;
  }
  vkUpdateDescriptorSets(m_core->device, 1, &w, 0, nullptr);
}

VkDescriptorSet VulkanBindGroup::prepare(uint32_t slot) {
  for (Entry &e : m_entries) {
    if (!e.buffer)
      continue;
    if (e.written[slot] != e.buffer->generation())
      write(slot, e);
    e.buffer->markUsed(m_core->recordingSerial);
  }
  return m_sets[slot];
}

// VulkanGraphicsPipeline

VulkanGraphicsPipeline::VulkanGraphicsPipeline(
    std::shared_ptr<VulkanCore> core, const rhi::GraphicsPipelineDesc &desc)
    : m_core(std::move(core)) {
  std::vector<VkDescriptorSetLayout> setHandles;
  for (rhi::IBindGroupLayout *l : desc.setLayouts) {
    auto data = static_cast<VulkanBindGroupLayout *>(l)->data();
    setHandles.push_back(data->handle());
    m_setLayouts.push_back(std::move(data));
  }
  VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  li.setLayoutCount = static_cast<uint32_t>(setHandles.size());
  li.pSetLayouts = setHandles.data();
  if (vkCreatePipelineLayout(m_core->device, &li, nullptr, &m_layout) !=
      VK_SUCCESS) {
    m_layout = VK_NULL_HANDLE;
    return;
  }

  std::vector<VkVertexInputBindingDescription> bindings;
  for (const rhi::VertexBindingDesc &b : desc.vertexInput.bindings)
    bindings.push_back({b.binding, b.stride,
                        b.rate == rhi::VertexInputRate::PerInstance
                            ? VK_VERTEX_INPUT_RATE_INSTANCE
                            : VK_VERTEX_INPUT_RATE_VERTEX});
  std::vector<VkVertexInputAttributeDescription> attributes;
  for (const rhi::VertexAttributeDesc &a : desc.vertexInput.attributes)
    attributes.push_back(
        {a.location, a.binding, toVkFormat(a.format), a.offset});
  VkPipelineVertexInputStateCreateInfo vertexInput{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInput.vertexBindingDescriptionCount =
      static_cast<uint32_t>(bindings.size());
  vertexInput.pVertexBindingDescriptions = bindings.data();
  vertexInput.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributes.size());
  vertexInput.pVertexAttributeDescriptions = attributes.data();

  VkPipelineInputAssemblyStateCreateInfo assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  const rhi::RenderState &st = desc.state;
  VkPipelineRasterizationStateCreateInfo raster{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = st.cull == rhi::CullMode::Back    ? VK_CULL_MODE_BACK_BIT
                    : st.cull == rhi::CullMode::Front ? VK_CULL_MODE_FRONT_BIT
                                                      : VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // dynamic
  raster.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo msaa{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth.depthTestEnable = st.depthTest ? VK_TRUE : VK_FALSE;
  depth.depthWriteEnable = st.depthWrite ? VK_TRUE : VK_FALSE;
  depth.depthCompareOp =
      st.depthFunc == rhi::DepthFunc::LessEqual ? VK_COMPARE_OP_LESS_OR_EQUAL
      : st.depthFunc == rhi::DepthFunc::Equal   ? VK_COMPARE_OP_EQUAL
                                                : VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState blend{};
  blend.colorWriteMask =
      st.colorWrite ? (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
                    : 0;
  if (st.blend != rhi::BlendMode::None) {
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor =
        st.blend == rhi::BlendMode::Alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                                          : VK_BLEND_FACTOR_ONE;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
  }
  VkPipelineColorBlendStateCreateInfo blendState{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blendState.attachmentCount = 1;
  blendState.pAttachments = &blend;

  const VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR,
                                     VK_DYNAMIC_STATE_FRONT_FACE};
  VkPipelineDynamicStateCreateInfo dynamic{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic.dynamicStateCount = 3;
  dynamic.pDynamicStates = dynamics;

  std::vector<VkSpecializationMapEntry> specEntries;
  std::vector<uint32_t> specData;
  for (const rhi::SpecConstant &c : desc.specConstants) {
    specEntries.push_back(
        {c.id, static_cast<uint32_t>(specData.size() * sizeof(uint32_t)),
         sizeof(uint32_t)});
    specData.push_back(c.value);
  }
  VkSpecializationInfo spec{};
  spec.mapEntryCount = static_cast<uint32_t>(specEntries.size());
  spec.pMapEntries = specEntries.data();
  spec.dataSize = specData.size() * sizeof(uint32_t);
  spec.pData = specData.data();

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = static_cast<VulkanShaderModule *>(desc.vertex)->handle();
  stages[0].pName = "main";
  stages[0].pSpecializationInfo = specEntries.empty() ? nullptr : &spec;
  stages[1] = stages[0];
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = static_cast<VulkanShaderModule *>(desc.fragment)->handle();

  const VkFormat colorFormat = toVkFormat(desc.colorFormat);
  VkPipelineRenderingCreateInfo rendering{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &colorFormat;
  rendering.depthAttachmentFormat = toVkFormat(desc.depthFormat);

  VkGraphicsPipelineCreateInfo pi{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pi.pNext = &rendering;
  pi.stageCount = 2;
  pi.pStages = stages;
  pi.pVertexInputState = &vertexInput;
  pi.pInputAssemblyState = &assembly;
  pi.pViewportState = &viewport;
  pi.pRasterizationState = &raster;
  pi.pMultisampleState = &msaa;
  pi.pDepthStencilState = &depth;
  pi.pColorBlendState = &blendState;
  pi.pDynamicState = &dynamic;
  pi.layout = m_layout;
  if (vkCreateGraphicsPipelines(m_core->device, VK_NULL_HANDLE, 1, &pi, nullptr,
                                &m_pipeline) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: pipeline creation failed");
    m_pipeline = VK_NULL_HANDLE;
  }
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
  VulkanCore *core = m_core.get();
  VkPipeline pipeline = m_pipeline;
  VkPipelineLayout layout = m_layout;
  core->defer([core, pipeline, layout]() {
    if (pipeline)
      vkDestroyPipeline(core->device, pipeline, nullptr);
    if (layout)
      vkDestroyPipelineLayout(core->device, layout, nullptr);
  });
}

} // namespace componeng::renderer::vulkan
