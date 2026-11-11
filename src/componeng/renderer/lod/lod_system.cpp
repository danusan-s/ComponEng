#include "componeng/renderer/lod/lod_system.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/lod/lod_component.hpp"

#include <algorithm>
#include <cmath>

namespace componeng::renderer {

void LodSystem::onUpdate(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  const ecs::EntityID cameraEntity =
      world.getResource<camera::MainCamera>().entity;
  const core::Vec3 cam =
      world.readComponent<core::TransformComponent>(cameraEntity).position;
  // Screen-height mode: height on screen = size / (2 * distance * tanHalfFov),
  // so a level holds while size >= threshold * 2 * tanHalfFov * distance.
  const float twoTanHalfFov =
      2.0f * std::tan(0.5f * core::radians(
                                 world.readComponent<camera::CameraComponent>(
                                          cameraEntity)
                                     .fov));

  world.query()
      .read<core::TransformComponent>()
      .write<MeshComponent, LodComponent>()
      .eachParallel(world.threadPool(),
                    [cam, twoTanHalfFov](const core::TransformComponent &transform,
                          MeshComponent &mesh, LodComponent &lod) {
                      const float dx = transform.position.x - cam.x;
                      const float dy = transform.position.y - cam.y;
                      const float dz = transform.position.z - cam.z;
                      const float distSq = dx * dx + dy * dy + dz * dz;

                      // First level that has a mesh and whose range still
                      // covers us; kMaxLevels means "beyond all of them".
                      int level = LodComponent::kMaxLevels;
                      if (lod.size > 0.0f) {
                        // Screen-height mode: the entity's apparent height as
                        // a fraction of the screen's against each threshold.
                        const core::Vec3 &s = transform.scale;
                        const float worldSize =
                            lod.size * std::max({std::fabs(s.x), std::fabs(s.y),
                                                 std::fabs(s.z)});
                        const float coverage =
                            worldSize /
                            (twoTanHalfFov * std::sqrt(distSq) + 1e-6f);
                        for (int k = 0; k < LodComponent::kMaxLevels; ++k) {
                          if (lod.mesh[k] != 0 &&
                              coverage >= lod.screenHeight[k]) {
                            level = k;
                            break;
                          }
                        }
                      } else {
                        for (int k = 0; k < LodComponent::kMaxLevels; ++k) {
                          if (lod.mesh[k] != 0 &&
                              distSq < lod.distance[k] * lod.distance[k]) {
                            level = k;
                            break;
                          }
                        }
                      }

                      if (level == lod.level) {
                        return;
                      }
                      lod.level = level;
                      if (level == LodComponent::kMaxLevels) {
                        mesh.visible = false;
                      } else {
                        mesh.visible = true;
                        mesh.meshID = lod.mesh[level];
                      }
                    });
}

} // namespace componeng::renderer
