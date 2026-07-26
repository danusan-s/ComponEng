#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct AudioComponent {
  static constexpr const char* component_name = "AudioComponent";

  renderer::AudioID audioID;
  const char *audioName = nullptr; // Optional: store name for serialization
  bool playOnAwake = true;
  bool loop = false;
  bool isPlaying = false;
  float volume = 1.0f;
  float pitch = 1.0f;
  bool is3D = false;
  float minDistance = 1.0f;
  float maxDistance = 100.0f;
};

template <> struct ComponentSerializer<AudioComponent> {
  static nlohmann::json serialize(const AudioComponent &component) {
    nlohmann::json audioJson;

    audioJson["audioName"] =
        component.audioName ? std::string(component.audioName) : "";
    audioJson["playOnAwake"] = component.playOnAwake;
    audioJson["loop"] = component.loop;
    audioJson["volume"] = component.volume;
    audioJson["pitch"] = component.pitch;
    audioJson["is3D"] = component.is3D;
    audioJson["minDistance"] = component.minDistance;
    audioJson["maxDistance"] = component.maxDistance;

    return audioJson;
  }

  static AudioComponent deserialize(const nlohmann::json &audioJson) {
    AudioComponent component;

    component.audioID =
        renderer::AudioID(); // Will be set later based on audioName
    std::string audioName = audioJson.value("audioName", std::string());
    LOG_INFO("Deserializing AudioComponent with audioName: %s",
             audioName.c_str());
    if (!audioName.empty()) {
      component.audioName = strdup(audioName.c_str());
    }
    component.playOnAwake = audioJson.value("playOnAwake", true);
    component.loop = audioJson.value("loop", false);
    component.volume = audioJson.value("volume", 1.0f);
    component.pitch = audioJson.value("pitch", 1.0f);
    component.is3D = audioJson.value("is3D", false);
    component.minDistance = audioJson.value("minDistance", 1.0f);
    component.maxDistance = audioJson.value("maxDistance", 100.0f);

    return component;
  }
};

} // namespace componeng::components
