#include "resource_manager.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Instantiate static variables
std::map<std::string, Texture2D> ResourceManager::Textures;
std::map<std::string, Shader> ResourceManager::Shaders;
std::map<std::string, Model> ResourceManager::Models;

Shader ResourceManager::LoadShader(const char *vShaderFile,
                                   const char *fShaderFile,
                                   const char *gShaderFile, std::string name) {
  std::cout << "> Loading Shader: " << name << std::endl;
  Shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
  return Shaders[name];
}

const Shader &ResourceManager::GetShader(std::string name) {
  return Shaders.at(name);
}

Texture2D ResourceManager::LoadTexture(const char *file, bool alpha,
                                       std::string name) {
  std::cout << "> Loading Texture: " << name << std::endl;
  Textures[name] = loadTextureFromFile(file, alpha);
  return Textures[name];
}

const Texture2D &ResourceManager::GetTexture(std::string name) {
  return Textures.at(name);
}

bool ResourceManager::TextureExists(std::string name) {
  return Textures.find(name) != Textures.end();
}

Model ResourceManager::LoadModel(const char *file, std::string name) {
  std::cout << "> Loading Model: " << name << std::endl;
  Models[name] = loadModelFromFile(file);
  return Models[name];
}

const Model &ResourceManager::GetModel(std::string name) {
  return Models.at(name);
}

void ResourceManager::Clear() {
  std::cout << "Deleting loaded resources" << std::endl;

  std::cout << "Attempting to delete shaders" << std::endl;
  // (properly) delete all shaders
  for (auto iter : Shaders)
    glDeleteProgram(iter.second.ID);
  std::cout << "Shaders deleted successfully" << std::endl;

  std::cout << "Attempting to delete textures" << std::endl;
  // (properly) delete all textures
  for (auto iter : Textures)
    glDeleteTextures(1, &iter.second.ID);

  std::cout << "Textures deleted successfully" << std::endl;

  std::cout << "Attempting to delete cubemaps" << std::endl;
  // (properly) delete all cubemaps
  for (auto iter : Cubemaps)
    glDeleteTextures(1, &iter.second.ID);
  std::cout << "Cubemaps deleted successfully" << std::endl;

  std::cout << "Attempting to delete models" << std::endl;
  // (properly) delete all models
  for (auto iter : Models) {
    glDeleteVertexArrays(1, &iter.second.VAO);
    glDeleteBuffers(1, &iter.second.VBO);
    glDeleteBuffers(1, &iter.second.EBO);
  }
  std::cout << "Models deleted successfully" << std::endl;
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
    std::cout << "ERROR::SHADER: Failed to read shader files" << std::endl;
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
  std::cout << "Loading texture file: " << file << std::endl;
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

Model ResourceManager::loadModelFromFile(const char *file) {
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
    std::cout << "ERROR::MODEL: Failed to read model files" << std::endl;
  }

  Model model;
  model.Generate(modelData);
  return model;
}
