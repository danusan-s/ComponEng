#pragma once

#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/backend/opengl/gl_buffer.hpp"
#include "componeng/renderer/backend/opengl/gl_mesh.hpp"
#include "componeng/renderer/backend/opengl/gl_shader.hpp"
#include "componeng/renderer/backend/opengl/gl_texture.hpp"
#include "glad/glad.h"

namespace componeng::renderer::opengl {

/**
 * @brief OpenGL implementation of IRenderDevice.
 *
 * Handles GL context loading (glad), global GL state setup, and creates
 * OpenGL-backed resources (shaders, textures, meshes, buffers).
 */
class GLRenderDevice final : public api::IRenderDevice {
public:
  GLRenderDevice() = default;
  ~GLRenderDevice() override = default;

  GLRenderDevice(const GLRenderDevice &) = delete;
  GLRenderDevice &operator=(const GLRenderDevice &) = delete;
  GLRenderDevice(GLRenderDevice &&) = delete;
  GLRenderDevice &operator=(GLRenderDevice &&) = delete;

  void init(void *windowHandle) override;
  void setViewport(int x, int y, int w, int h) override;
  void setScissor(int x, int y, int w, int h) override;
  void setActiveTextureUnit(uint32_t unit) override;
  void setRenderState(const api::RenderState &state) override;
  void bindUniformBuffer(uint32_t binding, api::IBuffer &buffer) override;
  void clear(float r, float g, float b, float a) override;
  void present(void *windowHandle) override;
  int checkErrors() const override;

  std::unique_ptr<api::IShader> createShader() override;
  std::unique_ptr<api::ITexture> createTexture() override;
  std::unique_ptr<api::IMesh> createMesh() override;
  std::unique_ptr<api::IBuffer> createBuffer() override;

  void setupInstanceAttributes(api::IBuffer &instanceBuffer,
                               const api::VertexLayout &layout) override;
  void unbindInstanceAttributes(const api::VertexLayout &layout) override;

  void drawIndexedInstanced(size_t indexCount, uint32_t instanceCount) override;
  void drawFullscreenTriangle() override;

private:
  /** Issues GL calls for the parts of `state` that differ from m_state (all of
   *  it when `force`), then records it as current. */
  void applyRenderState(const api::RenderState &state, bool force);

  api::RenderState m_state;

  // Core-profile GL refuses to draw without a VAO bound, even one with no
  // attributes. Created once in init() and kept for the life of the context.
  unsigned int m_fullscreenVao = 0;
};

} // namespace componeng::renderer::opengl
