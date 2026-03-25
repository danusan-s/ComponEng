#pragma once

class IGame {
public:
  virtual void onInit() = 0;
  virtual void onUpdate(float deltaTime) = 0;
  virtual void onShutdown() = 0;
};
