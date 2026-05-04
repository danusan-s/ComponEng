#pragma once

#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/renderer/opengl/gl_buffer.hpp"
#include "componeng/renderer/opengl/gl_mesh.hpp"
#include "componeng/renderer/opengl/gl_shader.hpp"
#include "componeng/renderer/opengl/gl_texture.hpp"
#include "glad/glad.h"

namespace componeng::renderer::opengl {

/**
 * @brief OpenGL implementation of IRenderDevice.
 *
 * Handles GL context loading (glad), global GL state setup, and creates
 * OpenGL-backed resources (shaders, textures, meshes, buffers).
 */
class GLRenderDevice : public api::IRenderDevice {
public:
  GLRenderDevice();
  ~GLRenderDevice() override;

  void init(void *windowHandle) override;
  void setViewport(int x, int y, int w, int h) override;
  void setScissor(int x, int y, int w, int h) override;
  void clear(float r, float g, float b, float a) override;
  void present(void *windowHandle) override;
  int checkErrors() const override;

  std::unique_ptr<api::IShader> createShader() override;
  std::unique_ptr<api::ITexture> createTexture() override;
  std::unique_ptr<api::IMesh> createMesh() override;
  std::unique_ptr<api::IBuffer> createBuffer() override;

  void setupInstanceAttributes(api::IBuffer &instanceBuffer) override;
  void unbindInstanceAttributes() override;

  void drawIndexedInstanced(size_t indexCount, uint32_t instanceCount) override;

private:
  void *m_windowHandle{nullptr};
};

} // namespace componeng::renderer::opengl
