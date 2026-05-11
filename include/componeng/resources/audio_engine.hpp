#pragma once

#include <memory>
#include <miniaudio.h>

namespace componeng::resources {

class AudioEngine {
public:
  AudioEngine();
  ~AudioEngine();

  void init();
  void shutdown();
  ma_engine &getEngine() {
    return m_audioEngine;
  }

  void setListenerPosition(float x, float y, float z);
  std::unique_ptr<ma_sound> decodeSound(ma_decoder *decoder);
  void setSoundPosition(ma_sound *sound, float x, float y, float z);
  void setSoundSettings(ma_sound *sound, float volume, float pitch, bool loop);
  void setSound3D(ma_sound *sound, float minDistance, float maxDistance);
  bool playSound(ma_sound *sound);

  std::unique_ptr<ma_decoder> getDecodedAudioFile(const char *file);

private:
  ma_engine m_audioEngine;
};

} // namespace componeng::resources
