#include "componeng/renderer/backend/opengl/gl_device.hpp"

#include "componeng/utils/logger.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace componeng::renderer::opengl {

namespace {

GLenum depthFuncOf(rhi::DepthFunc func) {
  switch (func) {
    case rhi::DepthFunc::LessEqual:
      return GL_LEQUAL;
    case rhi::DepthFunc::Equal:
      return GL_EQUAL;
    case rhi::DepthFunc::Less:
      break;
  }
  return GL_LESS;
}

GLbitfield barrierBits(rhi::Access access) {
  switch (access) {
    case rhi::Access::ShaderStorageWrite:
    case rhi::Access::ShaderStorageRead:
      return GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    case rhi::Access::Uniform:
      return GL_UNIFORM_BARRIER_BIT;
    case rhi::Access::VertexAttribute:
      return GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    case rhi::Access::Index:
      return GL_ELEMENT_ARRAY_BARRIER_BIT;
    case rhi::Access::IndirectCommand:
      return GL_COMMAND_BARRIER_BIT;
    case rhi::Access::TransferWrite:
    case rhi::Access::TransferRead:
      return GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT |
             GL_FRAMEBUFFER_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT;
  }
  return GL_ALL_BARRIER_BITS;
}

} // namespace

GLDevice::~GLDevice() = default;

void GLDevice::init(void *windowHandle) {
  m_window = static_cast<GLFWwindow *>(windowHandle);
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    LOG_ERROR("Failed to initialize GLAD");
    std::exit(1);
  }
  if (!GLAD_GL_VERSION_4_6) {
    LOG_ERROR("OpenGL 4.6 is required, but this context is '%s'",
              reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    std::exit(1);
  }
  if (!GLAD_GL_ARB_gl_spirv) {
    LOG_ERROR("OpenGL 4.6 is present, but GL_ARB_gl_spirv is not; the "
              "renderer needs it to load SPIR-V");
    std::exit(1);
  }
  LOG_INFO("OpenGL %s (%s)",
           reinterpret_cast<const char *>(glGetString(GL_VERSION)),
           reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

  // The engine's matrices are GL shaped (y up) but the depth range is
  // zero-to-one, matching the Vulkan backend.
  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
  glFrontFace(GL_CCW);
  glEnable(GL_SCISSOR_TEST);
  glCreateFramebuffers(1, &m_fbo);
  m_dumpPath = std::getenv("COMPONENG_GL_DUMP_FRAME");

  if (GLAD_GL_VERSION_4_3) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugCallback, this);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                          GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                          GL_FALSE);
  }
}

void GLDevice::shutdown() {
  if (m_fbo != 0) {
    glDeleteFramebuffers(1, &m_fbo);
    m_fbo = 0;
    m_fboColor = m_fboDepth = 0;
  }
  m_frameOpen = false;
}

bool GLDevice::beginFrame() {
  if (!m_window || glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
    m_frameOpen = false;
    return false;
  }
  // The UI backend and the driver's own state are unknown at frame start.
  m_pipeline = nullptr;
  m_vao = 0;
  m_indexType = GL_UNSIGNED_INT;
  m_indexOffset = 0;
  m_indexBuffer = 0;
  m_vertexBuffers = {};
  m_frameOpen = true;
  return true;
}

void GLDevice::present(void * /*windowHandle*/) {
  ++m_frameCount;
  if (m_window && m_dumpPath && m_frameCount == 120) {
    // Our own backbuffer, not the screen: flip GL's bottom-up rows into PPM.
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width) * height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    std::ofstream out(m_dumpPath, std::ios::binary);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row) {
      out.write(reinterpret_cast<const char *>(
                    &pixels[static_cast<std::size_t>(row) * width * 3]),
                static_cast<std::streamsize>(width) * 3);
    }
    LOG_INFO("Wrote %s", m_dumpPath);
  }
  if (m_window) {
    glfwSwapBuffers(m_window);
  }
  m_frameOpen = false;
}

