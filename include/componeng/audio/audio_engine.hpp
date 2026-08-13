#pragma once

#include "componeng/core/types.hpp"
#include <miniaudio.h>

#include <memory>
#include <unordered_map>

namespace componeng::audio {

class AudioEngine {
public:
  AudioEngine() = default;
  ~AudioEngine() = default;

  AudioEngine(const AudioEngine &) = delete;
  AudioEngine &operator=(const AudioEngine &) = delete;
  AudioEngine(AudioEngine &&) = default;
  AudioEngine &operator=(AudioEngine &&) = default;

  void init();
  void shutdown();
  ma_engine &getEngine() {
    return m_audioEngine;
  }

  void setListenerPosition(float x, float y, float z);
  void setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                              float upX, float upY, float upZ);

  std::unique_ptr<ma_sound> createSound(const char *filePath);
  void setSoundPosition(ma_sound *sound, float x, float y, float z);
  void updateSoundPosition(core::HandleID id, float x, float y, float z);
  void setSoundSettings(ma_sound *sound, float volume, float pitch, bool loop);
  void setSound3D(ma_sound *sound, float minDistance, float maxDistance);

  bool playSound(core::HandleID id, std::unique_ptr<ma_sound> sound);

  bool playSoundFromFile(const char *filePath, float x, float y, float z,
                         float volume, float pitch, bool loop,
                         float minDistance, float maxDistance);

  void cleanupFinishedSounds();

private:
  ma_engine m_audioEngine;
  std::unordered_map<core::HandleID, std::unique_ptr<ma_sound>> m_activeSounds;
};

} // namespace componeng::audio
