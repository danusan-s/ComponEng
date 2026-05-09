#include "componeng/systems/input_system.hpp"
#include "componeng/components/input_component.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/input_state.hpp"

namespace componeng::systems {

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::onUpdate(const ecs::SystemState &state) {
  auto &inputState = state.world->get_resource<resources::InputState>();

  state.world
      ->query<components::InputComponent, components::MouseInputComponent>()
      .each([&](components::InputComponent &input,
                components::MouseInputComponent &mouseInput) {
        input.forward = inputState.isKeyPressed(FORWARD_KEY);
        input.backward = inputState.isKeyPressed(BACKWARD_KEY);
        input.left = inputState.isKeyPressed(LEFT_KEY);
        input.right = inputState.isKeyPressed(RIGHT_KEY);
        input.jump = inputState.isKeyPressed(JUMP_KEY);
        input.crouch = inputState.isKeyPressed(CROUCH_KEY);

        mouseInput.deltaX = inputState.getMouseDeltaX();
        mouseInput.deltaY = inputState.getMouseDeltaY();
        mouseInput.leftButton =
            inputState.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
        mouseInput.rightButton =
            inputState.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
      });
}

} // namespace componeng::systems
