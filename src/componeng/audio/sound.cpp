#include "componeng/audio/sound.hpp"

namespace componeng::audio {

Sound::Sound() : m_sound(std::make_unique<ma_sound>()) {
}

Sound::~Sound() {
  if (m_sound) {
    ma_sound_uninit(m_sound.get());
  }
}

ma_sound *Sound::getSound() {
  return m_sound.get();
}

Sound &Sound::setPosition(float x, float y, float z) {
  ma_sound_set_position(m_sound.get(), x, y, z);
  return *this;
}

Sound &Sound::setVolume(float volume) {
  ma_sound_set_volume(m_sound.get(), volume);
  return *this;
}

Sound &Sound::setPitch(float pitch) {
  ma_sound_set_pitch(m_sound.get(), pitch);
  return *this;
}

Sound &Sound::setLooping(bool loop) {
  ma_sound_set_looping(m_sound.get(), loop);
  return *this;
}

Sound &Sound::set3D(float minDistance, float maxDistance) {
  ma_sound_set_min_distance(m_sound.get(), minDistance);
  ma_sound_set_max_distance(m_sound.get(), maxDistance);
  ma_sound_set_attenuation_model(m_sound.get(), ma_attenuation_model_linear);
  return *this;
}

bool Sound::play() {
  return ma_sound_start(m_sound.get()) == MA_SUCCESS;
}

bool Sound::stop() {
  return ma_sound_stop(m_sound.get()) == MA_SUCCESS;
}

bool Sound::isPlaying() const {
  return ma_sound_is_playing(m_sound.get()) == MA_TRUE;
}

} // namespace componeng::audio
