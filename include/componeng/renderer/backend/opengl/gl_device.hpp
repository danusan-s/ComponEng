#pragma once

#include "componeng/renderer/backend/api/rhi/device.hpp"
#include "componeng/renderer/backend/opengl/gl_objects.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace componeng::renderer::opengl {

/**
 * OpenGL 4.6 core implementation of rhi::IDevice: an emulation of the explicit
 * API over GL state. A pipeline is a linked SPIR-V program, a VAO and a baked
 * RenderState; a bind group is a set of GL buffer bindings and texture units;
 * a rendering is an FBO plus viewport, scissor and clears. Matrices stay GL
 * shaped (y up) because glClipControl switches the depth range to zero-to-one,
 * and render targets are bottom-up by construction, like GL always is.
 */
class GLDevice final : public rhi::IDevice {
public:
  GLDevice() = default;
  ~GLDevice() override;

  GLDevice(const GLDevice &) = delete;
  GLDevice &operator=(const GLDevice &) = delete;

  void init(void *windowHandle) override;
  void shutdown() override;

  rhi::Backend backend() const override {
    return rhi::Backend::OpenGL;
  }
  rhi::Format backbufferFormat() const override {
    return rhi::Format::RGBA8;
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

  int checkErrors() const override;

  void beginGpuTimer() override;
  void endGpuTimer() override;
  double lastGpuTimeMs() const override {
    return m_gpuTimeMs;
  }
  void beginGpuPass(api::PassId pass) override;
  void endGpuPass(api::PassId pass) override;
  double lastGpuPassTimeMs(api::PassId pass) const override {
    return m_gpuPasses[static_cast<std::size_t>(pass)].ms;
  }
  void beginShaderTimer(uint64_t shaderId) override;
  void endShaderTimer(uint64_t shaderId) override;
  double shaderTimeMs(uint64_t shaderId) const override;

private:
  /** Applies the whole baked state; pipeline binds are not frequent enough
   *  for a per-field diff, and ImGui leaves arbitrary state behind. */
  void applyState(const rhi::RenderState &state);
  void applyViewport(const rhi::Rect &rect);
  void clearTarget(GLuint fbo, const rhi::RenderingDesc &desc);

  void collectGpuPasses();
  void collectShaderTimers();

  static void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
                                     GLenum severity, GLsizei length,
                                     const GLchar *message,
                                     const void *userParam);

  GLFWwindow *m_window = nullptr;
  GLuint m_fbo = 0;
  GLuint m_fboColor = 0;
  GLuint m_fboDepth = 0;
  bool m_frameOpen = false;

  rhi::Rect m_windowViewport{};

  GLGraphicsPipeline *m_pipeline = nullptr;
  GLuint m_vao = 0;
  GLenum m_indexType = GL_UNSIGNED_INT;
  uint64_t m_indexOffset = 0;

  // Vertex and index buffers are command state, but GL stores them in the VAO,
  // and every pipeline owns a VAO. Keep the command state here and replay it
  // when the pipeline (and so the VAO) changes.
  struct VertexBindingState {
    GLuint buffer = 0;
    uint64_t offset = 0;
  };
  std::array<VertexBindingState, 16> m_vertexBuffers{};
  GLuint m_indexBuffer = 0;

  mutable std::atomic<int> m_errors{0};

  // COMPONENG_GL_DUMP_FRAME=<file.ppm>: write the backbuffer of the 120th
  // frame (before the swap) and carry on, for checking that a change draws.
  const char *m_dumpPath = nullptr;
  uint32_t m_frameCount = 0;

  // Whole-frame timer: one GL_TIME_ELAPSED query per slot.
  static constexpr int kGpuQueryCount = 4;
  GLuint m_gpuQueries[kGpuQueryCount] = {};
  bool m_gpuPending[kGpuQueryCount] = {};
  int m_gpuNext = 0;
  bool m_gpuSpanOpen = false;
  bool m_gpuCreated = false;
  double m_gpuTimeMs = -1.0;

  // Per-pass timestamps: GL_TIME_ELAPSED can only be active once, and the
  // frame timer holds it.
  struct GpuPassTimer {
    GLuint queries[kGpuQueryCount][2] = {};
    bool pending[kGpuQueryCount] = {};
    int next = 0;
    bool open = false;
    bool created = false;
    double ms = -1.0;
  };
  GpuPassTimer m_gpuPasses[api::kPassCount];

  // Per-shader spans, keyed by the Shader asset's id. Same timestamp scheme;
  // a slot's result is only read once the GPU reports it available.
  struct ShaderTimer {
    std::array<std::vector<GLuint>, kGpuQueryCount> queries;
    std::array<int, kGpuQueryCount> spans{};
    std::array<bool, kGpuQueryCount> pending{};
    int current = 0;
    bool open = false;
    double ms = -1.0;
  };
  std::unordered_map<uint64_t, ShaderTimer> m_shaderTimers;
};

} // namespace componeng::renderer::opengl
