#pragma once

#include <memory>
#include <miniaudio.h>
#include <vector>

namespace componeng::resources {

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

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

  std::unique_ptr<ma_sound> createSound(const char *filePath);
  void setSoundPosition(ma_sound *sound, float x, float y, float z);
  void setSoundSettings(ma_sound *sound, float volume, float pitch, bool loop);
  void setSound3D(ma_sound *sound, float minDistance, float maxDistance);

  bool playSound(std::unique_ptr<ma_sound> sound);

  bool playSoundFromFile(const char *filePath, float x, float y, float z,
                         float volume, float pitch, bool loop,
                         float minDistance, float maxDistance);

  void cleanupFinishedSounds();

private:
  ma_engine m_audioEngine;
  std::vector<std::unique_ptr<ma_sound>> m_activeSounds;
};

} // namespace componeng::resources
