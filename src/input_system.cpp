#include "systems/input_system.hpp"
#include "components/input_component.hpp"
#include "world.hpp"

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::Update(float deltaTime) {
  auto &state = *world->inputState;

  world->query<InputComponent, MouseInputComponent>().each(
      [&](InputComponent &input, MouseInputComponent &mouseInput) {
        input.forward = false;
        input.backward = false;
        input.left = false;
        input.right = false;
        input.jump = false;
        input.crouch = false;

        mouseInput.deltaX = 0.0f;
        mouseInput.deltaY = 0.0f;
        mouseInput.leftButton = false;
        mouseInput.rightButton = false;
      });
}