void GLDevice::setWindowViewport(int x, int y, int w, int h) {
  m_windowViewport = {x, y, static_cast<uint32_t>(std::max(w, 0)),
                      static_cast<uint32_t>(std::max(h, 0))};
}

rhi::Extent2D GLDevice::windowViewportExtent() const {
  return {m_windowViewport.width, m_windowViewport.height};
}

std::unique_ptr<rhi::IBuffer>
GLDevice::createBuffer(const rhi::BufferDesc &desc) {
  return std::make_unique<GLBuffer>(desc);
}

std::unique_ptr<rhi::IImage>
GLDevice::createImage(const rhi::ImageDesc &desc) {
  return std::make_unique<GLImage>(desc);
}

std::unique_ptr<rhi::ISampler>
GLDevice::createSampler(const rhi::SamplerDesc &desc) {
  return std::make_unique<GLSampler>(desc);
}

std::unique_ptr<rhi::IShaderModule>
GLDevice::createShaderModule(const rhi::ShaderModuleDesc &desc) {
  return std::make_unique<GLShaderModule>(desc);
}

std::unique_ptr<rhi::IBindGroupLayout>
GLDevice::createBindGroupLayout(const rhi::BindGroupLayoutDesc &desc) {
  return std::make_unique<GLBindGroupLayout>(desc);
}

std::unique_ptr<rhi::IBindGroup>
GLDevice::createBindGroup(const rhi::BindGroupDesc &desc) {
  return std::make_unique<GLBindGroup>(desc);
}

std::unique_ptr<rhi::IGraphicsPipeline>
GLDevice::createGraphicsPipeline(const rhi::GraphicsPipelineDesc &desc) {
  return std::make_unique<GLGraphicsPipeline>(desc);
}

std::unique_ptr<rhi::IComputePipeline>
GLDevice::createComputePipeline(const rhi::ComputePipelineDesc &desc) {
  return std::make_unique<GLComputePipeline>(desc);
}

void GLDevice::uploadBuffer(rhi::IBuffer &dst, uint64_t offset,
                            const void *data, uint64_t bytes) {
  static_cast<GLBuffer &>(dst).write(offset, data, bytes);
}

void GLDevice::uploadImage(rhi::IImage &dst, const rhi::Rect &region,
                           const void *pixels) {
  auto &image = static_cast<GLImage &>(dst);
  const PixelFormat format = pixelFormatOf(image.format());
  // Rows are tightly packed; RGB and single-channel data are rarely a
  // multiple of GL's default four-byte alignment.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTextureSubImage2D(image.handle(), 0, region.x, region.y,
                      static_cast<GLsizei>(region.width),
                      static_cast<GLsizei>(region.height), format.format,
                      format.type, pixels);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  if (image.mipLevels() > 1) {
    glGenerateTextureMipmap(image.handle());
  }
}

void GLDevice::beginRendering(const rhi::RenderingDesc &desc) {
  GLuint fbo = 0;
  rhi::Rect rect = m_windowViewport;

  if (desc.color.image) {
    auto &color = static_cast<GLImage &>(*desc.color.image);
    fbo = m_fbo;
    const GLuint depth =
        desc.hasDepth && desc.depth.image
            ? static_cast<GLImage &>(*desc.depth.image).handle()
            : 0;
    // Texture names are recycled once the old target is destroyed (a window
    // resize), so compare-and-skip would leave the FBO pointing at a different
    // texture than the one the caller asked for.
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, color.handle(), 0);
    glNamedFramebufferTexture(m_fbo, GL_DEPTH_ATTACHMENT, depth, 0);
    m_fboColor = color.handle();
    m_fboDepth = depth;
    if (glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG_ERROR("OpenGL: %ux%u render target is incomplete",
                color.extent().width, color.extent().height);
    }
    rect = {0, 0, color.extent().width, color.extent().height};
  }

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  if (rect.width == 0 || rect.height == 0) {
    int width = 0;
    int height = 0;
    if (m_window) {
      glfwGetFramebufferSize(m_window, &width, &height);
    }
    rect = {0, 0, static_cast<uint32_t>(std::max(width, 0)),
            static_cast<uint32_t>(std::max(height, 0))};
  }
  applyViewport(rect);
  clearTarget(fbo, desc);
}

