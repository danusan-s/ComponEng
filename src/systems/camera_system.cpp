#include "systems/camera_system.hpp"
#include "components/camera_component.hpp"
#include "components/input_component.hpp"
#include "components/transform_component.hpp"
#include "core/debug_ui.hpp"
#include "core/types.hpp"
#include "ecs/world.hpp"
#include <GLFW/glfw3.h>
#include <cmath>

static constexpr float DEFAULT_MOVE_SPEED = 100.0f;
static constexpr float MOUSE_SENSITIVITY = 0.5f;
static constexpr float PITCH_LIMIT = 89.0f;

static void UpdateCameraVectors(const TransformComponent &transform,
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

static void ProcessKeyboardInput(TransformComponent &transform,
                                 const InputComponent &input, float deltaTime,
                                 float speed) {
  float velocity = speed * deltaTime;
  Vec3 front, right, up;
  UpdateCameraVectors(transform, front, right, up);

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

static void ProcessMouseInput(TransformComponent &transform,
                              const MouseInputComponent &mouseInput,
                              float sensitivity) {
  transform.rotation.y += mouseInput.deltaX * sensitivity;
  transform.rotation.x += mouseInput.deltaY * sensitivity;

  if (transform.rotation.x > PITCH_LIMIT)
    transform.rotation.x = PITCH_LIMIT;
  if (transform.rotation.x < -PITCH_LIMIT)
    transform.rotation.x = -PITCH_LIMIT;
}

void CameraSystem::Update(float deltaTime) {
  world
      ->query<TransformComponent, CameraComponent, InputComponent,
              MouseInputComponent>()
      .each([&](auto &transform, auto &camera, auto &input, auto &mouseInput) {
        if (!camera.isMainCamera)
          return;

        ProcessKeyboardInput(transform, input, deltaTime, DEFAULT_MOVE_SPEED);

        smoothedMouseDelta =
            mix(smoothedMouseDelta, Vec2(mouseInput.deltaX, mouseInput.deltaY),
                1.0f - SMOOTHING);
        ProcessMouseInput(
            transform,
            {smoothedMouseDelta.x, smoothedMouseDelta.y, false, false},
            MOUSE_SENSITIVITY);

        Vec3 front, right, up;
        UpdateCameraVectors(transform, front, right, up);

        this->world->mainCameraData.viewMatrix =
            lookAt(transform.position, transform.position + front, up);

        this->world->mainCameraData.projectionMatrix =
            perspective(radians(camera.fov), camera.aspectRatio,
                        camera.nearPlane, camera.farPlane);
        this->world->mainCameraData.position = transform.position;

        DebugUI::AddVec3("Camera Position", transform.position);
      });
}
