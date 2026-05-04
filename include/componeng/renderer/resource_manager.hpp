#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "componeng/renderer/mesh.hpp"
#include "componeng/renderer/shader.hpp"
#include "componeng/renderer/texture.hpp"

using ShaderID = uint32_t;
using TextureID = uint32_t;
using MeshID = uint32_t;

/**
 * @brief Static asset manager for shaders, textures, and meshes.
 *
 * Loads assets from disk on first request, caches them by string handle,
 * and returns const references on subsequent lookups. All access is through
 * static methods — no public constructor exists.
 */
class ResourceManager {
public:
  static std::unordered_map<std::string, ShaderID> s_shaders;
  static std::unordered_map<std::string, TextureID> s_textures;
  static std::unordered_map<std::string, MeshID> s_meshes;

  static std::unordered_map<ShaderID, std::unique_ptr<Shader>>
      s_shaderResources;
  static std::unordered_map<TextureID, std::unique_ptr<Texture2D>>
      s_textureResources;
  static std::unordered_map<MeshID, std::unique_ptr<Mesh>> s_meshResources;

  static void loadShader(const char *vShaderFile, const char *fShaderFile,
                         const char *gShaderFile, std::string name);
  static const Shader &getShader(ShaderID id);
  static ShaderID getShaderID(std::string name);
  static void loadTexture(const char *file, bool alpha, std::string name);
  static const Texture2D &getTexture(TextureID id);
  static TextureID getTextureID(std::string name);
  static bool textureExists(std::string name);
  static void addMesh(std::string name, std::unique_ptr<Mesh> mesh);
  static void loadMesh(const char *file, std::string name);
  static const Mesh &getMesh(MeshID id);
  static MeshID getMeshID(std::string name);
  static void clear();

private:
  ResourceManager() {
  }

  static std::unique_ptr<Shader>
  loadShaderFromFile(const char *vShaderFile, const char *fShaderFile,
                     const char *gShaderFile = nullptr);
  static std::unique_ptr<Texture2D> loadTextureFromFile(const char *file,
                                                        bool alpha);
  static std::unique_ptr<Mesh> loadMeshFromFile(const char *file);

  static uint32_t nextShaderID;
  static uint32_t nextTextureID;
  static uint32_t nextMeshID;
};