void GLDevice::endRendering() {
}

void GLDevice::setViewport(const rhi::Rect &rect) {
  glViewport(rect.x, rect.y, static_cast<GLsizei>(rect.width),
             static_cast<GLsizei>(rect.height));
}

void GLDevice::setScissor(const rhi::Rect &rect) {
  glScissor(rect.x, rect.y, static_cast<GLsizei>(rect.width),
            static_cast<GLsizei>(rect.height));
}

void GLDevice::applyViewport(const rhi::Rect &rect) {
  glViewport(rect.x, rect.y, static_cast<GLsizei>(rect.width),
             static_cast<GLsizei>(rect.height));
  glScissor(rect.x, rect.y, static_cast<GLsizei>(rect.width),
            static_cast<GLsizei>(rect.height));
  glEnable(GL_SCISSOR_TEST);
}

void GLDevice::clearTarget(GLuint fbo, const rhi::RenderingDesc &desc) {
  const bool clearColor = desc.color.load == rhi::LoadOp::Clear;
  const bool clearDepth =
      desc.hasDepth && desc.depth.load == rhi::LoadOp::Clear;
  if (!clearColor && !clearDepth) {
    return;
  }
  // glClear honours the scissor, colour and depth masks; a load op clears the
  // whole attachment, as Vulkan's does.
  glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (clearColor) {
    glClearNamedFramebufferfv(fbo, GL_COLOR, 0, desc.color.clear);
  }
  if (clearDepth) {
    glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &desc.depth.clearDepth);
  }
  glEnable(GL_SCISSOR_TEST);
}

void GLDevice::applyState(const rhi::RenderState &state) {
  if (state.cull == rhi::CullMode::None) {
    glDisable(GL_CULL_FACE);
  } else {
    glEnable(GL_CULL_FACE);
    glCullFace(state.cull == rhi::CullMode::Front ? GL_FRONT : GL_BACK);
  }

  if (state.depthTest) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(state.depthWrite ? GL_TRUE : GL_FALSE);
  glDepthFunc(depthFuncOf(state.depthFunc));

  const GLboolean colorWrite = state.colorWrite ? GL_TRUE : GL_FALSE;
  glColorMask(colorWrite, colorWrite, colorWrite, colorWrite);

  switch (state.blend) {
    case rhi::BlendMode::None:
      glDisable(GL_BLEND);
      break;
    case rhi::BlendMode::Alpha:
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case rhi::BlendMode::Additive:
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      break;
  }
}

void GLDevice::bindPipeline(rhi::IGraphicsPipeline &pipeline) {
  auto &p = static_cast<GLGraphicsPipeline &>(pipeline);
  if (!p.valid()) {
    return;
  }
  glUseProgram(p.program());
  glBindVertexArray(p.vao());
  m_pipeline = &p;
  m_vao = p.vao();
  // The buffers the caller bound belong to the command stream, but GL keeps
  // them in the VAO. Replay the remembered bindings into the new VAO.
  for (std::size_t slot = 0; slot < m_vertexBuffers.size(); ++slot) {
    const VertexBindingState &vertex = m_vertexBuffers[slot];
    const uint32_t stride = p.stride(static_cast<uint32_t>(slot));
    if (vertex.buffer != 0 && stride != 0) {
      glVertexArrayVertexBuffer(
          m_vao, static_cast<GLuint>(slot), vertex.buffer,
          static_cast<GLintptr>(vertex.offset), static_cast<GLsizei>(stride));
    }
  }
  if (m_indexBuffer != 0) {
    glVertexArrayElementBuffer(m_vao, m_indexBuffer);
  }
  applyState(p.state());
}

