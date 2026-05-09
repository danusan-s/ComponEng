#include "componeng/systems/input_system.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/action_state.hpp"
#include "componeng/resources/input_state.hpp"
#include <GLFW/glfw3.h>

namespace componeng::systems {

constexpr auto FORWARD_KEY = GLFW_KEY_W;
constexpr auto BACKWARD_KEY = GLFW_KEY_S;
constexpr auto LEFT_KEY = GLFW_KEY_A;
constexpr auto RIGHT_KEY = GLFW_KEY_D;
constexpr auto JUMP_KEY = GLFW_KEY_SPACE;
constexpr auto CROUCH_KEY = GLFW_KEY_LEFT_CONTROL;
constexpr auto SPRINT_KEY = GLFW_KEY_LEFT_SHIFT;

void InputSystem::onUpdate(const ecs::SystemState &state) {
  auto &inputState = state.world->get_resource<resources::InputState>();
  auto &actionState = state.world->get_resource<resources::ActionState>();

  actionState.current[(size_t)resources::Action::MoveForward] =
      inputState.isKeyDown(FORWARD_KEY);
  actionState.current[(size_t)resources::Action::MoveBackward] =
      inputState.isKeyDown(BACKWARD_KEY);
  actionState.current[(size_t)resources::Action::MoveLeft] =
      inputState.isKeyDown(LEFT_KEY);
  actionState.current[(size_t)resources::Action::MoveRight] =
      inputState.isKeyDown(RIGHT_KEY);
  actionState.current[(size_t)resources::Action::Jump] =
      inputState.isKeyDown(JUMP_KEY);
  actionState.current[(size_t)resources::Action::Crouch] =
      inputState.isKeyDown(CROUCH_KEY);
  actionState.current[(size_t)resources::Action::Sprint] =
      inputState.isKeyDown(SPRINT_KEY);
}

} // namespace componeng::systems
