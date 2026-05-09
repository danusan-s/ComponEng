#include "player_controller.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/input_state.hpp"
#include "componeng/resources/main_camera.hpp"
#include <GLFW/glfw3.h>

using namespace componeng;

static constexpr float DEFAULT_MOVE_SPEED = 100.0f;
static constexpr float MOUSE_SENSITIVITY = 0.1f;
static constexpr float PITCH_LIMIT = 89.0f;

static void getCameraVectors(const components::TransformComponent &transform,
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
                                 resources::InputState &input, float deltaTime,
                                 float speed) {
  float velocity = speed * deltaTime;
  core::Vec3 front, right, up;
  getCameraVectors(transform, front, right, up);

  if (input.isKeyPressed(GLFW_KEY_W))
    transform.position = transform.position + front * velocity;
  if (input.isKeyPressed(GLFW_KEY_S))
    transform.position = transform.position - front * velocity;
  if (input.isKeyPressed(GLFW_KEY_A))
    transform.position = transform.position - right * velocity;
  if (input.isKeyPressed(GLFW_KEY_D))
    transform.position = transform.position + right * velocity;
  if (input.isKeyPressed(GLFW_KEY_SPACE))
    transform.position =
        transform.position + core::Vec3(0.0f, 1.0f, 0.0f) * velocity;
  if (input.isKeyPressed(GLFW_KEY_LEFT_CONTROL))
    transform.position =
        transform.position - core::Vec3(0.0f, 1.0f, 0.0f) * velocity;
}

static void processMouseInput(components::TransformComponent &transform,
                              resources::InputState &input, float sensitivity) {
  transform.rotation.y += input.getMouseDeltaX() * sensitivity;
  transform.rotation.x -= input.getMouseDeltaY() * sensitivity;

  if (transform.rotation.x > PITCH_LIMIT)
    transform.rotation.x = PITCH_LIMIT;
  if (transform.rotation.x < -PITCH_LIMIT)
    transform.rotation.x = -PITCH_LIMIT;
}

void PlayerController::onUpdate(const componeng::ecs::SystemState &state) {
  componeng::resources::InputState &input =
      state.world->get_resource<resources::InputState>();

  ecs::EntityID mainCameraEntity =
      state.world->get_resource<resources::MainCamera>().entity;

  components::TransformComponent &transform =
      state.world->getComponent<components::TransformComponent>(
          mainCameraEntity);
  processKeyboardInput(transform, input, state.deltaTime, DEFAULT_MOVE_SPEED);

  processMouseInput(transform, input, MOUSE_SENSITIVITY);
}
