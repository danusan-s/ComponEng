#include "core/utils.hpp"
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

std::string Utils::getExecutablePath() {
#if defined(_WIN32)
  char buffer[MAX_PATH];
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  return std::string(buffer);
#elif defined(__APPLE__)
  char buffer[1024];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) == 0) {
    return std::string(buffer);
  }
  return "";
#elif defined(__linux__)
  char buffer[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
  if (count != -1) {
    return std::string(buffer, count);
  }
  return "";
#else
  return "";
#endif
}

std::string Utils::getExecutableDir() {
  std::filesystem::path exePath = getExecutablePath();
  if (exePath.empty()) {
    return std::filesystem::current_path()
        .string();
  }
  return exePath.parent_path().string();
}

std::string Utils::getAssetPath(const std::string& relativePath) {
  static std::filesystem::path root = [] {
    std::filesystem::path p = __FILE__;
    return p.parent_path().parent_path().parent_path();
  }();

  std::filesystem::path assetPath = root / relativePath;

  return std::filesystem::weakly_canonical(assetPath).string();
}
