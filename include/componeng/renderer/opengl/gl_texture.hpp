#pragma once

#include "componeng/renderer/api/irender_device.hpp"
#include "glad/glad.h"

namespace componeng::renderer::opengl {

class GLTexture : public api::ITexture {
public:
  GLTexture();
  ~GLTexture() override;

  void generate(uint32_t width, uint32_t height, const unsigned char *data,
                bool alpha) override;
  void bind() const override;
  void release() override;

  GLuint handle() const {
    return m_id;
  }

private:
  GLuint m_id = 0;
};

} // namespace componeng::renderer::opengl
