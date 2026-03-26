#pragma once

class World;

struct SystemState {
  World *world;
  float deltaTime;
};

class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void onUpdate(const SystemState &state) = 0;
  virtual void onCreate(const SystemState &state) {
  }
  virtual void onDestroy(const SystemState &state) {
  }
};
