#include "componeng/core/types.hpp"
#define MINIAUDIO_IMPLEMENTATION

#include "componeng/audio/audio_engine.hpp"

#include "componeng/utils/logger.hpp"
#include <miniaudio.h>

namespace componeng::audio {

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
  for (auto &soundPair : m_activeSounds) {
    ma_sound_uninit(soundPair.second.get());
  }
  ma_engine_uninit(&m_audioEngine);
  LOG_INFO("Audio engine shutdown");
}

void AudioEngine::setListenerPosition(float x, float y, float z) {
  ma_engine_listener_set_position(&m_audioEngine, 0, x, y, z);
}

void AudioEngine::setListenerOrientation(float forwardX, float forwardY,
                                         float forwardZ, float upX, float upY,
                                         float upZ) {
  ma_engine_listener_set_direction(&m_audioEngine, 0, forwardX, forwardY,
                                   forwardZ);
  ma_engine_listener_set_world_up(&m_audioEngine, 0, upX, upY, upZ);
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

void AudioEngine::updateSoundPosition(core::HandleID id, float x, float y,
                                      float z) {
  auto it = m_activeSounds.find(id);
  if (it != m_activeSounds.end()) {
    ma_sound *sound = it->second.get();
    ma_sound_set_position(sound, x, y, z);
  } else {
    LOG_ERROR("Sound with ID %u not found", id);
  }
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

bool AudioEngine::playSound(core::HandleID id,
                            std::unique_ptr<ma_sound> sound) {
  ma_result result = ma_sound_start(sound.get());
  m_activeSounds[id] = (std::move(sound));
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to play sound");
    return false;
  }
  return true;
}

void AudioEngine::cleanupFinishedSounds() {
  for (auto i = m_activeSounds.begin(); i != m_activeSounds.end();) {
    auto &sound = i->second;

    if (!ma_sound_is_playing(sound.get())) {
      ma_sound_uninit(sound.get());
      m_activeSounds.erase(i);
    } else {
      ++i;
    }
  }
}

} // namespace componeng::audio
