#pragma once

#include "componeng/core/math/mat4.hpp"
#include "componeng/core/math/vec2.hpp"
#include "componeng/core/math/vec3.hpp"
#include "componeng/core/math/vec4.hpp"

#include <cmath>

namespace componeng::core {

constexpr float PI = 3.14159265358979323846f;

inline float length(const Vec3 &v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline float dot(const Vec3 &a, const Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 normalize(const Vec3 &v) {
  float len = length(v);
  if (len > 0.0f) {
    return Vec3(v.x / len, v.y / len, v.z / len);
  }
  return Vec3(std::nanf(""), std::nanf(""), std::nanf(""));
}

inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
  return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x);
}

inline Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
  Vec3 f = normalize(center - eye);
  Vec3 s = normalize(cross(f, up));
  Vec3 u = cross(s, f);

  Mat4 result(1.0f);
  result[0][0] = s.x;
  result[1][0] = s.y;
  result[2][0] = s.z;
  result[0][1] = u.x;
  result[1][1] = u.y;
  result[2][1] = u.z;
  result[0][2] = -f.x;
  result[1][2] = -f.y;
  result[2][2] = -f.z;
  result[3][0] = -dot(s, eye);
  result[3][1] = -dot(u, eye);
  result[3][2] = dot(f, eye);
  return result;
}

inline Mat4 perspective(float fov, float aspect, float near, float far) {
  float tanHalfFov = std::tan(fov / 2.0f);
  Mat4 result(0.0f);
  result[0][0] = 1.0f / (aspect * tanHalfFov);
  result[1][1] = 1.0f / (tanHalfFov);
  result[2][2] = -(far + near) / (far - near);
  result[2][3] = -1.0f;
  result[3][2] = -(2.0f * far * near) / (far - near);
  return result;
}

inline Mat4 translate(const Mat4 &matrix, const Vec3 &translation) {
  Mat4 result = matrix;
  result[3][0] += translation.x;
  result[3][1] += translation.y;
  result[3][2] += translation.z;
  return result;
}

inline Mat4 rotate(const Mat4 &matrix, float angle, const Vec3 &axis) {
  float c = std::cos(angle);
  float s = std::sin(angle);
  Vec3 normAxis = normalize(axis);

  Mat4 rotation(1.0f);
  rotation[0][0] = c + (1 - c) * normAxis.x * normAxis.x;
  rotation[0][1] = (1 - c) * normAxis.x * normAxis.y + s * normAxis.z;
  rotation[0][2] = (1 - c) * normAxis.x * normAxis.z - s * normAxis.y;

  rotation[1][0] = (1 - c) * normAxis.y * normAxis.x - s * normAxis.z;
  rotation[1][1] = c + (1 - c) * normAxis.y * normAxis.y;
  rotation[1][2] = (1 - c) * normAxis.y * normAxis.z + s * normAxis.x;

  rotation[2][0] = (1 - c) * normAxis.z * normAxis.x + s * normAxis.y;
  rotation[2][1] = (1 - c) * normAxis.z * normAxis.y - s * normAxis.x;
  rotation[2][2] = c + (1 - c) * normAxis.z * normAxis.z;

  return matrix * rotation;
}

inline Mat4 scale(const Mat4 &matrix, const Vec3 &scale) {
  Mat4 result = matrix;
  result[0][0] *= scale.x;
  result[1][1] *= scale.y;
  result[2][2] *= scale.z;
  return result;
}

inline float radians(float degrees) {
  return degrees * (PI / 180.0f);
}

inline const float *value_ptr(const Vec2 &v) {
  return &v.x;
}

inline const float *value_ptr(const Vec3 &v) {
  return &v.x;
}

inline const float *value_ptr(const Vec4 &v) {
  return &v.x;
}

inline const float *value_ptr(const Mat4 &m) {
  return &m[0][0];
}

} // namespace componeng::core