void GLDevice::bindGroup(uint32_t set, rhi::IBindGroup &group) {
  const auto &entries = static_cast<GLBindGroup &>(group).entries();
  for (const GLBindGroup::Entry &entry : entries) {
    const GLuint binding = set * 8 + entry.binding;
    switch (entry.type) {
      case rhi::BindingType::UniformBuffer:
        glBindBufferRange(GL_UNIFORM_BUFFER, binding, entry.buffer,
                          static_cast<GLintptr>(entry.offset),
                          static_cast<GLsizeiptr>(entry.range));
        break;
      case rhi::BindingType::StorageBuffer:
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, entry.buffer,
                          static_cast<GLintptr>(entry.offset),
                          static_cast<GLsizeiptr>(entry.range));
        break;
      case rhi::BindingType::CombinedImageSampler:
        glBindTextureUnit(binding, entry.image);
        glBindSampler(binding, entry.sampler);
        break;
      case rhi::BindingType::StorageImage:
        glBindImageTexture(binding, entry.image, 0, GL_FALSE, 0, GL_READ_WRITE,
                           entry.imageFormat);
        break;
    }
  }
}

void GLDevice::bindVertexBuffer(uint32_t slot, rhi::IBuffer &buffer,
                                uint64_t byteOffset) {
  if (slot >= m_vertexBuffers.size()) {
    return;
  }
  const GLuint handle = static_cast<GLBuffer &>(buffer).handle();
  m_vertexBuffers[slot] = {handle, byteOffset};
  if (m_pipeline == nullptr) {
    return;
  }
  glVertexArrayVertexBuffer(m_vao, slot, handle,
                            static_cast<GLintptr>(byteOffset),
                            static_cast<GLsizei>(m_pipeline->stride(slot)));
}

void GLDevice::bindIndexBuffer(rhi::IBuffer &buffer, uint64_t byteOffset,
                               rhi::IndexType type) {
  m_indexBuffer = static_cast<GLBuffer &>(buffer).handle();
  m_indexType = type == rhi::IndexType::U16 ? GL_UNSIGNED_SHORT
                                           : GL_UNSIGNED_INT;
  m_indexOffset = byteOffset;
  if (m_vao != 0) {
    glVertexArrayElementBuffer(m_vao, m_indexBuffer);
  }
}

void GLDevice::draw(uint32_t vertexCount, uint32_t instanceCount,
                    uint32_t firstVertex, uint32_t firstInstance) {
  glDrawArraysInstancedBaseInstance(GL_TRIANGLES, static_cast<GLint>(firstVertex),
                                    static_cast<GLsizei>(vertexCount),
                                    static_cast<GLsizei>(instanceCount),
                                    firstInstance);
}

void GLDevice::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                           uint32_t firstIndex, int32_t vertexOffset,
                           uint32_t firstInstance) {
  const uint64_t indexSize = m_indexType == GL_UNSIGNED_SHORT ? 2 : 4;
  const uint64_t offset = m_indexOffset + indexSize * firstIndex;
  glDrawElementsInstancedBaseVertexBaseInstance(
      GL_TRIANGLES, static_cast<GLsizei>(indexCount), m_indexType,
      reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)),
      static_cast<GLsizei>(instanceCount), vertexOffset, firstInstance);
}

void GLDevice::drawIndexedIndirect(rhi::IBuffer &commands, uint64_t offset,
                                   uint32_t drawCount, uint32_t stride) {
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER,
               static_cast<GLBuffer &>(commands).handle());
  glMultiDrawElementsIndirect(
      GL_TRIANGLES, m_indexType,
      reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), drawCount,
      stride != 0 ? stride : sizeof(rhi::DrawIndexedIndirectCommand));
}

void GLDevice::bindComputePipeline(rhi::IComputePipeline &pipeline) {
  auto &p = static_cast<GLComputePipeline &>(pipeline);
  if (p.valid()) {
    glUseProgram(p.program());
  }
}

void GLDevice::dispatch(uint32_t x, uint32_t y, uint32_t z) {
  glDispatchCompute(x, y, z);
}

void GLDevice::memoryBarrier(rhi::Access /*before*/, rhi::Access after) {
  glMemoryBarrier(barrierBits(after));
}

