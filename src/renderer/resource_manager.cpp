#include "core/logger.hpp"
#include "renderer/resource_manager.hpp"
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Instantiate static variables
std::map<std::string, Texture2D> ResourceManager::Textures;
std::map<std::string, Shader> ResourceManager::Shaders;
std::map<std::string, Mesh> ResourceManager::Meshes;

void ResourceManager::LoadShader(const char *vShaderFile,
                                 const char *fShaderFile,
                                 const char *gShaderFile, std::string name) {
  LOG_INFO("Loading Shader: %s", name.c_str());
  Shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
}

const Shader &ResourceManager::GetShader(std::string name) {
  return Shaders.at(name);
}

void ResourceManager::LoadTexture(const char *file, bool alpha,
                                  std::string name) {
  LOG_INFO("Loading Texture: %s", name.c_str());
  Textures[name] = loadTextureFromFile(file, alpha);
}

const Texture2D &ResourceManager::GetTexture(std::string name) {
  return Textures.at(name);
}

bool ResourceManager::TextureExists(std::string name) {
  return Textures.find(name) != Textures.end();
}

void ResourceManager::AddMesh(std::string name, Mesh &mesh) {
  LOG_INFO("Adding Mesh: %s", name.c_str());
  mesh.InitializeBuffers();
  Meshes[name] = mesh;
}

void ResourceManager::LoadMesh(const char *file, std::string name) {
  LOG_INFO("Loading Model: %s", name.c_str());
  Mesh mesh = loadMeshFromFile(file);
  AddMesh(name, mesh);
}

const Mesh &ResourceManager::GetMesh(std::string name) {
  return Meshes.at(name);
}

void ResourceManager::Clear() {
  LOG_INFO("Deleting loaded resources");

  LOG_INFO("Attempting to delete shaders");
  for (auto iter : Shaders)
    glDeleteProgram(iter.second.ID);
  LOG_INFO("Shaders deleted successfully");

  LOG_INFO("Attempting to delete textures");
  for (auto iter : Textures)
    glDeleteTextures(1, &iter.second.ID);

  LOG_INFO("Textures deleted successfully");

  LOG_INFO("Attempting to delete models");
  for (auto iter : Meshes) {
    glDeleteVertexArrays(1, &iter.second.VAO);
    glDeleteBuffers(1, &iter.second.VBO);
    glDeleteBuffers(1, &iter.second.EBO);
  }
  LOG_INFO("Models deleted successfully");
}

Shader ResourceManager::loadShaderFromFile(const char *vShaderFile,
                                           const char *fShaderFile,
                                           const char *gShaderFile) {
  // 1. retrieve the vertex/fragment source code from filePath
  std::string vertexCode;
  std::string fragmentCode;
  std::string geometryCode;
  try {
    // open files
    std::ifstream vertexShaderFile(vShaderFile);
    std::ifstream fragmentShaderFile(fShaderFile);
    std::stringstream vShaderStream, fShaderStream;
    // read file's buffer contents into streams
    vShaderStream << vertexShaderFile.rdbuf();
    fShaderStream << fragmentShaderFile.rdbuf();
    // close file handlers
    vertexShaderFile.close();
    fragmentShaderFile.close();
    // convert stream into string
    vertexCode = vShaderStream.str();
    fragmentCode = fShaderStream.str();
    // if geometry shader path is present, also load a geometry shader
    if (gShaderFile != nullptr) {
      std::ifstream geometryShaderFile(gShaderFile);
      std::stringstream gShaderStream;
      gShaderStream << geometryShaderFile.rdbuf();
      geometryShaderFile.close();
      geometryCode = gShaderStream.str();
    }
  } catch (const std::exception &e) {
    LOG_ERROR("ERROR::SHADER: Failed to read shader files");
  }
  const char *vShaderCode = vertexCode.c_str();
  const char *fShaderCode = fragmentCode.c_str();
  const char *gShaderCode = geometryCode.c_str();
  // 2. now create shader object from source code
  Shader shader;
  shader.Compile(vShaderCode, fShaderCode,
                 gShaderFile != nullptr ? gShaderCode : nullptr);
  return shader;
}

Texture2D ResourceManager::loadTextureFromFile(const char *file, bool alpha) {
  // create texture object
  Texture2D texture;
  if (alpha) {
    texture.Internal_Format = GL_RGBA;
    texture.Image_Format = GL_RGBA;
  }
  LOG_INFO("Loading texture file: %s", file);
  // load image
  int width, height, nrChannels;
  unsigned char *data =
      stbi_load(file, &width, &height, &nrChannels, alpha ? 4 : 3);
  // now generate texture
  // Error in generating ?
  texture.Generate(width, height, data);
  // and finally free image data
  stbi_image_free(data);
  return texture;
}

Mesh ResourceManager::loadMeshFromFile(const char *file) {
  std::string modelData;
  try {
    // open files
    std::ifstream modelWavefrontObjFile(file);
    std::stringstream wavefrontObjStream;
    // read file's buffer contents into streams
    wavefrontObjStream << modelWavefrontObjFile.rdbuf();
    // close file handlers
    modelWavefrontObjFile.close();
    // convert stream into string
    modelData = wavefrontObjStream.str();
  } catch (const std::exception &e) {
    LOG_ERROR("ERROR::MODEL: Failed to read model files");
  }

  Mesh model;
  model.GenerateFromWavefrontObj(modelData);
  return model;
}
