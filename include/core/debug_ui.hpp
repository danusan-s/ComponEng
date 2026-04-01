#pragma once

#include "core/types.hpp"
#include <string>

class DebugUI {
public:
  static void init();
  static void shutdown();

  static void addText(const std::string& label, const std::string& value);
  static void addValue(const std::string& label, float v);
  static void addVec3(const std::string& label, Vec3 v);

  static void beginFrame();
  static void endFrame();
};
