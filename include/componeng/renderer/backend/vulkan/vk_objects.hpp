#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/backend/vulkan/vk_core.hpp"

#include <array>
#include <memory>
#include <vector>

namespace componeng::renderer::vulkan {

class VulkanDevice;

VkFormat toVkFormat(rhi::Format f);
uint32_t bytesPerPixel(rhi::Format f);

/**
 * Host-visible buffer written by plain memcpy. The GPU may still be reading
 * the previous frame, so a buffer remembers the last frame that bound it
 * (markUsed). A write arriving while that frame is in flight either
 *  - renames (Upload buffers): swaps to another backing allocation, refilled
 *    from a CPU shadow copy, and retires the old one until its frame ends; or
 *  - waits (GpuOnly buffers: no shadow, written rarely).
 * So buffers rewritten every frame never stall the CPU. Each swap bumps
 * generation(), which bind groups compare to know a descriptor went stale.
 */
class VulkanBuffer final : public rhi::IBuffer {
public:
  VulkanBuffer(std::shared_ptr<VulkanCore> core, const rhi::BufferDesc &desc);
  ~VulkanBuffer() override;

  uint64_t size() const override {
    return m_size;
  }
  void write(uint64_t offset, const void *data, uint64_t bytes) override;

  /** Like write, for any memory kind (IDevice::uploadBuffer). */
  void fill(uint64_t offset, const void *data, uint64_t bytes);

  /** The backing the frame being recorded should bind. Changes when a write
   *  renames the buffer. */
  VkBuffer handle() const {
    return m_current.alloc.buffer;
  }
  uint64_t generation() const {
    return m_generation;
  }
  /** Record that the frame being recorded (`serial`) reads this buffer. */
  void markUsed(uint64_t serial) const {
    m_current.lastUse = serial;
  }

private:
  struct Backing {
    VulkanCore::Allocation alloc;
    uint64_t lastUse = 0; // last frame serial that bound it
  };

  void destroyBacking(Backing &backing);
  /** Make the backing writable. Returns true if it was swapped (refill it from
   *  the shadow), false if it can be written in place, possibly after a wait.
   */
  bool prepareWrite();

  std::shared_ptr<VulkanCore> m_core;
  VkBufferUsageFlags m_usage = 0;
  mutable Backing m_current;
  std::vector<Backing> m_retired;
  std::vector<unsigned char> m_shadow; // contents, when the buffer is Upload
  bool m_shadowed = false;
  uint64_t m_size = 0;
  uint64_t m_generation = 1;
};

/**
 * A 2D image. A PerFrame image keeps one copy per frame in flight and the
 * device picks the current one; each copy tracks its own layout.
 */
class VulkanImage final : public rhi::IImage {
public:
  struct Slot {
    ImageAlloc alloc;
    VkImageView view = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The frame (VulkanCore::recordingSerial) that last used this copy as an
    // attachment; a new frame starts it from an undefined layout.
    uint64_t frameSerial = 0;
  };

  VulkanImage(std::shared_ptr<VulkanCore> core, const rhi::ImageDesc &desc);
  ~VulkanImage() override;

  bool valid() const {
    return m_valid;
  }
  rhi::Extent2D extent() const override {
    return m_desc.extent;
  }
  rhi::Format format() const override {
    return m_desc.format;
  }
  uint32_t mipLevels() const override {
    return m_mipLevels;
  }

  const rhi::ImageDesc &desc() const {
    return m_desc;
  }
  VkFormat vkFormat() const {
    return m_vkFormat;
  }
  VkImageAspectFlags aspect() const {
    return m_aspect;
  }
  bool perFrame() const {
    return m_perFrame;
  }
  bool canLinearFilter() const {
    return m_canLinear;
  }
  Slot &slot(uint32_t frameSlot) {
    return m_slots[m_perFrame ? frameSlot : 0];
  }

private:
  std::shared_ptr<VulkanCore> m_core;
  rhi::ImageDesc m_desc;
  VkFormat m_vkFormat = VK_FORMAT_UNDEFINED;
  VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  uint32_t m_mipLevels = 1;
  bool m_perFrame = false;
  bool m_canLinear = false;
  bool m_valid = false;
  std::array<Slot, kFramesInFlight> m_slots;
};

class VulkanSampler final : public rhi::ISampler {
public:
  VulkanSampler(std::shared_ptr<VulkanCore> core, const rhi::SamplerDesc &desc);
  ~VulkanSampler() override;

  /** The sampler to bind with an image; a format that cannot be filtered
   *  linearly gets a nearest copy, made on first need. */
  VkSampler handle(bool linearFilterable);

private:
  VkSampler create(bool nearestOnly) const;

