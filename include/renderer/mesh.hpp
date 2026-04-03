#pragma once

#include "glad/glad.h"
#include <string>
#include <vector>

/**
 * @brief OpenGL mesh resource with vertex/index buffers.
 *
 * Parses Wavefront OBJ data, generates VAO/VBO/EBO, and stores
 * raw vertex and index arrays for GPU rendering.
 */
class Mesh {
public:
  GLuint m_vao;
  GLuint m_vbo;
  GLuint m_ebo;
  std::vector<float> m_vertices;
  std::vector<unsigned int> m_indices;

  Mesh();

  void initializeBuffers();
  void generateFromWavefrontObj(const std::string& data);
};
