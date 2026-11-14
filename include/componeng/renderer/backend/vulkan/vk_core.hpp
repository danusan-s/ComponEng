#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace componeng::renderer::vulkan {

/**
 * The Vulkan objects every resource needs, shared (shared_ptr) by the device
 * and all its resources so the VkDevice outlives them in any teardown order.
 */
struct VulkanCore {
  VulkanCore() = default;
  ~VulkanCore();
  VulkanCore(const VulkanCore &) = delete;
  VulkanCore &operator=(const VulkanCore &) = delete;

  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t queueFamily = 0;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties props{};
  VkPhysicalDeviceMemoryProperties memProps{};
  bool anisotropy = false;

  /** Index of a memory type in `typeBits` with all of `flags`, or UINT32_MAX. */
  uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) const;

  /** A mapped, host-visible buffer. Prefers device-local host-visible memory
   *  (resizable BAR) and falls back to plain host-visible. */
  struct Allocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    VkDeviceSize size = 0;
  };
  bool createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          Allocation &out) const;
  void destroyAllocation(Allocation &a) const;

  /** Runs `fn` once the GPU is known to be done with the current frame. */
  void defer(std::function<void()> fn);
  void flushDeferred();

  /** Record and submit commands now, blocking until they finish. */
  VkCommandBuffer beginOneShot() const;
  void endOneShot(VkCommandBuffer cmd) const;

private:
  std::vector<std::function<void()>> m_deferred;
};

/** sync2 barrier over `levelCount` mips of a colour image. */
void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout from,
                     VkImageLayout to, uint32_t baseMip, uint32_t levelCount,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                     VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
                     VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

} // namespace componeng::renderer::vulkan
