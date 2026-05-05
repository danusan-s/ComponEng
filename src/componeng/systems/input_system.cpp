#include "componeng/systems/input_system.hpp"
#include "componeng/components/input_component.hpp"
#include "componeng/core/engine.hpp"
#include "componeng/ecs/world.hpp"

namespace componeng::systems {

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::onUpdate(const ecs::SystemState &state) {
  auto &inputState = core::Engine::get().m_window.m_inputState;

  state.world
      ->query<components::InputComponent, components::MouseInputComponent>()
      .each([&](components::InputComponent &input,
                components::MouseInputComponent &mouseInput) {
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

} // namespace componeng::systems
