#ifndef MODEL_H
#define MODEL_H

#include "glad/glad.h"
#include <string>
#include <vector>

// Parser and initializer for WavefrontOBJ files to be used as 3D models.
class Model {
public:
  GLuint VAO, VBO, EBO;
  // Holds x y z nx ny nz u v for each vertex
  std::vector<float> vertices;
  std::vector<unsigned int> indices;

  Model();

  // generates model from obj data
  void Generate(const std::string &data);
};

#endif
