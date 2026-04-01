#include "systems/input_system.hpp"
#include "components/input_component.hpp"
#include "core/engine.hpp"
#include "ecs/world.hpp"

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::onUpdate(const SystemState &state) {
  auto &inputState = Engine::Get().window.inputState;

  state.world->query<InputComponent, MouseInputComponent>().each(
      [&](InputComponent &input, MouseInputComponent &mouseInput) {
        input.forward = inputState.keys[FORWARD_KEY];
        input.backward = inputState.keys[BACKWARD_KEY];
        input.left = inputState.keys[LEFT_KEY];
        input.right = inputState.keys[RIGHT_KEY];
        input.jump = inputState.keys[JUMP_KEY];
        input.crouch = inputState.keys[CROUCH_KEY];

        mouseInput.deltaX = inputState.mouseX - inputState.lastMouseX;
        mouseInput.deltaY = inputState.mouseY - inputState.lastMouseY;
        mouseInput.leftButton = inputState.mouseButtons[GLFW_MOUSE_BUTTON_LEFT];
        mouseInput.rightButton =
            inputState.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT];
      });

  inputState.lastMouseX = inputState.mouseX;
  inputState.lastMouseY = inputState.mouseY;
}
