#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/utils/logger.hpp"

#include <unordered_map>
#include <unordered_set>

namespace componeng::renderer {

class IMaterial {
public:
  IMaterial(core::HandleID shaderID, core::HandleID textureID)
      : m_shaderID(shaderID), m_textureID(textureID) {
  }

  virtual ~IMaterial() = default;

  core::HandleID getShaderID() const {
    return m_shaderID;
  }

  core::HandleID getTextureID() const {
    return m_textureID;
  }

  void setUniform(const char *name, core::UniformMap::mapped_type value) {
    if (!m_knownUniforms.empty() &&
        m_knownUniforms.find(core::Name(name)) == m_knownUniforms.end()) {
      LOG_ERROR("setUniform: '%s' is not an active uniform in this "
                "material's shader",
                name);
    }
    m_uniforms[name] = value;
  }

  core::UniformMap getUniforms() const {
    return m_uniforms;
  }

  /** Called by AssetManager::registerMaterial right after construction, from
   *  the shader's reflected active uniforms; makes setUniform() typo-safe. */
  void setKnownUniforms(const std::vector<std::string> &names) {
    m_knownUniforms.clear();
    for (const auto &n : names) {
      m_knownUniforms.insert(core::Name(n));
    }
  }

  void setVertexLayout(const api::VertexLayout &layout) {
    m_vertexLayout = layout;
  }

  const api::VertexLayout getVertexLayout() const {
    return m_vertexLayout;
  }

  virtual core::UniformMap buildInstanceData(ecs::World &world,
                                             ecs::EntityID entity) const = 0;

  std::vector<float> buildInstanceDataFloats(ecs::World &world,
                                             ecs::EntityID entity) const {
    api::VertexLayout layout = getVertexLayout();
    std::vector<float> instanceDataFloats(layout.stride / sizeof(float), 0.0f);

    if (layout.attributes.empty()) {
      return instanceDataFloats;
    }

    core::UniformMap instanceData = buildInstanceData(world, entity);
    auto *base = reinterpret_cast<uint8_t *>(instanceDataFloats.data());

    for (const auto &attr : layout.attributes) {
      auto it = instanceData.find(attr.name.c_str());
      if (it == instanceData.end()) {
        continue;
      }
      const auto &value = it->second;

      // attr.matrixRow >= 0 means this slot is one row of a mat4-typed
      // value shared by name across its 4 rows (see reflectInstanceLayout).
      if (attr.matrixRow >= 0) {
        if (std::holds_alternative<core::Mat4>(value)) {
          const core::Mat4 &mat4 = std::get<core::Mat4>(value);
          std::memcpy(base + attr.offset,
                      glm::value_ptr(mat4) + attr.matrixRow * 4,
                      sizeof(float) * 4);
        }
        continue;
      }

      if (std::holds_alternative<float>(value)) {
        float f = std::get<float>(value);
        std::memcpy(base + attr.offset, &f, sizeof(float));
      } else if (std::holds_alternative<core::Vec2>(value)) {
        std::memcpy(base + attr.offset,
                    glm::value_ptr(std::get<core::Vec2>(value)),
                    sizeof(core::Vec2));
      } else if (std::holds_alternative<core::Vec3>(value)) {
        std::memcpy(base + attr.offset,
                    glm::value_ptr(std::get<core::Vec3>(value)),
                    sizeof(core::Vec3));
      } else if (std::holds_alternative<core::Vec4>(value)) {
        std::memcpy(base + attr.offset,
                    glm::value_ptr(std::get<core::Vec4>(value)),
                    sizeof(core::Vec4));
      } else if (std::holds_alternative<core::Mat4>(value)) {
        std::memcpy(base + attr.offset,
                    glm::value_ptr(std::get<core::Mat4>(value)),
                    sizeof(core::Mat4));
      }
    }
    return instanceDataFloats;
  }

private:
  core::HandleID m_shaderID;
  core::HandleID m_textureID;
  api::VertexLayout m_vertexLayout;
  core::UniformMap m_uniforms;
  std::unordered_set<core::Name> m_knownUniforms;
};

} // namespace componeng::renderer
