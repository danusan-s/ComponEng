#include "systems/input_system.hpp"
#include "components/input_component.hpp"
#include "core/game.hpp"
#include "ecs/world.hpp"

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::Update(float deltaTime) {
  auto &state = Game::Get().window.inputState;

  world->query<InputComponent, MouseInputComponent>().each(
      [&](InputComponent &input, MouseInputComponent &mouseInput) {
        input.forward = state.keys[FORWARD_KEY];
        input.backward = state.keys[BACKWARD_KEY];
        input.left = state.keys[LEFT_KEY];
        input.right = state.keys[RIGHT_KEY];
        input.jump = state.keys[JUMP_KEY];
        input.crouch = state.keys[CROUCH_KEY];

        mouseInput.deltaX = state.mouseDeltaX;
        mouseInput.deltaY = state.mouseDeltaY;
        mouseInput.leftButton = state.mouseButtons[GLFW_MOUSE_BUTTON_LEFT];
        mouseInput.rightButton = state.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT];
      });

  state.mouseDeltaX = 0.0f;
  state.mouseDeltaY = 0.0f;
}
