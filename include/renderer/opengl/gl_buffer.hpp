#pragma once

#include "glad/glad.h"
#include "renderer/api/irender_device.hpp"

class GLBuffer : public IBuffer {
public:
  GLBuffer();
  ~GLBuffer() override;

  void setData(const void* data, size_t sizeBytes) override;
  void setSubData(size_t offset, const void* data, size_t sizeBytes) override;
  void bind() override;
  void unbind() override;
  void release() override;

  GLuint handle() const { return m_id; }

private:
  GLuint m_id = 0;
  size_t m_size = 0;
};
