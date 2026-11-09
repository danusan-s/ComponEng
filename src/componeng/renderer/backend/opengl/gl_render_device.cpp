#include "componeng/renderer/backend/opengl/gl_render_device.hpp"

#include "componeng/core/window.hpp"
#include "componeng/utils/logger.hpp"
#include <GLFW/glfw3.h>

#include <cstdint>

namespace componeng::renderer::opengl {

// windowHandle is unused here: the caller has already made the GL context
// current, and glad resolves entry points from it. Other backends need it.
void GLRenderDevice::beginGpuTimer() {
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

void GLRenderDevice::endGpuTimer() {
  if (m_gpuSpanOpen) {
    glEndQuery(GL_TIME_ELAPSED);
    m_gpuPending[m_gpuNext] = true;
    m_gpuNext = (m_gpuNext + 1) % kGpuQueryCount;
    m_gpuSpanOpen = false;
  }

  // Collect finished spans, oldest first so the newest completed one wins.
  for (int i = 0; i < kGpuQueryCount; ++i) {
    const int slot = (m_gpuNext + i) % kGpuQueryCount;
    if (!m_gpuPending[slot])
      continue;
    GLint available = 0;
    glGetQueryObjectiv(m_gpuQueries[slot], GL_QUERY_RESULT_AVAILABLE,
                       &available);
    if (!available)
      continue;
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_gpuQueries[slot], GL_QUERY_RESULT, &nanoseconds);
    m_gpuTimeMs = static_cast<double>(nanoseconds) * 1e-6;
    m_gpuPending[slot] = false;
  }
}

void GLRenderDevice::init(void * /*windowHandle*/) {
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    LOG_ERROR("Failed to initialize GLAD");
    return;
  }

  glEnable(GL_SCISSOR_TEST);
  glGenVertexArrays(1, &m_fullscreenVao);

  // Establish the baseline pipeline state and the cache that tracks it.
  applyRenderState(api::RenderState{}, /*force=*/true);
}

void GLRenderDevice::setRenderState(const api::RenderState &state) {
  applyRenderState(state, /*force=*/false);
}

void GLRenderDevice::applyRenderState(const api::RenderState &state,
                                      bool force) {
  const api::RenderState &cur = m_state;

  if (force || state.cull != cur.cull) {
    if (state.cull == api::CullMode::None) {
      glDisable(GL_CULL_FACE);
    } else {
      glEnable(GL_CULL_FACE);
      glCullFace(state.cull == api::CullMode::Front ? GL_FRONT : GL_BACK);
    }
  }

  if (force || state.depthTest != cur.depthTest) {
    if (state.depthTest) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  if (force || state.depthWrite != cur.depthWrite) {
    glDepthMask(state.depthWrite ? GL_TRUE : GL_FALSE);
  }
  if (force || state.depthFunc != cur.depthFunc) {
    glDepthFunc(state.depthFunc == api::DepthFunc::LessEqual ? GL_LEQUAL
                                                             : GL_LESS);
  }

  if (force || state.blend != cur.blend) {
    switch (state.blend) {
      case api::BlendMode::None:
        glDisable(GL_BLEND);
        break;
      case api::BlendMode::Alpha:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case api::BlendMode::Additive:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    }
  }

  m_state = state;
}

void GLRenderDevice::bindUniformBuffer(uint32_t binding, api::IBuffer &buffer) {
  glBindBufferBase(GL_UNIFORM_BUFFER, binding,
                   static_cast<GLBuffer &>(buffer).handle());
}

void GLRenderDevice::setViewport(int x, int y, int w, int h) {
  glViewport(x, y, w, h);
}

void GLRenderDevice::setScissor(int x, int y, int w, int h) {
  glScissor(x, y, w, h);
}

void GLRenderDevice::setActiveTextureUnit(uint32_t unit) {
  glActiveTexture(GL_TEXTURE0 + unit);
}

void GLRenderDevice::bindBufferTexture(uint32_t unit, api::IBuffer &buffer,
                                       api::BufferTextureFormat format) {
  GLenum internal = GL_R32F;
  switch (format) {
    case api::BufferTextureFormat::R32F:
      internal = GL_R32F;
      break;
    case api::BufferTextureFormat::RG32F:
      internal = GL_RG32F;
      break;
    case api::BufferTextureFormat::RGBA32F:
      internal = GL_RGBA32F;
      break;
  }
  GLBuffer &glBuffer = static_cast<GLBuffer &>(buffer);
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_BUFFER, glBuffer.bufferTexture());
  // Attached on every bind: setData may have replaced the buffer's storage
  // since the last frame, and re-attaching is cheap.
  glTexBuffer(GL_TEXTURE_BUFFER, internal, glBuffer.handle());
}

void GLRenderDevice::clear(float r, float g, float b, float a) {
  // glClear honours the depth mask, so a state with depth writes off would
  // leave last frame's depth in place. Clear with the mask on, then put the
  // tracked state back.
  if (!m_state.depthWrite) {
    glDepthMask(GL_TRUE);
  }
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!m_state.depthWrite) {
    glDepthMask(GL_FALSE);
  }
}

void GLRenderDevice::present(void *windowHandle) {
  GLFWwindow *window = static_cast<GLFWwindow *>(windowHandle);
  glfwSwapBuffers(window);
}

int GLRenderDevice::checkErrors() const {
  int count = 0;
  while (glGetError() != GL_NO_ERROR) {
    ++count;
  }
  if (count > 0) {
    LOG_ERROR("OpenGL errors detected: %d", count);
  }
  return count;
}

std::unique_ptr<api::IShader> GLRenderDevice::createShader() {
  return std::make_unique<GLShader>();
}

std::unique_ptr<api::ITexture> GLRenderDevice::createTexture() {
  return std::make_unique<GLTexture>();
}

std::unique_ptr<api::IMesh> GLRenderDevice::createMesh() {
  return std::make_unique<GLMesh>();
}

std::unique_ptr<api::IBuffer> GLRenderDevice::createBuffer() {
  return std::make_unique<GLBuffer>();
}

void GLRenderDevice::setupInstanceAttributes(api::IBuffer &instanceBuffer,
                                             const api::VertexLayout &layout) {
  GLuint instanceVBO = static_cast<GLBuffer &>(instanceBuffer).handle();

  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

  for (size_t i = 0; i < layout.attributes.size(); ++i) {
    const auto &attr = layout.attributes[i];
    GLuint location =
        api::kFirstInstanceAttribLocation + static_cast<GLuint>(i);
    glVertexAttribPointer(location, attr.componentCount, GL_FLOAT,
                          attr.normalized, layout.stride,
                          reinterpret_cast<const void *>(attr.offset));
    glEnableVertexAttribArray(location);
    glVertexAttribDivisor(location, 1); // Advance per instance
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLRenderDevice::unbindInstanceAttributes(const api::VertexLayout &layout) {
  for (size_t i = 0; i < layout.attributes.size(); ++i) {
    glDisableVertexAttribArray(api::kFirstInstanceAttribLocation +
                               static_cast<GLuint>(i));
  }
}

void GLRenderDevice::drawFullscreenTriangle() {
  glBindVertexArray(m_fullscreenVao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GLRenderDevice::drawIndexedInstanced(size_t indexCount,
                                          uint32_t instanceCount) {
  glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                          instanceCount);
}

} // namespace componeng::renderer::opengl
