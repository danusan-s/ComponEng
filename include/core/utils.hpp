#pragma once

#include <string>

class Utils {
public:
  static std::string getExecutablePath();
  static std::string getExecutableDir();
  static std::string getAssetPath(const std::string& relativePath);
};