void GLDevice::copyBuffer(rhi::IBuffer &src, rhi::IBuffer &dst,
                          uint64_t srcOffset, uint64_t dstOffset,
                          uint64_t bytes) {
  glCopyNamedBufferSubData(static_cast<GLBuffer &>(src).handle(),
                           static_cast<GLBuffer &>(dst).handle(),
                           static_cast<GLintptr>(srcOffset),
                           static_cast<GLintptr>(dstOffset),
                           static_cast<GLsizeiptr>(bytes));
}

void GLDevice::initImGui(void *windowHandle) {
  ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow *>(windowHandle), true);
  ImGui_ImplOpenGL3_Init("#version 460");
}

void GLDevice::imguiNewFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}

void GLDevice::imguiRender() {
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GLDevice::shutdownImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
}

int GLDevice::checkErrors() const {
  int count = m_errors.exchange(0);
  while (glGetError() != GL_NO_ERROR) {
    ++count;
  }
  if (count > 0) {
    LOG_ERROR("OpenGL errors detected: %d", count);
  }
  return count;
}

void APIENTRY GLDevice::debugCallback(GLenum source, GLenum type, GLuint id,
                                      GLenum severity, GLsizei /*length*/,
                                      const GLchar *message,
                                      const void *userParam) {
  (void)source;
  (void)type;
  (void)id;
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION ||
      severity == GL_DEBUG_SEVERITY_LOW) {
    return;
  }
  if (const auto *self = static_cast<const GLDevice *>(userParam)) {
    ++self->m_errors;
  }
  LOG_ERROR("[opengl] %s", message);
}

void GLDevice::beginGpuTimer() {
  if (!m_gpuCreated) {
    glGenQueries(kGpuQueryCount, m_gpuQueries);
    m_gpuCreated = true;
  }
  // Every slot still waiting on the GPU: skip this frame's measurement rather
  // than overwrite a query that is in flight.
  if (m_gpuPending[m_gpuNext]) {
    m_gpuSpanOpen = false;
    return;
  }
  glBeginQuery(GL_TIME_ELAPSED, m_gpuQueries[m_gpuNext]);
  m_gpuSpanOpen = true;
}

void GLDevice::endGpuTimer() {
  if (m_gpuSpanOpen) {
    glEndQuery(GL_TIME_ELAPSED);
    m_gpuPending[m_gpuNext] = true;
    m_gpuNext = (m_gpuNext + 1) % kGpuQueryCount;
    m_gpuSpanOpen = false;
  }

  for (int i = 0; i < kGpuQueryCount; ++i) {
    const int slot = (m_gpuNext + i) % kGpuQueryCount;
    if (!m_gpuPending[slot]) {
      continue;
    }
    GLint available = 0;
    glGetQueryObjectiv(m_gpuQueries[slot], GL_QUERY_RESULT_AVAILABLE,
                       &available);
    if (!available) {
      continue;
    }
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_gpuQueries[slot], GL_QUERY_RESULT, &nanoseconds);
    m_gpuTimeMs = static_cast<double>(nanoseconds) * 1e-6;
    m_gpuPending[slot] = false;
  }

  collectGpuPasses();
  collectShaderTimers();
}

void GLDevice::beginGpuPass(api::PassId pass) {
  GpuPassTimer &t = m_gpuPasses[static_cast<std::size_t>(pass)];
  if (!t.created) {
    glGenQueries(kGpuQueryCount * 2, &t.queries[0][0]);
    t.created = true;
  }
  if (t.pending[t.next]) {
    t.open = false;
    return;
  }
  glQueryCounter(t.queries[t.next][0], GL_TIMESTAMP);
  t.open = true;
}

void GLDevice::endGpuPass(api::PassId pass) {
  GpuPassTimer &t = m_gpuPasses[static_cast<std::size_t>(pass)];
  if (!t.open) {
    return;
  }
  glQueryCounter(t.queries[t.next][1], GL_TIMESTAMP);
  t.pending[t.next] = true;
  t.next = (t.next + 1) % kGpuQueryCount;
  t.open = false;
}

