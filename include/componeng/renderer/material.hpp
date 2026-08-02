#pragma once

#include "componeng/core/types.hpp"

#include <variant>

namespace componeng::renderer {

using UniformMap =
    std::unordered_map<const char *, std::variant<float, core::Vec2, core::Vec3,
                                                  core::Vec4, core::Mat4>>;

class Material {
public:
  Material(core::HandleID shaderID, core::HandleID textureID)
      : m_shaderID(shaderID), m_textureID(textureID) {
  }

  core::HandleID getShaderID() const {
    return m_shaderID;
  }

  core::HandleID getTextureID() const {
    return m_textureID;
  }

  void setUniform(const char *name,
                  const std::variant<float, core::Vec2, core::Vec3, core::Vec4,
                                     core::Mat4> &value) {
    m_uniforms[name] = value;
  }

  UniformMap getUniforms() const {
    return m_uniforms;
  }

private:
  core::HandleID m_shaderID = 0;
  core::HandleID m_textureID = 0;

  UniformMap m_uniforms;
};

} // namespace componeng::renderer
