#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/backend/vulkan/vk_core.hpp"
#include "componeng/renderer/backend/vulkan/vk_gpu_timer.hpp"
#include "componeng/renderer/backend/vulkan/vk_objects.hpp"

#include <atomic>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace componeng::renderer::vulkan {

/**
 * Vulkan 1.3 implementation of rhi::IDevice, using dynamic rendering (no
 * render passes). Up to kFramesInFlight frames are recorded ahead of the GPU
 * on one thread; each frame slot owns a command buffer, and every bind group
 * owns a descriptor set per slot. Buffers the CPU rewrites track the last
 * frame that read them (VulkanBuffer::markUsed), so rewriting one every frame
 * never stalls. Descriptor sets are bound lazily at each draw against the
 * bound pipeline's layout. See dev_docs/upgrade_opengl for the design.
 *
 * Window conventions stay in here: the window is drawn with a negative-height
 * viewport so GL-style matrices come out upright, and an off-screen image is
 * drawn unflipped (row 0 at the bottom, as in GL) with a dynamic clockwise
 * front face to match.
 */
class VulkanDevice final : public rhi::IDevice {
public:
  VulkanDevice() = default;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;

  void init(void *windowHandle) override;
  void shutdown() override;

  rhi::Backend backend() const override {
    return rhi::Backend::Vulkan;
  }
  rhi::Format backbufferFormat() const override {
    return m_swapFormat == VK_FORMAT_R8G8B8A8_UNORM ? rhi::Format::RGBA8
                                                    : rhi::Format::BGRA8;
  }
  bool frameOpen() const override {
    return m_frameOpen;
  }
  bool beginFrame() override;
  void present(void *windowHandle) override;

  void setWindowViewport(int x, int y, int w, int h) override;
  rhi::Extent2D windowViewportExtent() const override;

  std::unique_ptr<rhi::IBuffer> createBuffer(const rhi::BufferDesc &) override;
  std::unique_ptr<rhi::IImage> createImage(const rhi::ImageDesc &) override;
  std::unique_ptr<rhi::ISampler>
  createSampler(const rhi::SamplerDesc &) override;
  std::unique_ptr<rhi::IShaderModule>
  createShaderModule(const rhi::ShaderModuleDesc &) override;
  std::unique_ptr<rhi::IBindGroupLayout>
  createBindGroupLayout(const rhi::BindGroupLayoutDesc &) override;
  std::unique_ptr<rhi::IBindGroup>
  createBindGroup(const rhi::BindGroupDesc &) override;
  std::unique_ptr<rhi::IGraphicsPipeline>
  createGraphicsPipeline(const rhi::GraphicsPipelineDesc &) override;
  std::unique_ptr<rhi::IComputePipeline>
  createComputePipeline(const rhi::ComputePipelineDesc &) override;

  void uploadBuffer(rhi::IBuffer &dst, uint64_t offset, const void *data,
                    uint64_t bytes) override;
  void uploadImage(rhi::IImage &dst, const rhi::Rect &region,
                   const void *pixels) override;

  void beginRendering(const rhi::RenderingDesc &) override;
  void endRendering() override;
  void setViewport(const rhi::Rect &) override;
  void setScissor(const rhi::Rect &) override;

  void bindPipeline(rhi::IGraphicsPipeline &) override;
  void bindGroup(uint32_t set, rhi::IBindGroup &) override;
  void bindVertexBuffer(uint32_t slot, rhi::IBuffer &,
                        uint64_t byteOffset) override;
  void bindIndexBuffer(rhi::IBuffer &, uint64_t byteOffset,
                       rhi::IndexType) override;
  void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance) override;
  void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance) override;
  void drawIndexedIndirect(rhi::IBuffer &commands, uint64_t offset,
                           uint32_t drawCount, uint32_t stride) override;

  void bindComputePipeline(rhi::IComputePipeline &) override;
  void dispatch(uint32_t x, uint32_t y, uint32_t z) override;
  void memoryBarrier(rhi::Access before, rhi::Access after) override;
  void copyBuffer(rhi::IBuffer &src, rhi::IBuffer &dst, uint64_t srcOffset,
                  uint64_t dstOffset, uint64_t bytes) override;

  void initImGui(void *windowHandle) override;
  void imguiNewFrame() override;
  void imguiRender() override;
  void shutdownImGui() override;

  int checkErrors() const override {
    return m_errors.exchange(0);
  }
  void countError() {
    ++m_errors;
  }

  void beginGpuTimer() override {
    m_timer.beginFrameSpan();
  }
  void endGpuTimer() override {
    m_timer.endFrameSpan();
  }
  double lastGpuTimeMs() const override {
    return m_timer.frameMs();
  }
  void beginGpuPass(api::PassId pass) override {
    if (m_timer.supported() && ensureCommandBuffer())
      m_timer.beginPass(pass);
  }
  void endGpuPass(api::PassId pass) override {
    m_timer.endPass(pass);
  }
  double lastGpuPassTimeMs(api::PassId pass) const override {
    return m_timer.passMs(pass);
  }
  void beginShaderTimer(uint64_t shaderId) override {
    if (m_timer.supported() && ensureCommandBuffer())
      m_timer.beginShader(shaderId);
  }
  void endShaderTimer(uint64_t shaderId) override {
    m_timer.endShader(shaderId);
  }
  double shaderTimeMs(uint64_t shaderId) const override {
    return m_timer.shaderMs(shaderId);
  }

