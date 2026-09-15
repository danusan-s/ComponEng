#include "componeng/audio/audio_system.hpp"

#include "componeng/audio/audio_component.hpp"
#include "componeng/audio/audio_manager.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"

#include <cmath>

namespace componeng::audio {

void AudioSystem::onUpdate(const ecs::SystemState &state) {
  auto &audioManager = state.world->getResource<AudioManager>();

  auto &mainCamera = state.world->getResource<camera::MainCamera>();
  if (state.world->hasComponent<componeng::core::TransformComponent>(
          mainCamera.entity)) {
    auto &camTransform =
        state.world->getComponent<componeng::core::TransformComponent>(
            mainCamera.entity);
    audioManager.setListenerPosition(camTransform.position.x,
                                     camTransform.position.y,
                                     camTransform.position.z);

    float cosYaw = cos(core::radians(camTransform.rotation.y));
    float sinYaw = sin(core::radians(camTransform.rotation.y));
    float cosPitch = cos(core::radians(camTransform.rotation.x));
    float sinPitch = sin(core::radians(camTransform.rotation.x));
    core::Vec3 front;
    front.x = cosYaw * cosPitch;
    front.y = sinPitch;
    front.z = sinYaw * cosPitch;
    front = core::normalize(front);
    core::Vec3 up = core::normalize(
        core::cross(core::cross(front, core::Vec3(0.0f, 1.0f, 0.0f)), front));
    audioManager.setListenerOrientation(front.x, front.y, front.z, up.x, up.y,
                                        up.z);
  }

  state.world->query<AudioComponent, core::TransformComponent>().each(
      [&](AudioComponent &audio, core::TransformComponent &transform) {
        if (audio.audioID == core::INVALID_HANDLE) {
          audio.audioID = audioManager.createSound(audio.audioName);
        }
        auto &sound = audioManager.getSound(audio.audioID);

        if (sound.isPlaying() && audio.is3D) {
          sound.setPosition(transform.position.x, transform.position.y,
                            transform.position.z);
          return;
        }

        if (audio.is3D) {
          sound
              .setPosition(transform.position.x, transform.position.y,
                           transform.position.z)
              .set3D(audio.minDistance, audio.maxDistance);
        }

        sound.setVolume(audio.volume)
            .setPitch(audio.pitch)
            .setLooping(audio.loop ? MA_TRUE : MA_FALSE);

        if (audioManager.playSound(audio.audioID)) {
          audio.isPlaying = true;
        }
      });

  audioManager.cleanupFinishedSounds();
}

} // namespace componeng::audio
