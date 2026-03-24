#include "core/debug_ui.hpp"
#include "core/logger.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

void DebugUI::Init() {
  LOG_INFO("Initializing UI System...");

  auto window = glfwGetCurrentContext();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void DebugUI::Shutdown() {
  LOG_INFO("Shutting down UI System...");
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void DebugUI::BeginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::Begin("Debug Info");
}

void DebugUI::EndFrame() {
  ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::AddText(const std::string &label, const std::string &value) {
  ImGui::Text("%s: %s", label.c_str(), value.c_str());
}

void DebugUI::AddValue(const std::string &label, float v) {
  ImGui::Text("%s: %.2f", label.c_str(), v);
}

void DebugUI::AddVec3(const std::string &label, Vec3 v) {
  ImGui::Text("%s: (%.2f, %.2f, %.2f)", label.c_str(), v.x, v.y, v.z);
}
