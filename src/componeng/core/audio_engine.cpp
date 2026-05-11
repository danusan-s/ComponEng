#include "componeng/resources/audio_engine.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::resources {

AudioEngine::AudioEngine() {
}

AudioEngine::~AudioEngine() {
}

void AudioEngine::init() {
  ma_result result = ma_engine_init(nullptr, &m_audioEngine);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to initialize audio engine");
    return;
  }
  ma_engine_listener_set_position(&m_audioEngine, 0, 0, 0, 0);
  LOG_INFO("Audio engine initialized");
}

void AudioEngine::shutdown() {
  ma_engine_uninit(&m_audioEngine);
  LOG_INFO("Audio engine shutdown");
}

ma_sound AudioEngine::loadAudioFromFile(const char *file) {
  ma_sound sound;
  ma_result result = ma_sound_init_from_file(&m_audioEngine, file, 0, nullptr,
                                             nullptr, &sound);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to load audio file: %s", file);
  }
  return sound;
}

} // namespace componeng::resources
