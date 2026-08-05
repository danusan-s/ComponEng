#include "componeng/audio/audio_system.hpp"

#include "componeng/audio/audio_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"

#include <cmath>

namespace componeng::audio {

void AudioSystem::onUpdate(const ecs::SystemState &state) {
  auto &assetManager = state.world->getResource<renderer::AssetManager>();
  auto &audioEngine = state.world->getResource<AudioEngine>();

  auto &mainCamera = state.world->getResource<camera::MainCamera>();
  if (state.world->hasComponent<componeng::core::TransformComponent>(
          mainCamera.entity)) {
    auto &camTransform =
        state.world->getComponent<componeng::core::TransformComponent>(
            mainCamera.entity);
    audioEngine.setListenerPosition(camTransform.position.x,
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
    audioEngine.setListenerOrientation(front.x, front.y, front.z, up.x, up.y,
                                       up.z);
  }

  state.world->query<AudioComponent, core::TransformComponent>().each(
      [&](AudioComponent &audio, core::TransformComponent &transform) {
        if (audio.audioID == 0) {
          audio.audioID = assetManager.getAudioID(audio.audioName);
        }
        if (audio.isPlaying && audio.is3D) {
          audioEngine.updateSoundPosition(audio.audioID, transform.position.x,
                                          transform.position.y,
                                          transform.position.z);
          return;
        }

        auto soundPath = assetManager.getAudio(audio.audioID);
        auto sound = audioEngine.createSound(soundPath);

        if (audio.is3D) {
          audioEngine.setSoundPosition(sound.get(), transform.position.x,
                                       transform.position.y,
                                       transform.position.z);
          audioEngine.setSound3D(sound.get(), audio.minDistance,
                                 audio.maxDistance);
        }

        audioEngine.setSoundSettings(sound.get(), audio.volume, audio.pitch,
                                     audio.loop ? MA_TRUE : MA_FALSE);

        if (audioEngine.playSound(audio.audioID, std::move(sound))) {
          audio.isPlaying = true;
        }
      });

  audioEngine.cleanupFinishedSounds();
}

} // namespace componeng::audio
