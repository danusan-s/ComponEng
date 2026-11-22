#pragma once

#include "componeng/renderer/backend/api/pass_id.hpp"
#include "componeng/renderer/backend/api/rhi/resources.hpp"

#include <memory>

namespace componeng::renderer::rhi {

enum class LoadOp : uint8_t { Load, Clear, DontCare };
enum class StoreOp : uint8_t { Store, DontCare };

struct ColorAttachment {
  // nullptr: the window's backbuffer.
  IImage *image = nullptr;
  LoadOp load = LoadOp::Clear;
  StoreOp store = StoreOp::Store;
  float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct DepthAttachment {
  // nullptr: the window's depth buffer (only with a nullptr colour image).
  IImage *image = nullptr;
  LoadOp load = LoadOp::Clear;
  StoreOp store = StoreOp::Store;
  float clearDepth = 1.0f;
};

struct RenderingDesc {
  ColorAttachment color;
  DepthAttachment depth;
  bool hasDepth = true;
};

/** What a barrier makes visible to what. Image layouts of attachments and
 *  sampled images are not declared: the backend derives them from
 *  beginRendering and bindGroup. These are for storage writes. */
enum class Access : uint8_t {
  ShaderStorageWrite,
  ShaderStorageRead,
  Uniform,
  VertexAttribute,
  Index,
  IndirectCommand,
  TransferWrite,
  TransferRead,
};

/** Layout of one drawIndexedIndirect command; the same on both APIs. */
struct DrawIndexedIndirectCommand {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;
};

/**
 * @brief The explicit rendering device: resources, pipelines, bind groups and
 * recorded commands. Single-threaded; objects come from the factories and the
 * device must outlive them.
 */
class IDevice {
public:
  virtual ~IDevice() = default;

  /** windowHandle is a GLFWwindow*. */
  virtual void init(void *windowHandle) = 0;

  /** Release objects that must go before the window (swapchain, surface). */
  virtual void shutdown() {
  }

  virtual Backend backend() const = 0;

  /** The colour format of the window's backbuffer. A pipeline drawn into the
   *  backbuffer is created with it. */
  virtual Format backbufferFormat() const = 0;

  /** True between a beginFrame that returned true and present(). A system that
   *  draws checks it, so a skipped frame (minimised window) costs nothing. */
  virtual bool frameOpen() const = 0;

  /** Waits for the frame slot and acquires the backbuffer. False means skip
   *  this frame (minimised window, swapchain rebuilding). Resource creation
   *  and uploads are allowed outside a frame. */
  virtual bool beginFrame() = 0;

  virtual void present(void *windowHandle) = 0;

  /** The window area drawn into (the letterbox). A rendering on the
   *  backbuffer starts with viewport and scissor set to it; one on an image
   *  starts with the whole image. */
  virtual void setWindowViewport(int x, int y, int w, int h) = 0;
  virtual Extent2D windowViewportExtent() const = 0;

  virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc &) = 0;
  virtual std::unique_ptr<IImage> createImage(const ImageDesc &) = 0;
  virtual std::unique_ptr<ISampler> createSampler(const SamplerDesc &) = 0;
  virtual std::unique_ptr<IShaderModule>
  createShaderModule(const ShaderModuleDesc &) = 0;
  virtual std::unique_ptr<IBindGroupLayout>
  createBindGroupLayout(const BindGroupLayoutDesc &) = 0;
  virtual std::unique_ptr<IBindGroup>
  createBindGroup(const BindGroupDesc &) = 0;
  virtual std::unique_ptr<IGraphicsPipeline>
  createGraphicsPipeline(const GraphicsPipelineDesc &) = 0;
  virtual std::unique_ptr<IComputePipeline>
  createComputePipeline(const ComputePipelineDesc &) = 0;

  /** Recorded into the open frame, or a blocking one-shot submit when no
   *  frame is open. */
  virtual void uploadBuffer(IBuffer &dst, uint64_t offset, const void *data,
                            uint64_t bytes) = 0;

  /** Fills `region` of mip 0, then rebuilds the mip chain if the image has
   *  one. */
  virtual void uploadImage(IImage &dst, const Rect &region,
                           const void *pixels) = 0;

  /** Commands below are valid between beginFrame and present. */
  virtual void beginRendering(const RenderingDesc &) = 0;
  virtual void endRendering() = 0;
  virtual void setViewport(const Rect &) = 0;
  virtual void setScissor(const Rect &) = 0;

  virtual void bindPipeline(IGraphicsPipeline &) = 0;
  virtual void bindGroup(uint32_t set, IBindGroup &) = 0;
  virtual void bindVertexBuffer(uint32_t slot, IBuffer &,
                                uint64_t byteOffset) = 0;
  virtual void bindIndexBuffer(IBuffer &, uint64_t byteOffset, IndexType) = 0;
  virtual void draw(uint32_t vertexCount, uint32_t instanceCount,
                    uint32_t firstVertex, uint32_t firstInstance) = 0;
  virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                           uint32_t firstIndex, int32_t vertexOffset,
                           uint32_t firstInstance) = 0;
  virtual void drawIndexedIndirect(IBuffer &commands, uint64_t offset,
                                   uint32_t drawCount, uint32_t stride) = 0;

  virtual void bindComputePipeline(IComputePipeline &) = 0;
  virtual void dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
  virtual void memoryBarrier(Access before, Access after) = 0;
  virtual void copyBuffer(IBuffer &src, IBuffer &dst, uint64_t srcOffset,
                          uint64_t dstOffset, uint64_t bytes) = 0;

  /** Dear ImGui hooks; the defaults do nothing. imguiRender draws onto the
   *  backbuffer in its own rendering, just before present(). */
  virtual void initImGui(void * /*windowHandle*/) {
  }
  virtual void imguiNewFrame() {
  }
  virtual void imguiRender() {
  }
  virtual void shutdownImGui() {
  }

  /** Reports and counts API errors seen since the last call (GL_KHR_debug,
   *  validation layers). */
  virtual int checkErrors() const = 0;

  /** GPU timing, optional like the old device's: the defaults do nothing and
   *  report "unavailable". Results arrive a few frames late. */
  virtual void beginGpuTimer() {
  }
  virtual void endGpuTimer() {
  }
  virtual double lastGpuTimeMs() const {
    return -1.0;
  }
  virtual void beginGpuPass(api::PassId) {
  }
  virtual void endGpuPass(api::PassId) {
  }
  virtual double lastGpuPassTimeMs(api::PassId) const {
    return -1.0;
  }

  /** Per-shader spans, keyed by an id the Shader asset owns. */
  virtual void beginShaderTimer(uint64_t /*shaderId*/) {
  }
  virtual void endShaderTimer(uint64_t /*shaderId*/) {
  }
  virtual double shaderTimeMs(uint64_t /*shaderId*/) const {
    return -1.0;
  }
};

} // namespace componeng::renderer::rhi
