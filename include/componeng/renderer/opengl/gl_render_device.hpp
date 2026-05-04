#pragma once

#include "glad/glad.h"
#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/renderer/opengl/gl_buffer.hpp"
#include "componeng/renderer/opengl/gl_mesh.hpp"
#include "componeng/renderer/opengl/gl_shader.hpp"
#include "componeng/renderer/opengl/gl_texture.hpp"

/**
 * @brief OpenGL implementation of IRenderDevice.
 *
 * Handles GL context loading (glad), global GL state setup, and creates
 * OpenGL-backed resources (shaders, textures, meshes, buffers).
 */
class GLRenderDevice : public IRenderDevice {
public:
  GLRenderDevice();
  ~GLRenderDevice() override;

  void init(void* windowHandle) override;
  void setViewport(int x, int y, int w, int h) override;
  void setScissor(int x, int y, int w, int h) override;
  void clear(float r, float g, float b, float a) override;
  void present(void* windowHandle) override;
  int checkErrors() const override;

  std::unique_ptr<IShader> createShader() override;
  std::unique_ptr<ITexture> createTexture() override;
  std::unique_ptr<IMesh> createMesh() override;
  std::unique_ptr<IBuffer> createBuffer() override;

  void setupInstanceAttributes(IBuffer& instanceBuffer) override;
  void unbindInstanceAttributes() override;

  void drawIndexedInstanced(size_t indexCount, uint32_t instanceCount) override;

private:
  void* m_windowHandle{nullptr};
};
