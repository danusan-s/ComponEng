#pragma once

class IRenderSystem {
public:
  virtual ~IRenderSystem() = default;
  virtual void Render() = 0;
};

class OpenGLRenderSystem : public IRenderSystem {
public:
  void Render() override;
};
