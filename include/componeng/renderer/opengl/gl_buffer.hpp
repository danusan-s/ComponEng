#pragma once

#include "componeng/renderer/api/irender_device.hpp"
#include "glad/glad.h"

namespace componeng::renderer::opengl {

class GLBuffer : public api::IBuffer {
public:
  GLBuffer();
  ~GLBuffer() override;

  void
  setData(const void *data, size_t sizeBytes,
          api::IBuffer::Usage usage = api::IBuffer::Usage::Dynamic) override;
  void setSubData(size_t offset, const void *data, size_t sizeBytes) override;
  void release() override;

  GLuint handle() const {
    return m_id;
  }

private:
  GLuint m_id = 0;
  size_t m_size = 0;
};

} // namespace componeng::renderer::opengl