private:
  bool createInstance();
  bool pickPhysicalDevice();
  bool createDevice();
  bool createSwapchain();
  void destroySwapchain(bool keepHandle);
  bool createDepthImage();
  void createFrameObjects();
  /** Pick up frames the GPU has finished: free what they held, read timings. */
  void pollCompletedFrames();

  /** Opens the frame slot's command buffer (waiting for the frame that last
   *  used it); uploads and timers may need it before beginFrame. */
  bool ensureCommandBuffer();
  /** The frame's command buffer for a transfer that reads `stagingBytes` of
   *  staging memory, or null when the transfer must be a blocking one-shot
   *  (a rendering is open, or too much staging is queued). */
  VkCommandBuffer beginTransfer(size_t stagingBytes);
  struct UploadScope;

  void transition(VulkanImage &image, uint32_t slot, VkImageLayout to);
  void beginRenderingOn(const rhi::RenderingDesc &desc);
  void applyViewport(const rhi::Rect &rect);
  /** Binds the sets the pipeline needs and returns true if a draw can go. */
  bool prepareDraw(const char *what);

  std::shared_ptr<VulkanCore> m_core;
  GLFWwindow *m_window = nullptr;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;

  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  VkFormat m_swapFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D m_extent{};
  std::vector<VkImage> m_images;
  std::vector<VkImageView> m_imageViews;
  std::vector<VkSemaphore> m_renderDone;
  VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
  // One depth image per frame slot: with a single shared one, frame N+1's depth
  // tests would have to wait for frame N's, serialising the two on the GPU.
  struct DepthTarget {
    ImageAlloc image;
    VkImageView view = VK_NULL_HANDLE;
  };
  DepthTarget m_depth[kFramesInFlight];
  bool m_swapDirty = false;

  struct FrameSlot {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    uint64_t serial = 0; // the frame last submitted from this slot (0: none)
  };
  FrameSlot m_slots[kFramesInFlight];
  uint32_t m_slot = 0; // slot of the frame being recorded / recorded next

  VkCommandBuffer m_cmd = VK_NULL_HANDLE;
  uint32_t m_imageIndex = 0;
  bool m_cmdOpen = false;
  bool m_frameOpen = false;
  bool m_rendering = false;
  size_t m_stagingBytes = 0; // staging queued into the current command buffer

  // The open rendering: the image drawn into (null: the backbuffer) and the
  // attachments that need a layout change when it ends.
  VulkanImage *m_targetColor = nullptr;
  VulkanImage *m_targetDepth = nullptr;
  VkExtent2D m_renderExtent{};

  rhi::Rect m_windowViewport{};
  bool m_windowViewportSet = false;

  // What is bound. Sets are applied at the draw, because Vulkan needs the
  // pipeline layout to bind one.
  static constexpr uint32_t kMaxSets = 2;
  VulkanGraphicsPipeline *m_pipeline = nullptr;
  VulkanBindGroup *m_groups[kMaxSets] = {};
  VkDescriptorSet m_applied[kMaxSets] = {};
  bool m_warnedUnbound = false;

  // COMPONENG_VK_DUMP_FRAME=<file.ppm>: write the backbuffer of the 120th
  // frame (our own swapchain image, not the screen) and carry on. A debug aid
  // for checking a change draws something.
  const char *m_dumpPath = nullptr;
  uint32_t m_frameCount = 0;

  VulkanGpuTimer m_timer;
  bool m_imguiReady = false;
  bool m_shutDown = false;
  mutable std::atomic<int> m_errors{0};
};

} // namespace componeng::renderer::vulkan
