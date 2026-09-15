#define MINIAUDIO_IMPLEMENTATION

#include "componeng/audio/audio_manager.hpp"
#include "componeng/core/types.hpp"

#include "componeng/utils/logger.hpp"
#include <miniaudio.h>

namespace componeng::audio {

void AudioManager::init() {
  ma_result result = ma_engine_init(nullptr, &m_audioEngine);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to initialize audio engine");
    return;
  }
  ma_engine_listener_set_position(&m_audioEngine, 0, 0, 0, 0);
  LOG_INFO("Audio engine initialized");
}

void AudioManager::shutdown() {
  m_activeSounds.clear();
  m_soundInstances.clear();
  m_soundResources.clear();
  ma_engine_uninit(&m_audioEngine);
  LOG_INFO("Audio engine shutdown");
}

void AudioManager::setListenerPosition(float x, float y, float z) {
  ma_engine_listener_set_position(&m_audioEngine, 0, x, y, z);
}

void AudioManager::setListenerOrientation(float forwardX, float forwardY,
                                          float forwardZ, float upX, float upY,
                                          float upZ) {
  ma_engine_listener_set_direction(&m_audioEngine, 0, forwardX, forwardY,
                                   forwardZ);
  ma_engine_listener_set_world_up(&m_audioEngine, 0, upX, upY, upZ);
}

void AudioManager::registerSound(const char *filePath, core::Name name) {
  Sound sound;
  ma_result result =
      ma_sound_init_from_file(&m_audioEngine, filePath, MA_SOUND_FLAG_DECODE,
                              nullptr, nullptr, sound.getSound());

  if (result != MA_SUCCESS) {
    if (filePath) {
      LOG_ERROR("Failed to register sound from file: %s", filePath);
    } else {
      LOG_ERROR("Failed to register sound from file: (null)");
    }
    return;
  }
  m_soundResources[name] = std::move(sound);
}

core::HandleID AudioManager::createSound(core::Name name) {
  Sound sound;
  ma_result result =
      ma_sound_init_copy(&m_audioEngine, m_soundResources[name].getSound(), 0,
                         nullptr, sound.getSound());

  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to create sound");
    return 0;
  }

  if (!m_finishedSounds.empty()) {
    core::HandleID id = m_finishedSounds.front();
    m_finishedSounds.pop();
    m_soundInstances[id] = std::move(sound);
    return id;
  }

  m_soundInstances.push_back(std::move(sound));
  return m_soundInstances.size() - 1;
}

Sound &AudioManager::getSound(core::HandleID id) {
  if (id < m_soundInstances.size()) {
    return m_soundInstances[id];
  }
  LOG_ERROR("Sound with ID %d not found", id);
  throw std::out_of_range("Sound ID out of range");
}

bool AudioManager::playSound(core::HandleID id) {
  if (id < m_soundInstances.size()) {
    auto &sound = m_soundInstances[id];
    if (sound.play()) {
      m_activeSounds.push_back(id);
      return true;
    } else {
      LOG_ERROR("Failed to play sound with ID %d", id);
      return false;
    }
  }
  LOG_ERROR("Sound with ID %d not found", id);
  return false;
}

void AudioManager::cleanupFinishedSounds() {
  for (auto i = m_activeSounds.begin(); i != m_activeSounds.end();) {
    auto &sound = m_soundInstances[*i];

    if (!sound.isPlaying()) {
      m_activeSounds.erase(i);
      m_finishedSounds.push(*i);
    } else {
      ++i;
    }
  }
}

} // namespace componeng::audio
