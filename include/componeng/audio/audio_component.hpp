#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::audio {

struct AudioComponent {
  static constexpr const char *component_name = "AudioComponent";

  core::Name audioName;
  bool playOnAwake = true;
  bool loop = false;
  bool isPlaying = false;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool is3D = false;
  float minDistance = 1.0f;
  float maxDistance = 100.0f;

  // Runtime data (not serialized)
  core::HandleID audioID = 0;
};

} // namespace componeng::audio

namespace componeng::components {

#define AUDIO_COMPONENT_FIELDS(F, ctx)                                         \
  F(audioName, ctx)                                                            \
  F(playOnAwake, ctx)                                                          \
  F(loop, ctx)                                                                 \
  F(volume, ctx)                                                               \
  F(pitch, ctx)                                                                \
  F(is3D, ctx)                                                                 \
  F(minDistance, ctx)                                                          \
  F(maxDistance, ctx)

SERIALIZABLE_COMPONENT(audio::AudioComponent, AUDIO_COMPONENT_FIELDS)

#undef AUDIO_COMPONENT_FIELDS

} // namespace componeng::components
