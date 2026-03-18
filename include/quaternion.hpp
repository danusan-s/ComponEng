#pragma once

struct Quaternion {
  float w, x, y, z;
  Quaternion() : w(1), x(0), y(0), z(0) {};
  Quaternion(float w_, float x_, float y_, float z_)
      : w(w_), x(x_), y(y_), z(z_) {};
};
