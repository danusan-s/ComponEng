#include "componeng/renderer/asset/material.hpp"

namespace componeng::renderer {

Material::Material(core::HandleID shaderID, core::HandleID textureID)
    : m_shaderID(shaderID), m_textureID(textureID) {
}

core::HandleID Material::getShaderID() const {
  return m_shaderID;
}

core::HandleID Material::getTextureID() const {
  return m_textureID;
}

void Material::setUniform(const char *name, UniformMap::mapped_type value) {
  m_uniforms[name] = value;
}

UniformMap Material::getUniforms() const {
  return m_uniforms;
}

} // namespace componeng::renderer
