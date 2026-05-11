#define MINIAUDIO_IMPLEMENTATION

#include "componeng/resources/audio_engine.hpp"
#include "componeng/utils/logger.hpp"

#include <miniaudio.h>

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
  for (auto &sound : m_activeSounds) {
    ma_sound_uninit(sound.get());
  }
  ma_engine_uninit(&m_audioEngine);
  LOG_INFO("Audio engine shutdown");
}

void AudioEngine::setListenerPosition(float x, float y, float z) {
  ma_engine_listener_set_position(&m_audioEngine, 0, x, y, z);
}

std::unique_ptr<ma_sound> AudioEngine::createSound(const char *filePath) {
  std::unique_ptr<ma_sound> sound = std::make_unique<ma_sound>();
  ma_result result =
      ma_sound_init_from_file(&m_audioEngine, filePath, MA_SOUND_FLAG_DECODE,
                              nullptr, nullptr, sound.get());

  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to create sound from file: %s", filePath);
    return nullptr;
  }
  return sound;
}

void AudioEngine::setSoundPosition(ma_sound *sound, float x, float y, float z) {
  ma_sound_set_position(sound, x, y, z);
}

void AudioEngine::setSoundSettings(ma_sound *sound, float volume, float pitch,
                                   bool loop) {
  ma_sound_set_volume(sound, volume);
  ma_sound_set_pitch(sound, pitch);
  ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
}

void AudioEngine::setSound3D(ma_sound *sound, float minDistance,
                             float maxDistance) {
  ma_sound_set_min_distance(sound, minDistance);
  ma_sound_set_max_distance(sound, maxDistance);
  ma_sound_set_attenuation_model(sound, ma_attenuation_model_linear);
}

bool AudioEngine::playSound(std::unique_ptr<ma_sound> sound) {
  ma_result result = ma_sound_start(sound.get());
  m_activeSounds.push_back(std::move(sound));
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to play sound");
    return false;
  }
  return true;
}

bool AudioEngine::playSoundFromFile(const char *filePath, float x, float y,
                                    float z, float volume, float pitch,
                                    bool loop, float minDistance,
                                    float maxDistance) {
  auto sound = createSound(filePath);
  if (!sound) {
    return false;
  }
  setSoundPosition(sound.get(), x, y, z);
  setSoundSettings(sound.get(), volume, pitch, loop);
  setSound3D(sound.get(), minDistance, maxDistance);
  return playSound(std::move(sound));
}

void AudioEngine::cleanupFinishedSounds() {
  for (size_t i = 0; i < m_activeSounds.size();) {
    auto &sound = m_activeSounds[i];

    if (!ma_sound_is_playing(sound.get())) {
      ma_sound_uninit(sound.get());

      m_activeSounds[i] = std::move(m_activeSounds.back());
      m_activeSounds.pop_back();
    } else {
      ++i;
    }
  }
}

} // namespace componeng::resources
