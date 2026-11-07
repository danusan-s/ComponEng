#pragma once

#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace componeng::ecs {
class World;
} // namespace componeng::ecs

namespace componeng::renderer {

/**
 * @brief Memoises TransformComponent::toMatrix() per entity.
 *
 * Transform fields are public and written directly, so there is no setter to
 * hang a dirty flag on. Instead each entry remembers the position, rotation
 * and scale it was built from and is reused only when all nine floats still
 * match. That makes it correct by construction (a stale matrix is impossible,
 * even across entity-id reuse) while a static entity costs nine compares
 * instead of trigonometry.
 */
class TransformMatrixCache {
public:
  const core::Mat4 &get(ecs::EntityID entity,
                        const core::TransformComponent &transform) {
    if (entity >= m_entries.size()) {
      m_entries.resize(static_cast<std::size_t>(entity) + 1);
    }
    Entry &e = m_entries[entity];
    if (!e.valid || !sameTransform(e, transform)) {
      e.position = transform.position;
      e.rotation = transform.rotation;
      e.scale = transform.scale;
      e.model = transform.toMatrix();
      e.valid = true;
    }
    return e.model;
  }

  void clear() {
    m_entries.clear();
  }

private:
  struct Entry {
    core::Vec3 position;
    core::Vec3 rotation;
    core::Vec3 scale;
    core::Mat4 model;
    bool valid = false;
  };

  static bool sameTransform(const Entry &e,
                            const core::TransformComponent &t) {
    return e.position.x == t.position.x && e.position.y == t.position.y &&
           e.position.z == t.position.z && e.rotation.x == t.rotation.x &&
           e.rotation.y == t.rotation.y && e.rotation.z == t.rotation.z &&
           e.scale.x == t.scale.x && e.scale.y == t.scale.y &&
           e.scale.z == t.scale.z;
  }

  std::vector<Entry> m_entries;
};

/**
 * @brief What a material gets to work with when writing one entity's
 * per-instance data. The transform is handed over already fetched (the
 * batching query has it in hand), so a material needs no component lookup
 * for the common case.
 */
class InstanceContext {
public:
  InstanceContext(ecs::World &worldRef, ecs::EntityID entityID,
                  const core::TransformComponent &transformRef,
                  TransformMatrixCache &matrices)
      : world(worldRef), entity(entityID), transform(transformRef),
        m_matrices(matrices) {
  }

  ecs::World &world;
  ecs::EntityID entity;
  const core::TransformComponent &transform;

  /** Model matrix for this entity, from the per-entity cache. */
  const core::Mat4 &modelMatrix() const {
    return m_matrices.get(entity, transform);
  }

private:
  TransformMatrixCache &m_matrices;
};

/**
 * @brief Cursor over the floats of a single instance, addressed by the
 * shader's own attribute names (see IShader::reflectInstanceLayout).
 *
 * Writes straight into the batch's flat instance buffer: no allocation, no
 * intermediate map. Names the shader does not declare are ignored, so a
 * material can offer more attributes than a given shader consumes. A value
 * wider than its attribute is truncated to the attribute's component count
 * instead of spilling into the next attribute.
 *
 * Name construction interns a string; materials should build their Names once
 * (a member or a static) rather than from a literal on every call.
 */
class InstanceWriter {
public:
  InstanceWriter(const api::VertexLayout &layout, float *instance)
      : m_layout(layout), m_instance(instance) {
  }

  void set(core::Name name, float value) {
    setScalars(name, &value, 1);
  }
  void set(core::Name name, const core::Vec2 &value) {
    setScalars(name, core::value_ptr(value), 2);
  }
  void set(core::Name name, const core::Vec3 &value) {
    setScalars(name, core::value_ptr(value), 3);
  }
  void set(core::Name name, const core::Vec4 &value) {
    setScalars(name, core::value_ptr(value), 4);
  }

  /** A mat4 attribute occupies four consecutive vec4 slots sharing one name;
   *  slot r takes column r of the matrix. */
  void set(core::Name name, const core::Mat4 &value) {
    for (const api::VertexAttribute &attr : m_layout.attributes) {
      if (attr.name == name && attr.matrixRow >= 0) {
        std::memcpy(m_instance + attr.offset / sizeof(float),
                    core::value_ptr(value) + attr.matrixRow * 4,
                    sizeof(float) * 4);
      }
    }
  }

  /** Type-erased form, used by the legacy UniformMap material path. */
  void set(core::Name name, const core::UniformMap::mapped_type &value) {
    std::visit([&](const auto &v) { set(name, v); }, value);
  }

private:
  void setScalars(core::Name name, const float *src, uint32_t count) {
    for (const api::VertexAttribute &attr : m_layout.attributes) {
      if (attr.name == name && attr.matrixRow < 0) {
        std::memcpy(m_instance + attr.offset / sizeof(float), src,
                    sizeof(float) * std::min(count, attr.componentCount));
        return;
      }
    }
  }

  const api::VertexLayout &m_layout;
  float *m_instance;
};

} // namespace componeng::renderer
