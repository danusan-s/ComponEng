#include "componeng/core/debug_ui.hpp"

#include "componeng/utils/logger.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

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
  // The default window only exists for the debug panels; without them nothing
  // is added outside a pane of its own.
  if constexpr (kEnabled)
    ImGui::Begin("Debug Info");
  s_frameActive = true;
}

void DebugUI::endFrame() {
  s_frameActive = false;
  if constexpr (kEnabled)
    ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::showFps(float fps, float avgFps) {
  if (!s_frameActive)
    return;
  if constexpr (kEnabled) {
    detail::addValue("FPS", fps);
    detail::addValue("Avg FPS", avgFps);
  } else {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("FPS", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Avg FPS: %.1f", avgFps);
    ImGui::End();
  }
}

namespace detail {

void addText(const std::string &label, const std::string &value) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: %s", label.c_str(), value.c_str());
}

void addValue(const std::string &label, float v) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: %.2f", label.c_str(), v);
}

bool addSection(const std::string &label) {
  if (!s_frameActive)
    return false;
  return ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
}

bool beginPane(const std::string &label) {
  if (!s_frameActive)
    return false;
  // A window's first appearance cascades from the top-left, so the panes do not
  // all open on top of each other. Afterwards ImGui's own saved layout wins.
  static std::vector<std::string> opened;
  std::size_t index = 0;
  while (index < opened.size() && opened[index] != label)
    ++index;
  if (index == opened.size())
    opened.push_back(label);
  const float offset = 40.0f * static_cast<float>(index + 1);
  ImGui::SetNextWindowPos(ImVec2(offset, offset), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(560.0f, 380.0f), ImGuiCond_FirstUseEver);
  return ImGui::Begin(label.c_str());
}

void endPane() {
  if (!s_frameActive)
    return;
  ImGui::End();
}

void addVec3(const std::string &label, const Vec3 &v) {
  if (!s_frameActive)
    return;
  ImGui::Text("%s: (%.2f, %.2f, %.2f)", label.c_str(), v.x, v.y, v.z);
}

void addSlider(const std::string &label, float *v, float min, float max) {
  if (!s_frameActive)
    return;
  ImGui::SliderFloat(label.c_str(), v, min, max);
}

} // namespace detail

} // namespace componeng::core
