#include "renderer/model.hpp"
#include <array>
#include <iostream>
#include <sstream>

Model::Model() : VAO(0), VBO(0), EBO(0) {
}

void Model::Generate(const std::string &data) {
  std::cout << "> Generating model" << std::endl;
  std::istringstream stream(data);
  std::string line;

  std::vector<std::array<float, 3>> positions;
  std::vector<std::array<float, 3>> normals;
  std::vector<std::array<float, 2>> texCoords;

  while (std::getline(stream, line)) {
    std::istringstream lineStream(line);
    std::string prefix;
    lineStream >> prefix;

    if (prefix == "v") {
      float x, y, z;
      lineStream >> x >> y >> z;
      positions.push_back({x, y, z});
    } else if (prefix == "vn") {
      float nx, ny, nz;
      lineStream >> nx >> ny >> nz;
      normals.push_back({nx, ny, nz});
    } else if (prefix == "vt") {
      float u, v;
      lineStream >> u >> v;
      texCoords.push_back({u, v});
    } else if (prefix == "f") {
      std::string vertexStr;
      int faceVertCount = 0;
      while (lineStream >> vertexStr) {
        std::istringstream vertexStream(vertexStr);
        std::string posIndexStr, texIndexStr, normIndexStr;

        std::getline(vertexStream, posIndexStr, '/');
        std::getline(vertexStream, texIndexStr, '/');
        std::getline(vertexStream, normIndexStr, '/');

        int posIndex = std::stoi(posIndexStr) - 1;
        int texIndex = texIndexStr.empty() ? -1 : std::stoi(texIndexStr) - 1;
        int normIndex = normIndexStr.empty() ? -1 : std::stoi(normIndexStr) - 1;

        const auto &pos = positions[posIndex];
        const auto &norm = normals[normIndex];
        const auto &tex = texCoords[texIndex];

        vertices.push_back(pos[0]);
        vertices.push_back(pos[1]);
        vertices.push_back(pos[2]);
        vertices.push_back(norm[0]);
        vertices.push_back(norm[1]);
        vertices.push_back(norm[2]);
        vertices.push_back(tex[0]);
        vertices.push_back(tex[1]);

        ++faceVertCount;

        // std::cout << "Added vertex: pos(" << pos[0] << ", " << pos[1] << ", "
        //           << pos[2] << ") norm(" << norm[0] << ", " << norm[1] << ",
        //           "
        //           << norm[2] << ") tex(" << tex[0] << ", " << tex[1] << ")"
        //           << std::endl;
      }
      if (faceVertCount == 3) {
        // For triangles, just add the indices
        indices.push_back((unsigned int)(vertices.size() / 8) - 3);
        indices.push_back((unsigned int)(vertices.size() / 8) - 2);
        indices.push_back((unsigned int)(vertices.size() / 8) - 1);
      } else if (faceVertCount == 4) {
        // For quads, add two triangles
        indices.push_back((unsigned int)(vertices.size() / 8) - 4);
        indices.push_back((unsigned int)(vertices.size() / 8) - 3);
        indices.push_back((unsigned int)(vertices.size() / 8) - 2);

        indices.push_back((unsigned int)(vertices.size() / 8) - 4);
        indices.push_back((unsigned int)(vertices.size() / 8) - 2);
        indices.push_back((unsigned int)(vertices.size() / 8) - 1);
      }
    }
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  // Position (location = 0)
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Normal (location = 1)
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // UV (location = 2)
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
}