  std::shared_ptr<VulkanCore> m_core;
  rhi::SamplerDesc m_desc;
  VkSampler m_sampler = VK_NULL_HANDLE;
  VkSampler m_nearest = VK_NULL_HANDLE;
};

class VulkanShaderModule final : public rhi::IShaderModule {
public:
  VulkanShaderModule(std::shared_ptr<VulkanCore> core,
                     const rhi::ShaderModuleDesc &desc);
  ~VulkanShaderModule() override;

  VkShaderModule handle() const {
    return m_module;
  }
  rhi::ShaderStage stage() const {
    return m_stage;
  }

private:
  std::shared_ptr<VulkanCore> m_core;
  VkShaderModule m_module = VK_NULL_HANDLE;
  rhi::ShaderStage m_stage;
};

/** What a layout's groups share. Kept alive by every group and pipeline that
 *  uses it, so it outlives them in any teardown order. */
class LayoutData {
public:
  LayoutData(std::shared_ptr<VulkanCore> core,
             const rhi::BindGroupLayoutDesc &desc);
  ~LayoutData();

  bool valid() const {
    return m_layout != VK_NULL_HANDLE;
  }
  VkDescriptorSetLayout handle() const {
    return m_layout;
  }
  const std::vector<rhi::BindingDesc> &bindings() const {
    return m_bindings;
  }

  /** One descriptor set per frame slot, from a pool this layout owns. */
  bool allocate(std::array<VkDescriptorSet, kFramesInFlight> &sets,
                VkDescriptorPool &pool);
  /** Gives the sets back once the frames that may use them are done. */
  void release(const std::array<VkDescriptorSet, kFramesInFlight> &sets,
               VkDescriptorPool pool);

private:
  VkDescriptorPool makePool();

  std::shared_ptr<VulkanCore> m_core;
  VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
  std::vector<rhi::BindingDesc> m_bindings;
  std::vector<VkDescriptorPool> m_pools;
};

class VulkanBindGroupLayout final : public rhi::IBindGroupLayout {
public:
  VulkanBindGroupLayout(std::shared_ptr<VulkanCore> core,
                        const rhi::BindGroupLayoutDesc &desc)
      : m_data(std::make_shared<LayoutData>(std::move(core), desc)) {
  }
  const std::shared_ptr<LayoutData> &data() const {
    return m_data;
  }

private:
  std::shared_ptr<LayoutData> m_data;
};

/**
 * A group owns one descriptor set per frame slot. `prepare(slot)` rewrites the
 * entries whose buffer renamed since that slot's set was written; the slot's
 * previous frame has finished by then, so no set the GPU reads is touched.
 */
class VulkanBindGroup final : public rhi::IBindGroup {
public:
  VulkanBindGroup(std::shared_ptr<VulkanCore> core,
                  std::shared_ptr<LayoutData> layout,
                  const rhi::BindGroupDesc &desc);
  ~VulkanBindGroup() override;

  bool valid() const {
    return m_valid;
  }
  const std::shared_ptr<LayoutData> &layout() const {
    return m_layout;
  }

  /** Brings `slot`'s set up to date and marks the buffers used by the frame
   *  being recorded. Returns the set to bind. */
  VkDescriptorSet prepare(uint32_t slot);

private:
  struct Entry {
    uint32_t binding = 0;
    rhi::BindingType type = rhi::BindingType::UniformBuffer;
    class VulkanBuffer *buffer = nullptr;
    class VulkanImage *image = nullptr;
    class VulkanSampler *sampler = nullptr;
    uint64_t offset = 0;
    uint64_t range = 0;
    std::array<uint64_t, kFramesInFlight> written{};
  };

  void write(uint32_t slot, Entry &entry);

  std::shared_ptr<VulkanCore> m_core;
  std::shared_ptr<LayoutData> m_layout;
  std::vector<Entry> m_entries;
  std::array<VkDescriptorSet, kFramesInFlight> m_sets{};
  VkDescriptorPool m_pool = VK_NULL_HANDLE;
  bool m_valid = false;
};

class VulkanGraphicsPipeline final : public rhi::IGraphicsPipeline {
public:
  VulkanGraphicsPipeline(std::shared_ptr<VulkanCore> core,
                         const rhi::GraphicsPipelineDesc &desc);
  ~VulkanGraphicsPipeline() override;

  bool valid() const {
    return m_pipeline != VK_NULL_HANDLE;
  }
  VkPipeline handle() const {
    return m_pipeline;
  }
  VkPipelineLayout layout() const {
    return m_layout;
  }
  const std::vector<std::shared_ptr<LayoutData>> &setLayouts() const {
    return m_setLayouts;
  }

private:
  std::shared_ptr<VulkanCore> m_core;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_layout = VK_NULL_HANDLE;
  std::vector<std::shared_ptr<LayoutData>> m_setLayouts;
};

} // namespace componeng::renderer::vulkan
