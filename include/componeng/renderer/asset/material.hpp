#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/archetype.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"

#include <cstdlib>
#include <unordered_map>

namespace componeng::renderer {

class IMaterial {
public:
  IMaterial(core::HandleID shaderID, core::HandleID textureID)
      : m_shaderID(shaderID), m_textureID(textureID) {
  }

  core::HandleID getShaderID() const {
    return m_shaderID;
  }

  core::HandleID getTextureID() const {
    return m_textureID;
  }

  void setUniform(const char *name, core::UniformMap::mapped_type value) {
    m_uniforms[name] = value;
  }

  core::UniformMap getUniforms() const {
    return m_uniforms;
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
      // Attributes like "modelMatrix[1]" address one row of a Mat4 uniform
      // stored under the base name ("modelMatrix"); split on '[' to find it.
      std::string baseName = attr.name;
      int row = -1;
      size_t bracketPos = attr.name.find('[');
      if (bracketPos != std::string::npos) {
        baseName = attr.name.substr(0, bracketPos);
        row = std::atoi(attr.name.c_str() + bracketPos + 1);
      }

      auto it = instanceData.find(baseName.c_str());
      if (it == instanceData.end()) {
        continue;
      }
      const auto &value = it->second;

      if (row >= 0) {
        if (std::holds_alternative<core::Mat4>(value)) {
          const core::Mat4 &mat4 = std::get<core::Mat4>(value);
          std::memcpy(base + attr.offset, glm::value_ptr(mat4) + row * 4,
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
};

} // namespace componeng::renderer