void GLDevice::collectGpuPasses() {
  for (GpuPassTimer &t : m_gpuPasses) {
    for (int i = 0; i < kGpuQueryCount; ++i) {
      const int slot = (t.next + i) % kGpuQueryCount;
      if (!t.pending[slot]) {
        continue;
      }
      GLint available = 0;
      glGetQueryObjectiv(t.queries[slot][1], GL_QUERY_RESULT_AVAILABLE,
                         &available);
      if (!available) {
        break; // timestamps finish in order: nothing newer is ready either
      }
      GLuint64 start = 0;
      GLuint64 end = 0;
      glGetQueryObjectui64v(t.queries[slot][0], GL_QUERY_RESULT, &start);
      glGetQueryObjectui64v(t.queries[slot][1], GL_QUERY_RESULT, &end);
      t.ms = static_cast<double>(end - start) * 1e-6;
      t.pending[slot] = false;
    }
  }
}

void GLDevice::beginShaderTimer(uint64_t shaderId) {
  ShaderTimer &timer = m_shaderTimers[shaderId];
  if (timer.open) {
    return; // spans do not nest within one shader
  }
  const int slot = timer.current;
  if (timer.pending[slot]) {
    return;
  }
  const std::size_t needed = static_cast<std::size_t>(timer.spans[slot] + 1) * 2;
  std::vector<GLuint> &queries = timer.queries[slot];
  if (queries.size() < needed) {
    const std::size_t have = queries.size();
    queries.resize(needed);
    glGenQueries(static_cast<GLsizei>(needed - have), &queries[have]);
  }
  glQueryCounter(queries[static_cast<std::size_t>(timer.spans[slot]) * 2],
                 GL_TIMESTAMP);
  timer.open = true;
}

void GLDevice::endShaderTimer(uint64_t shaderId) {
  auto it = m_shaderTimers.find(shaderId);
  if (it == m_shaderTimers.end() || !it->second.open) {
    return;
  }
  ShaderTimer &timer = it->second;
  const int slot = timer.current;
  glQueryCounter(timer.queries[slot][static_cast<std::size_t>(
                     timer.spans[slot]) * 2 + 1],
                 GL_TIMESTAMP);
  ++timer.spans[slot];
  timer.open = false;
}

void GLDevice::collectShaderTimers() {
  for (auto &pair : m_shaderTimers) {
    ShaderTimer &timer = pair.second;
    // A span left open belongs to this frame; close it before collecting.
    if (timer.open) {
      const int slot = timer.current;
      glQueryCounter(timer.queries[slot][static_cast<std::size_t>(
                         timer.spans[slot]) * 2 + 1],
                     GL_TIMESTAMP);
      ++timer.spans[slot];
      timer.open = false;
    }
    timer.pending[timer.current] = true;
    timer.current = (timer.current + 1) % kGpuQueryCount;
    for (int i = 0; i < kGpuQueryCount; ++i) {
      const int slot = (timer.current + i) % kGpuQueryCount;
      if (!timer.pending[slot]) {
        continue;
      }
      const int spans = timer.spans[slot];
      const std::vector<GLuint> &queries = timer.queries[slot];
      if (spans > 0) {
        // Timestamps complete in submission order, so the last end implies
        // every earlier one is available too.
        GLint available = 0;
        glGetQueryObjectiv(queries[static_cast<std::size_t>(spans) * 2 - 1],
                           GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) {
          break;
        }
      }
      GLuint64 nanoseconds = 0;
      for (int s = 0; s < spans; ++s) {
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(queries[static_cast<std::size_t>(s) * 2],
                              GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(queries[static_cast<std::size_t>(s) * 2 + 1],
                              GL_QUERY_RESULT, &end);
        nanoseconds += end - start;
      }
      timer.ms = static_cast<double>(nanoseconds) * 1e-6;
      timer.spans[slot] = 0;
      timer.pending[slot] = false;
    }
  }
}

double GLDevice::shaderTimeMs(uint64_t shaderId) const {
  auto it = m_shaderTimers.find(shaderId);
  return it == m_shaderTimers.end() ? -1.0 : it->second.ms;
}

} // namespace componeng::renderer::opengl
