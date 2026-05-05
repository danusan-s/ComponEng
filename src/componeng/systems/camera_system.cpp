#include "componeng/systems/camera_system.hpp"
#include "componeng/components/camera_component.hpp"
#include "componeng/components/input_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include <GLFW/glfw3.h>
#include <cmath>

namespace componeng::systems {

static constexpr float DEFAULT_MOVE_SPEED = 100.0f;
static constexpr float MOUSE_SENSITIVITY = 1.0f;
static constexpr float PITCH_LIMIT = 89.0f;

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

static void processKeyboardInput(components::TransformComponent &transform,
                                 const components::InputComponent &input,
                                 float deltaTime, float speed) {
  float velocity = speed * deltaTime;
  core::Vec3 front, right, up;
  updateCameraVectors(transform, front, right, up);

  if (input.forward)
    transform.position = transform.position + front * velocity;
  if (input.backward)
    transform.position = transform.position - front * velocity;
  if (input.left)
    transform.position = transform.position - right * velocity;
  if (input.right)
    transform.position = transform.position + right * velocity;
  if (input.jump)
    transform.position =
        transform.position + core::Vec3(0.0f, 1.0f, 0.0f) * velocity;
  if (input.crouch)
    transform.position =
        transform.position - core::Vec3(0.0f, 1.0f, 0.0f) * velocity;
}

static void processMouseInput(components::TransformComponent &transform,
                              const components::MouseInputComponent &mouseInput,
                              float sensitivity) {
  transform.rotation.y += mouseInput.deltaX * sensitivity;
  transform.rotation.x -= mouseInput.deltaY * sensitivity;

  if (transform.rotation.x > PITCH_LIMIT)
    transform.rotation.x = PITCH_LIMIT;
  if (transform.rotation.x < -PITCH_LIMIT)
    transform.rotation.x = -PITCH_LIMIT;
}

void CameraSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraEntity =
      state.world->getSingleton<components::MainCameraSingleton>().entity;

  components::TransformComponent &transform =
      state.world->getComponent<components::TransformComponent>(
          mainCameraEntity);
  components::CameraComponent &camera =
      state.world->getComponent<components::CameraComponent>(mainCameraEntity);
  components::InputComponent &input =
      state.world->getComponent<components::InputComponent>(mainCameraEntity);
  components::MouseInputComponent &mouseInput =
      state.world->getComponent<components::MouseInputComponent>(
          mainCameraEntity);

  processKeyboardInput(transform, input, state.deltaTime, DEFAULT_MOVE_SPEED);

  processMouseInput(transform, mouseInput, MOUSE_SENSITIVITY);

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
