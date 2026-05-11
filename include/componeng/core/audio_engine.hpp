#pragma once

#include <miniaudio.h>

namespace componeng::core {

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  void init();
  void shutdown();
  ma_engine &getEngine() {
    return m_audioEngine;
  }

  ma_sound loadAudioFromFile(const char *file);

private:
  ma_engine m_audioEngine;
};

} // namespace componeng::core
