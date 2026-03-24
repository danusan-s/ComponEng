#include "systems/ui_system.hpp"
#include "components/camera_component.hpp"
#include "components/transform_component.hpp"
#include "core/input_state.hpp"
#include "core/types.hpp"
#include "ecs/world.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

static void ShutdownImGui() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void UISystem::Init(World &world) {
  System::Init(world);
  std::cout << "Initializing UI System..." << std::endl;

  auto window = glfwGetCurrentContext();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void UISystem::Shutdown() {
  std::cout << "Shutting down UI System..." << std::endl;
  ShutdownImGui();
}

void UISystem::Update(float deltaTime) {

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  world->query<CameraComponent, TransformComponent>().each(
      [&](CameraComponent &camera, TransformComponent &transform) {
        if (!camera.isMainCamera)
          return;
        ImGui::Begin("Camera Inspector");

        ImGui::Text("Position: (%.2f, %.2f, %.2f)", transform.position.x,
                    transform.position.y, transform.position.z);
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", transform.rotation.x,
                    transform.rotation.y, transform.rotation.z);
        ImGui::Text("FOV: %.2f", camera.fov);
        ImGui::Text("Near/Far: %.2f / %.2f", camera.nearPlane, camera.farPlane);

        ImGui::Separator();
        ImGui::Text("Delta Time: %.4f s", deltaTime);
        ImGui::Text("FPS: %.0f", 1.0f / deltaTime);

        ImGui::End();
      });

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
