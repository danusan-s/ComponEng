#pragma once

#include <string>

/**
 * @brief Static utility functions for filesystem path resolution.
 *
 * Provides helpers to locate the executable directory and construct
 * absolute paths to assets relative to the project root.
 */
class Utils {
public:
  static std::string getExecutablePath();
  static std::string getExecutableDir();
  static std::string getAssetPath(const std::string &relativePath);
};
