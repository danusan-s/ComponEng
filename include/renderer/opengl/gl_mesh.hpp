#pragma once

#include "glad/glad.h"
#include "renderer/api/irender_device.hpp"

class GLMesh : public IMesh {
public:
  GLMesh();
  ~GLMesh() override;

  void upload(const float* vertices, size_t vertexCount,
              const uint32_t* indices, size_t indexCount,
              const VertexLayout& layout) override;
  void bind() const override;
  void release() override;
  size_t indexCount() const override;

  GLuint vao() const { return m_vao; }

private:
  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  GLuint m_ebo = 0;
  size_t m_indexCount = 0;
};
