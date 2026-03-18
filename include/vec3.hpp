#pragma once
#include <cmath>

struct Vec3 {
  float x, y, z;

  Vec3() : x(0), y(0), z(0) {};
  Vec3(float a) : x(a), y(a), z(a) {};
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {};

  Vec3 operator+(const Vec3 &other) {
    return Vec3(x + other.x, y + other.y, z + other.z);
  }

  Vec3 operator-(const Vec3 &other) {
    return Vec3(x - other.x, y - other.y, z - other.z);
  }

  Vec3 operator*(float scalar) {
    return Vec3(x * scalar, y * scalar, z * scalar);
  }

  Vec3 &operator+=(const Vec3 &other) {
    this->x += other.x;
    this->y += other.y;
    this->z += other.z;
    return *this;
  }

  float length() {
    return sqrtf(x * x + y * y + z * z);
  }

  Vec3 normalized() {
    float l = length();
    if (l == 0) {
      return Vec3();
    }
    return Vec3(x / l, y / l, z / l);
  }
};
