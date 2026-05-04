#pragma once

#include "glad/glad.h"
#include "componeng/renderer/api/irender_device.hpp"

class GLTexture : public ITexture {
public:
  GLTexture();
  ~GLTexture() override;

  void generate(uint32_t width, uint32_t height, const unsigned char* data,
                bool alpha) override;
  void bind() const override;
  void release() override;

  GLuint handle() const { return m_id; }

private:
  GLuint m_id = 0;
};
