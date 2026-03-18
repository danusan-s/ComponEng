#ifndef UTILS_H
#define UTILS_H

#include <string>

class Utils {
public:
  static std::string GetExecutablePath();
  static std::string GetExecutableDir();
  static std::string GetAssetPath(const std::string &relativePath);
};

#endif // UTILS_H
