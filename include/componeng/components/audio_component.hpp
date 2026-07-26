#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct AudioComponent {
  static constexpr const char *component_name = "AudioComponent";

  std::string audioName;
  bool playOnAwake = true;
  bool loop = false;
  bool isPlaying = false;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool is3D = false;
  float minDistance = 1.0f;
  float maxDistance = 100.0f;

  // Runtime data (not serialized)
  renderer::AudioID audioID = 0;
};

#define AUDIO_COMPONENT_FIELDS(F, ctx) \
  F(string, audioName, ctx) \
  F(bool, playOnAwake, true, ctx) \
  F(bool, loop, false, ctx) \
  F(float, volume, 1.0f, ctx) \
  F(float, pitch, 1.0f, ctx) \
  F(bool, is3D, false, ctx) \
  F(float, minDistance, 1.0f, ctx) \
  F(float, maxDistance, 100.0f, ctx)

SERIALIZABLE_COMPONENT(AudioComponent, AUDIO_COMPONENT_FIELDS)

#undef AUDIO_COMPONENT_FIELDS

} // namespace componeng::components
