#include "componeng/renderer/lod/lod_system.hpp"

#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/lod/lod_component.hpp"

namespace componeng::renderer {

void LodSystem::onUpdate(const ecs::SystemState &state) {
  ecs::World &world = *state.world;
  const core::Vec3 cam = world
                             .readComponent<core::TransformComponent>(
                                 world.getResource<camera::MainCamera>().entity)
                             .position;

  world.query()
      .read<core::TransformComponent>()
      .write<MeshComponent, LodComponent>()
      .eachParallel(world.threadPool(),
                    [cam](const core::TransformComponent &transform,
                          MeshComponent &mesh, LodComponent &lod) {
                      const float dx = transform.position.x - cam.x;
                      const float dy = transform.position.y - cam.y;
                      const float dz = transform.position.z - cam.z;
                      const float distSq = dx * dx + dy * dy + dz * dz;

                      // First level that has a mesh and whose range still
                      // covers us; kMaxLevels means "beyond all of them".
                      int level = LodComponent::kMaxLevels;
                      for (int k = 0; k < LodComponent::kMaxLevels; ++k) {
                        if (lod.mesh[k] != 0 &&
                            distSq < lod.distance[k] * lod.distance[k]) {
                          level = k;
                          break;
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
