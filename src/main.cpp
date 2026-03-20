#include "game.hpp"
#include "glad/glad.h"

#include <GLFW/glfw3.h>

#include <iostream>

void letterbox_viewport(int windowWidth, int windowHeight);

// The Width of the screen
// Modify only width if you want to change size
const unsigned int INIT_SCREEN_WIDTH = 1920;
// The height of the screen
const unsigned int INIT_SCREEN_HEIGHT = 1080;

Game gameObj;

int main(int argc, char *argv[]) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(INIT_SCREEN_WIDTH, INIT_SCREEN_HEIGHT,
                                        "Game", nullptr, nullptr);
  if (!window) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  // glad: load all OpenGL function pointers
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);

  glfwSetFramebufferSizeCallback(window,
                                 [](GLFWwindow *window, int width, int height) {
                                   letterbox_viewport(width, height);
                                 });

  letterbox_viewport(INIT_SCREEN_WIDTH, INIT_SCREEN_HEIGHT);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // initialize game
  // ---------------
  gameObj.Init();

  gameObj.Run();

  return 0;
}

void letterbox_viewport(int windowWidth, int windowHeight) {
  float targetAspectRatio = 16.0f / 9.0f;
  float currentAspectRatio = static_cast<float>(windowWidth) / windowHeight;

  if (currentAspectRatio > targetAspectRatio) {
    int newWidth = static_cast<int>(windowHeight * targetAspectRatio);
    int xOffset = (windowWidth - newWidth) / 2;
    glScissor(xOffset, 0, newWidth, windowHeight);
    glViewport(xOffset, 0, newWidth, windowHeight);
    return;
  }

  int newHeight = static_cast<int>(windowWidth / targetAspectRatio);
  int yOffset = (windowHeight - newHeight) / 2;
  glScissor(0, yOffset, windowWidth, newHeight);
  glViewport(0, yOffset, windowWidth, newHeight);
}
