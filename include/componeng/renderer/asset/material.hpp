#pragma once

#include "componeng/core/types.hpp"

#include <unordered_map>
#include <variant>

namespace componeng::renderer {

using UniformMap =
    std::unordered_map<const char *, std::variant<float, core::Vec2, core::Vec3,
                                                  core::Vec4, core::Mat4>>;

class Material {
public:
  Material(core::HandleID shaderID, core::HandleID textureID);

  core::HandleID getShaderID() const;

  core::HandleID getTextureID() const;

  void setUniform(const char *name, UniformMap::mapped_type value);

  UniformMap getUniforms() const;

private:
  core::HandleID m_shaderID = 0;
  core::HandleID m_textureID = 0;

  UniformMap m_uniforms;
};

} // namespace componeng::renderer
