#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace componeng::core {

struct Name {
  // Trivially copyable so components holding a Name can live in archetype
  // storage, which relocates rows with memcpy
  static constexpr std::size_t CAPACITY = 64;
  static constexpr std::size_t MAX_LENGTH = CAPACITY - 1;

  char value[CAPACITY] = {};

  Name() = default;
  Name(const char *s) {
    if (!s)
      return;
    const std::size_t length = std::strlen(s);
    // Truncating would silently alias two distinct names onto one key
    if (length > MAX_LENGTH) {
      throw std::length_error(
          std::string("Name exceeds ") + std::to_string(MAX_LENGTH) +
          " characters: " + std::string(s, MAX_LENGTH) + "...");
    }
    std::memcpy(value, s, length);
    value[length] = '\0';
  }
  Name(const std::string &s) : Name(s.c_str()) {
  }

  const char *c_str() const {
    return value;
  }
  bool operator==(const Name &other) const {
    return std::strcmp(value, other.value) == 0;
  }
  bool operator==(const char *s) const {
    return s && std::strcmp(value, s) == 0;
  }
  bool operator!=(const Name &other) const {
    return !(*this == other);
  }
  bool operator!=(const char *s) const {
    return !(*this == s);
  }
  operator const char *() const {
    return value;
  }
  operator std::string() const {
    return value;
  }
  bool empty() const {
    return value[0] == '\0';
  }
  std::size_t size() const {
    return std::strlen(value);
  }
};

} // namespace componeng::core

namespace std {
template <> struct hash<componeng::core::Name> {
  std::size_t operator()(const componeng::core::Name &name) const noexcept {
    return std::hash<std::string_view>{}(name.c_str());
  }
};
} // namespace std

namespace componeng::core {

using HandleID = uint32_t;

// Type aliases for cleaner code
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

// GLM function aliases
using glm::cross;
using glm::degrees;
using glm::dot;
using glm::length;
using glm::lookAt;
using glm::normalize;
using glm::ortho;
using glm::perspective;
using glm::radians;
using glm::rotate;
using glm::scale;
using glm::translate;
using glm::value_ptr;

using UniformMap = std::unordered_map<
    Name, std::variant<float, core::Vec2, core::Vec3, core::Vec4, core::Mat4>>;

} // namespace componeng::core
