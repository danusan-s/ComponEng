#include "componeng/renderer/backend/opengl/gl_render_device.hpp"

#include "componeng/core/window.hpp"
#include "componeng/utils/logger.hpp"
#include <GLFW/glfw3.h>

#include <cstdint>

namespace componeng::renderer::opengl {

GLRenderDevice::GLRenderDevice() {
}

GLRenderDevice::~GLRenderDevice() {
}

void GLRenderDevice::init(void *windowHandle) {
  GLFWwindow *window = static_cast<GLFWwindow *>(windowHandle);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    LOG_ERROR("Failed to initialize GLAD");
    return;
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GLRenderDevice::setViewport(int x, int y, int w, int h) {
  glViewport(x, y, w, h);
}

void GLRenderDevice::setScissor(int x, int y, int w, int h) {
  glScissor(x, y, w, h);
}

void GLRenderDevice::clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

  for (int i = 0; i < layout.attributes.size(); ++i) {
    const auto &attr = layout.attributes[i];
    glVertexAttribPointer(3 + i, attr.componentCount, GL_FLOAT, attr.normalized,
                          layout.stride,
                          reinterpret_cast<const void *>(attr.offset));
    glEnableVertexAttribArray(3 + i);
    glVertexAttribDivisor(3 + i, 1); // Advance per instance
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLRenderDevice::unbindInstanceAttributes(const api::VertexLayout &layout) {
  for (int i = 0; i < layout.attributes.size(); ++i) {
    glDisableVertexAttribArray(3 + i);
  }
}

void GLRenderDevice::drawIndexedInstanced(size_t indexCount,
                                          uint32_t instanceCount) {
  glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                          instanceCount);
}

} // namespace componeng::renderer::opengl
