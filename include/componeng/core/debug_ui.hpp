#pragma once

#include "componeng/core/types.hpp"
#include <string>

/**
 * @brief Wrapper around ImGui to provide a simple interface for displaying
 * debug information
 *
 * Provides initialization and shutdown of ImGui and beginFrame and endFrame
 * functions to wrap ImGui calls.
 * Provides helper functions to add text, values, and Vec3s to the debug UI.
 */
class DebugUI {
public:
  // Setup and teardown of ImGui context and OpenGL bindings
  static void init();
  static void shutdown();

  // Helper functions to add different types of information to the debug UI
  static void addText(const std::string &label, const std::string &value);
  static void addValue(const std::string &label, float v);
  static void addVec3(const std::string &label, Vec3 v);

  // Convenience functions to wrap ImGui frame calls
  static void beginFrame();
  static void endFrame();
};
