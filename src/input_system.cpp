#include "systems/input_system.hpp"
#include "components/input_component.hpp"
#include "world.hpp"

static void GLFWKeyCallback(GLFWwindow *window, int key, int scancode,
                            int action, int mode) {
  auto *inputSystem =
      static_cast<InputSystem *>(glfwGetWindowUserPointer(window));
  if (inputSystem)
    inputSystem->KeyCallback(key, scancode, action, mode);
}

static void GLFWMouseButtonCallback(GLFWwindow *window, int button, int action,
                                    int mods) {
  auto *inputSystem =
      static_cast<InputSystem *>(glfwGetWindowUserPointer(window));
  if (inputSystem)
    inputSystem->MouseButtonCallback(button, action, mods);
}

static void GLFWMouseCallback(GLFWwindow *window, double xposIn,
                              double yposIn) {
  auto *inputSystem =
      static_cast<InputSystem *>(glfwGetWindowUserPointer(window));
  if (inputSystem)
    inputSystem->MouseCallback(xposIn, yposIn);
}

void InputSystem::Init(World &world) {
  System::Init(world);

  GLFWwindow *window = glfwGetCurrentContext();
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, GLFWKeyCallback);
  glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
  glfwSetCursorPosCallback(window, GLFWMouseCallback);
}

void InputSystem::Update(float deltaTime) {
  for (auto entity : entities) {
    auto &input = world->GetComponent<InputComponent>(entity);
    input.forward = state.keys[GLFW_KEY_W];
    input.backward = state.keys[GLFW_KEY_S];
    input.left = state.keys[GLFW_KEY_A];
    input.right = state.keys[GLFW_KEY_D];
    input.jump = state.keys[GLFW_KEY_SPACE];
    input.crouch = state.keys[GLFW_KEY_LEFT_SHIFT];

    auto &mouseInput = world->GetComponent<MouseInputComponent>(entity);
    mouseInput.deltaX = state.mouseDeltaX;
    mouseInput.deltaY = state.mouseDeltaY;
    mouseInput.leftButton = state.mouseButtons[GLFW_MOUSE_BUTTON_LEFT];
    mouseInput.rightButton = state.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT];
  }

  state.mouseDeltaX = 0.0f;
  state.mouseDeltaY = 0.0f;
}

void InputSystem::KeyCallback(int key, int scancode, int action, int mode) {
  if (key >= 0 && key < 1024) {
    if (action == GLFW_PRESS)
      state.keys[key] = true;
    else if (action == GLFW_RELEASE)
      state.keys[key] = false;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
  }
}

void InputSystem::MouseButtonCallback(int button, int action, int mods) {
  if (button >= 0 && button < 8) {
    state.mouseButtons[button] = (action == GLFW_PRESS);
  }
}

void InputSystem::MouseCallback(double xposIn, double yposIn) {
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (state.firstMouse) {
    state.lastMouseX = xpos;
    state.lastMouseY = ypos;
    state.firstMouse = false;
  }

  state.mouseDeltaX = xpos - state.lastMouseX;
  state.mouseDeltaY = state.lastMouseY - ypos; // inverted Y
  state.lastMouseX = xpos;
  state.lastMouseY = ypos;
}
