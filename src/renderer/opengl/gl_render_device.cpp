#include "core/logger.hpp"
#include "renderer/opengl/gl_render_device.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>

GLRenderDevice::GLRenderDevice() {
}

GLRenderDevice::~GLRenderDevice() {
}

void GLRenderDevice::init(void* windowHandle) {
  GLFWwindow* window = static_cast<GLFWwindow*>(windowHandle);
  glfwMakeContextCurrent(window);

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

void GLRenderDevice::present(void* windowHandle) {
  GLFWwindow* window = static_cast<GLFWwindow*>(windowHandle);
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

std::unique_ptr<IShader> GLRenderDevice::createShader() {
  return std::make_unique<GLShader>();
}

std::unique_ptr<ITexture> GLRenderDevice::createTexture() {
  return std::make_unique<GLTexture>();
}

std::unique_ptr<IMesh> GLRenderDevice::createMesh() {
  return std::make_unique<GLMesh>();
}

std::unique_ptr<IBuffer> GLRenderDevice::createBuffer() {
  return std::make_unique<GLBuffer>();
}

void GLRenderDevice::setupInstanceAttributes(IBuffer& instanceBuffer) {
  GLuint instanceVBO = static_cast<GLBuffer&>(instanceBuffer).handle();

  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

  struct InstanceData {
    float modelMatrix[16];
    float color[3];
  };

  for (int i = 0; i < 4; ++i) {
    glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void*>(sizeof(float) * 4 * i));
    glEnableVertexAttribArray(3 + i);
    glVertexAttribDivisor(3 + i, 1);
  }
  glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                        reinterpret_cast<void*>(sizeof(float) * 16));
  glEnableVertexAttribArray(7);
  glVertexAttribDivisor(7, 1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLRenderDevice::unbindInstanceAttributes() {
  for (int i = 3; i <= 7; ++i) {
    glDisableVertexAttribArray(i);
  }
}

void GLRenderDevice::drawIndexedInstanced(size_t indexCount,
                                           uint32_t instanceCount) {
  glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                          instanceCount);
}
