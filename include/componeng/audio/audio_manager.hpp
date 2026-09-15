#pragma once

#include "componeng/audio/sound.hpp"
#include "componeng/core/types.hpp"
#include "miniaudio.h"

#include <list>
#include <queue>

namespace componeng::audio {

// Manages audio engine lifecycle and sound resources.
class AudioManager {
public:
  AudioManager() = default;
  ~AudioManager() = default;

  AudioManager(const AudioManager &) = delete;
  AudioManager &operator=(const AudioManager &) = delete;
  AudioManager(AudioManager &&) = default;
  AudioManager &operator=(AudioManager &&) = default;

  void init();
  void shutdown();
  ma_engine &getEngine() {
    return m_audioEngine;
  }

  void setListenerPosition(float x, float y, float z);
  void setListenerOrientation(float forwardX, float forwardY, float forwardZ,
                              float upX, float upY, float upZ);

  void registerSound(const char *filePath, core::Name name);
  core::HandleID createSound(core::Name name);
  Sound &getSound(core::HandleID id);

  bool playSound(core::HandleID id);

  void cleanupFinishedSounds();

private:
  ma_engine m_audioEngine;
  std::unordered_map<core::Name, Sound> m_soundResources;

  std::vector<Sound> m_soundInstances;
  std::list<core::HandleID> m_activeSounds;
  std::queue<core::HandleID> m_finishedSounds;
};

} // namespace componeng::audio
