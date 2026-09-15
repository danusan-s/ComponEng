#pragma once

#include "miniaudio.h"

#include <memory>

namespace componeng::audio {
class Sound {
private:
  std::unique_ptr<ma_sound> m_sound;

public:
  Sound();
  ~Sound();
  Sound(const Sound &) = delete;
  Sound &operator=(const Sound &) = delete;
  Sound(Sound &&) = default;
  Sound &operator=(Sound &&) = default;

  ma_sound *getSound();
  Sound &setPosition(float x, float y, float z);
  Sound &setVolume(float volume);
  Sound &setPitch(float pitch);
  Sound &setLooping(bool loop);
  Sound &set3D(float minDistance, float maxDistance);
  bool play();
  bool stop();
  bool isPlaying() const;
};
} // namespace componeng::audio
