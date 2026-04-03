#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"

/**
 * @brief Static asset manager for shaders, textures, and meshes.
 *
 * Loads assets from disk on first request, caches them by string handle,
 * and returns const references on subsequent lookups. All access is through
 * static methods — no public constructor exists.
 */
class ResourceManager {
public:
  static std::unordered_map<std::string, std::unique_ptr<Shader>> s_shaders;
  static std::unordered_map<std::string, std::unique_ptr<Texture2D>> s_textures;
  static std::unordered_map<std::string, std::unique_ptr<Mesh>> s_meshes;

  static void loadShader(const char* vShaderFile, const char* fShaderFile,
                         const char* gShaderFile, std::string name);
  static const Shader& getShader(std::string name);
  static void loadTexture(const char* file, bool alpha, std::string name);
  static const Texture2D& getTexture(std::string name);
  static bool textureExists(std::string name);
  static void addMesh(std::string name, std::unique_ptr<Mesh> mesh);
  static void loadMesh(const char* file, std::string name);
  static const Mesh& getMesh(std::string name);
  static void clear();

private:
  ResourceManager() {
  }

  static std::unique_ptr<Shader> loadShaderFromFile(const char* vShaderFile,
                                                     const char* fShaderFile,
                                                     const char* gShaderFile = nullptr);
  static std::unique_ptr<Texture2D> loadTextureFromFile(const char* file, bool alpha);
  static std::unique_ptr<Mesh> loadMeshFromFile(const char* file);
};
