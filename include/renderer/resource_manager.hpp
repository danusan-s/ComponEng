#pragma once

#include <map>
#include <string>

#include "renderer/mesh.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"

// A static singleton ResourceManager class that hosts several
// functions to load Textures and Shaders. Each loaded texture
// and/or shader is also stored for future reference by string
// handles. All functions and resources are static and no
// public constructor is defined.
class ResourceManager {
public:
  static std::map<std::string, Shader> s_shaders;
  static std::map<std::string, Texture2D> s_textures;
  static std::map<std::string, Mesh> s_meshes;

  static void loadShader(const char* vShaderFile, const char* fShaderFile,
                         const char* gShaderFile, std::string name);
  static const Shader& getShader(std::string name);
  static void loadTexture(const char* file, bool alpha, std::string name);
  static const Texture2D& getTexture(std::string name);
  static bool textureExists(std::string name);
  static void addMesh(std::string name, Mesh& mesh);
  static void loadMesh(const char* file, std::string name);
  static const Mesh& getMesh(std::string name);
  static void clear();

private:
  ResourceManager() {}

  static Shader loadShaderFromFile(const char* vShaderFile,
                                   const char* fShaderFile,
                                   const char* gShaderFile = nullptr);
  static Texture2D loadTextureFromFile(const char* file, bool alpha);
  static Mesh loadMeshFromFile(const char* file);
};
