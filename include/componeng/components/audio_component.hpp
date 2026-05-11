#pragma once
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct AudioComponent {
  renderer::AudioID audioID;
  bool playOnAwake = true;
  bool loop = false;
  bool isPlaying = false;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool is3D = false;
  float minDistance = 1.0f;
  float maxDistance = 100.0f;
};

} // namespace componeng::components
