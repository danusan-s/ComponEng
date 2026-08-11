#pragma once

#include "componeng/audio/audio_engine.hpp"
#include "componeng/core/types.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset/mesh.hpp"
#include "componeng/renderer/asset/shader.hpp"
#include "componeng/renderer/asset/texture.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace componeng::renderer {

/**
 * @brief Asset manager for shaders, textures, and meshes.
 *
 * Loads assets from disk on first request, caches them by string handle,
 * and returns const references on subsequent lookups. All access is through
 * static methods — no public constructor exists.
 */
class AssetManager {
public:
  AssetManager() = default;
  ~AssetManager() = default;

  // Non-copyable, Movable
  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  AssetManager(AssetManager &&) = default;
  AssetManager &operator=(AssetManager &&) = default;

  audio::AudioEngine *m_audioEngine = nullptr;

  void loadShader(const char *vShaderFile, const char *fShaderFile,
                  const char *gShaderFile, core::Name name);
  const Shader &getShader(core::HandleID id) const;
  core::HandleID getShaderID(core::Name name) const;

  void loadTexture(const char *file, bool alpha, core::Name name);
  const Texture2D &getTexture(core::HandleID id) const;
  core::HandleID getTextureID(core::Name name) const;
  bool textureExists(core::Name name) const;

  template <typename T>
  void registerMaterial(const char *name, const char *shaderName,
                        const char *textureName) {
    core::HandleID shaderID = getShaderID(shaderName);
    core::HandleID textureID = getTextureID(textureName);
    auto material = std::make_unique<T>(shaderID, textureID);

    const Shader &shader = getShader(shaderID);
    material->setVertexLayout(shader.reflectInstanceLayout());
    material->setKnownUniforms(shader.reflectActiveUniformNames());

    m_materialResources.push_back(std::move(material));
    core::HandleID id = m_materialResources.size();
    m_materials[name] = id;
  }

  core::HandleID getMaterialID(core::Name name) const;
  IMaterial &getMaterial(core::HandleID id) const;

  void addMesh(core::Name name, std::unique_ptr<Mesh> mesh);
  void loadMesh(const char *file, core::Name name);
  const Mesh &getMesh(core::HandleID id) const;
  core::HandleID getMeshID(core::Name name) const;

  void loadAudio(const char *file, core::Name name);
  core::HandleID getAudioID(core::Name name) const;
  const char *getAudio(core::HandleID id) const;
  void clear();

private:
  std::unordered_map<core::Name, core::HandleID> m_shaders;
  std::unordered_map<core::Name, core::HandleID> m_textures;
  std::unordered_map<core::Name, core::HandleID> m_materials;
  std::unordered_map<core::Name, core::HandleID> m_meshes;
  std::unordered_map<core::Name, core::HandleID> m_audios;

  std::vector<std::unique_ptr<Shader>> m_shaderResources;
  std::vector<std::unique_ptr<Texture2D>> m_textureResources;
  std::vector<std::unique_ptr<IMaterial>> m_materialResources;
  std::vector<std::unique_ptr<Mesh>> m_meshResources;
  std::vector<std::string> m_audioPaths;

  std::unique_ptr<Shader> loadShaderFromFile(const char *vShaderFile,
                                             const char *fShaderFile,
                                             const char *gShaderFile = nullptr);
  std::unique_ptr<Texture2D> loadTextureFromFile(const char *file, bool alpha);
  std::unique_ptr<Mesh> loadMeshFromFile(const char *file);
};

} // namespace componeng::renderer
