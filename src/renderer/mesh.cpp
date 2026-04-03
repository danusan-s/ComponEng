#include "core/logger.hpp"
#include "renderer/mesh.hpp"
#include "renderer/opengl/gl_render_device.hpp"
#include <array>
#include <sstream>

Mesh::Mesh() {
}

void Mesh::uploadToGPU() {
  if (m_vertices.empty() || m_indices.empty()) {
    LOG_ERROR("Cannot initialize buffers for an empty mesh.");
    return;
  }

  m_impl = std::make_unique<GLMesh>();
  m_impl->upload(m_vertices.data(), m_vertices.size() / 8, m_indices.data(),
                 m_indices.size(), defaultMeshLayout());
}

void Mesh::generateFromWavefrontObj(const std::string& data) {
  LOG_INFO("Generating model");
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

        if (posIndexStr.empty() || texIndexStr.empty() ||
            normIndexStr.empty()) {
          LOG_ERROR("Invalid vertex format in OBJ file: %s.",
                    vertexStr.c_str());
          continue;
        }

        int posIndex = std::stoi(posIndexStr) - 1;
        int texIndex = std::stoi(texIndexStr) - 1;
        int normIndex = std::stoi(normIndexStr) - 1;

        if (posIndex < 0 || posIndex >= (int)positions.size()) {
          LOG_ERROR("Invalid position index in OBJ file: %s.",
                    posIndexStr.c_str());
          continue;
        }
        if (texIndex < 0 || texIndex >= (int)texCoords.size()) {
          LOG_ERROR("Invalid texture coordinate index in OBJ file: %s.",
                    texIndexStr.c_str());
          continue;
        }
        if (normIndex < 0 || normIndex >= (int)normals.size()) {
          LOG_ERROR("Invalid normal index in OBJ file: %s.",
                    normIndexStr.c_str());
          continue;
        }

        const auto& pos = positions[posIndex];
        const auto& norm = normals[normIndex];
        const auto& tex = texCoords[texIndex];

        m_vertices.push_back(pos[0]);
        m_vertices.push_back(pos[1]);
        m_vertices.push_back(pos[2]);
        m_vertices.push_back(norm[0]);
        m_vertices.push_back(norm[1]);
        m_vertices.push_back(norm[2]);
        m_vertices.push_back(tex[0]);
        m_vertices.push_back(tex[1]);

        ++faceVertCount;
      }
      if (faceVertCount == 3) {
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 3);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 2);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 1);
      } else if (faceVertCount == 4) {
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 4);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 3);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 2);

        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 4);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 2);
        m_indices.push_back((unsigned int)(m_vertices.size() / 8) - 1);
      }
    }
  }
}

const void* Mesh::getHandle() const {
  return reinterpret_cast<const void*>(
      static_cast<GLMesh&>(*m_impl).vao());
}

size_t Mesh::indexCount() const {
  return m_impl->indexCount();
}
