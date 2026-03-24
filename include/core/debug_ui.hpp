#pragma once
#include "core/types.hpp"
#include <string>

class DebugUI {
public:
  static void Init();
  static void Shutdown();

  static void AddText(const std::string &label, const std::string &value);
  static void AddValue(const std::string &label, float v);
  static void AddVec3(const std::string &label, Vec3 v);

  static void BeginFrame();
  static void EndFrame();
};
