#pragma once

#include "componeng/core/types.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/component_serializer.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace componeng::renderer {

/**
 * @brief Physically Based Rendering material.
 *
 * Supports Cook-Torrance PBR with:
 *   - albedo (base color) texture
 *   - normal map (tangent-space)
 *   - metallic map
 *   - roughness map
 *   - ambient occlusion map
 *
 * Per-instance data: instanceModel (mat4) + instanceColor (vec4)
 * Material uniforms: albedoMap, normalMap, metallicMap, roughnessMap, aoMap,
 *   lightDir, lightColor, cameraPos, ambientColor, ambientStrength,
 *   fogColor, fogDensity, fogStart, fogEnd, useTextures, useNormalMap
 */
class PbrMaterial : public IMaterial {
public:
  PbrMaterial(core::HandleID shaderID,
              core::HandleID textureID)
      : IMaterial(shaderID, textureID),
        m_normalMapID(0), m_metallicMapID(0), m_roughnessMapID(0), m_aoMapID(0) {}

  void buildInstanceData(ecs::World &world, ecs::EntityID entity,
                         core::UniformMap &outInstanceData) const override {
    const auto &transform =
        world.getComponent<core::TransformComponent>(entity);

    core::Mat4 modelMatrix = core::Mat4(1.0f);
    modelMatrix = core::translate(modelMatrix, transform.position);
    modelMatrix = core::rotate(modelMatrix,
                               core::radians(transform.rotation.x),
                               core::Vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = core::rotate(modelMatrix,
                               core::radians(transform.rotation.y),
                               core::Vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = core::rotate(modelMatrix,
                               core::radians(transform.rotation.z),
                               core::Vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = core::scale(modelMatrix, transform.scale);

    outInstanceData["instanceModel"] = modelMatrix;
    outInstanceData["instanceColor"] =
        world.hasComponent<ColorComponent>(entity)
            ? world.getComponent<ColorComponent>(entity).color
            : core::Vec4(1.0f);
  }

  core::HandleID getNormalMapID() const override { return m_normalMapID; }
  core::HandleID getMetallicMapID() const override { return m_metallicMapID; }
  core::HandleID getRoughnessMapID() const override { return m_roughnessMapID; }
  core::HandleID getAOMapID() const override { return m_aoMapID; }

  void setNormalMapID(core::HandleID id) { m_normalMapID = id; }
  void setMetallicMapID(core::HandleID id) { m_metallicMapID = id; }
  void setRoughnessMapID(core::HandleID id) { m_roughnessMapID = id; }
  void setAOMapID(core::HandleID id) { m_aoMapID = id; }

private:
  core::HandleID m_normalMapID;
  core::HandleID m_metallicMapID;
  core::HandleID m_roughnessMapID;
  core::HandleID m_aoMapID;
};

} // namespace componeng::renderer
