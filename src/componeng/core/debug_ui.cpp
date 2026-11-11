#include "componeng/core/debug_ui.hpp"

// With the debug UI compiled out the header supplies empty inline versions.
#ifndef COMPONENG_DISABLE_DEBUG_UI

#include "componeng/utils/logger.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

namespace componeng::core {

// The add* calls only mean something between beginFrame() and endFrame();
// outside that window (headless runs, tests) they are ignored instead of
// reaching ImGui without a frame in progress.
static bool s_frameActive = false;

void DebugUI::init() {
  LOG_INFO("Initializing UI System...");

  auto window = glfwGetCurrentContext();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.FontGlobalScale = 1.5f;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

void DebugUI::shutdown() {
  LOG_INFO("Shutting down UI System...");
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void DebugUI::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::Begin("Debug Info");
  s_frameActive = true;
}

void DebugUI::endFrame() {
  s_frameActive = false;
  ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::addText(const std::string &label, const std::string &value) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: %s", label.c_str(), value.c_str());
}

void DebugUI::addValue(const std::string &label, float v) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: %.2f", label.c_str(), v);
}

bool DebugUI::addSection(const std::string &label) {
  if (!s_frameActive)
    return false;
  return ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
}

void DebugUI::addVec3(const std::string &label, const Vec3 &v) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: (%.2f, %.2f, %.2f)", label.c_str(), v.x, v.y, v.z);
}

void DebugUI::addSlider(const std::string &label, float *v, float min,
                        float max) {
  if (!s_frameActive)
    return;
  ImGui::SliderFloat(label.c_str(), v, min, max);
}

} // namespace componeng::core

#endif // COMPONENG_DISABLE_DEBUG_UI
