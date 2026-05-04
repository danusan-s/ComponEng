#include "componeng/systems/camera_system.hpp"
#include "componeng/components/camera_component.hpp"
#include "componeng/components/input_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include <GLFW/glfw3.h>
#include <cmath>

static constexpr float DEFAULT_MOVE_SPEED = 100.0f;
static constexpr float MOUSE_SENSITIVITY = 1.0f;
static constexpr float PITCH_LIMIT = 89.0f;

static void updateCameraVectors(const TransformComponent &transform,
                                Vec3 &front, Vec3 &right, Vec3 &up) {
  float cosYaw = cos(radians(transform.rotation.y));
  float sinYaw = sin(radians(transform.rotation.y));
  float cosPitch = cos(radians(transform.rotation.x));
  float sinPitch = sin(radians(transform.rotation.x));

  front.x = cosYaw * cosPitch;
  front.y = sinPitch;
  front.z = sinYaw * cosPitch;
  front = normalize(front);

  right = normalize(cross(front, Vec3(0.0f, 1.0f, 0.0f)));
  up = normalize(cross(right, front));
}

static void processKeyboardInput(TransformComponent &transform,
                                 const InputComponent &input, float deltaTime,
                                 float speed) {
  float velocity = speed * deltaTime;
  Vec3 front, right, up;
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
    transform.position = transform.position + Vec3(0.0f, 1.0f, 0.0f) * velocity;
  if (input.crouch)
    transform.position = transform.position - Vec3(0.0f, 1.0f, 0.0f) * velocity;
}

static void processMouseInput(TransformComponent &transform,
                              const MouseInputComponent &mouseInput,
                              float sensitivity) {
  transform.rotation.y += mouseInput.deltaX * sensitivity;
  transform.rotation.x -= mouseInput.deltaY * sensitivity;

  if (transform.rotation.x > PITCH_LIMIT)
    transform.rotation.x = PITCH_LIMIT;
  if (transform.rotation.x < -PITCH_LIMIT)
    transform.rotation.x = -PITCH_LIMIT;
}

void CameraSystem::onUpdate(const SystemState &state) {
  EntityID mainCameraEntity =
      state.world->getSingleton<MainCameraSingleton>().entity;

  TransformComponent &transform =
      state.world->getComponent<TransformComponent>(mainCameraEntity);
  CameraComponent &camera =
      state.world->getComponent<CameraComponent>(mainCameraEntity);
  InputComponent &input =
      state.world->getComponent<InputComponent>(mainCameraEntity);
  MouseInputComponent &mouseInput =
      state.world->getComponent<MouseInputComponent>(mainCameraEntity);

  processKeyboardInput(transform, input, state.deltaTime, DEFAULT_MOVE_SPEED);

  processMouseInput(transform, mouseInput, MOUSE_SENSITIVITY);

  Vec3 front, right, up;
  updateCameraVectors(transform, front, right, up);

  Mat4 viewMatrix = lookAt(transform.position, transform.position + front, up);

  Mat4 projectionMatrix = perspective(radians(camera.fov), camera.aspectRatio,
                                      camera.nearPlane, camera.farPlane);

  camera.viewProjectionMatrix = projectionMatrix * viewMatrix;

  DebugUI::addVec3("Camera Position", transform.position);
}
