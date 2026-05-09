#include "componeng/systems/camera_system.hpp"
#include "componeng/components/camera_component.hpp"
#include "componeng/components/input_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/main_camera.hpp"
#include <GLFW/glfw3.h>
#include <cmath>

namespace componeng::systems {

static void updateCameraVectors(const components::TransformComponent &transform,
                                core::Vec3 &front, core::Vec3 &right,
                                core::Vec3 &up) {
  float cosYaw = cos(core::radians(transform.rotation.y));
  float sinYaw = sin(core::radians(transform.rotation.y));
  float cosPitch = cos(core::radians(transform.rotation.x));
  float sinPitch = sin(core::radians(transform.rotation.x));

  front.x = cosYaw * cosPitch;
  front.y = sinPitch;
  front.z = sinYaw * cosPitch;
  front = normalize(front);

  right = normalize(cross(front, core::Vec3(0.0f, 1.0f, 0.0f)));
  up = normalize(cross(right, front));
}

void CameraSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraEntity =
      state.world->get_resource<resources::MainCamera>().entity;

  components::TransformComponent &transform =
      state.world->getComponent<components::TransformComponent>(
          mainCameraEntity);

  components::CameraComponent &camera =
      state.world->getComponent<components::CameraComponent>(mainCameraEntity);

  core::Vec3 front, right, up;
  updateCameraVectors(transform, front, right, up);

  core::Mat4 viewMatrix =
      lookAt(transform.position, transform.position + front, up);

  core::Mat4 projectionMatrix =
      core::perspective(core::radians(camera.fov), camera.aspectRatio,
                        camera.nearPlane, camera.farPlane);

  camera.viewProjectionMatrix = projectionMatrix * viewMatrix;

  core::DebugUI::addVec3("Camera Position", transform.position);
}

} // namespace componeng::systems
