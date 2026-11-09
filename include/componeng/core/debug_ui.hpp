#pragma once

#include "componeng/core/types.hpp"

#include <string>

namespace componeng::core {

/**
 * @brief Wrapper around ImGui to provide a simple interface for displaying
 * debug information
 *
 * Provides initialization and shutdown of ImGui and beginFrame and endFrame
 * functions to wrap ImGui calls.
 * Provides helper functions to add text, values, and Vec3s to the debug UI.
 *
 * Configure with -DCOMPONENG_DEBUG_UI=OFF to compile it out entirely: ImGui is
 * not built, every function here becomes an empty inline, and the engine skips
 * its frame-timing and GPU-timer work. Code that calls ImGui directly (rather
 * than through DebugUI) must guard it with DebugUI::kEnabled or
 * COMPONENG_DISABLE_DEBUG_UI, since ImGui is not linked in that build.
 */
class DebugUI {
public:
#ifdef COMPONENG_DISABLE_DEBUG_UI
  /** False in builds configured with -DCOMPONENG_DEBUG_UI=OFF. Code that does
   *  extra work only to feed the UI can skip it with `if constexpr`. */
  static constexpr bool kEnabled = false;

  // Debug UI compiled out: every call is an empty inline, so call sites cost
  // nothing and no ImGui code is built or linked.
  static void init() {
  }
  static void shutdown() {
  }
  static void addText(const std::string &, const std::string &) {
  }
  static void addValue(const std::string &, float) {
  }
  static void addVec3(const std::string &, const Vec3 &) {
  }
  static bool addSection(const std::string &) {
    return false;
  }
  static void addSlider(const std::string &, float *, float, float) {
  }
  static void beginFrame() {
  }
  static void endFrame() {
  }
#else
  static constexpr bool kEnabled = true;

  // Setup and teardown of ImGui context and OpenGL bindings
  static void init();
  static void shutdown();

  // Helper functions to add different types of information to the debug UI
  static void addText(const std::string &label, const std::string &value);
  static void addValue(const std::string &label, float v);
  static void addVec3(const std::string &label, const Vec3 &v);
  /** Starts a collapsible group (open by default); the add* calls that follow
   *  until the next section belong to it. Returns false when it is collapsed,
   *  so the caller can skip building its contents. */
  static bool addSection(const std::string &label);
  static void addSlider(const std::string &label, float *v, float min,
                        float max);

  // Convenience functions to wrap ImGui frame calls
  static void beginFrame();
  static void endFrame();
#endif
};

} // namespace componeng::core
