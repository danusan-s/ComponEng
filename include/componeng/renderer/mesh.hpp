#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include <memory>
#include <string>
#include <vector>

namespace componeng::renderer {

/**
 * @brief Mesh resource with vertex/index buffers.
 *
 * Parses Wavefront OBJ data and uploads to the GPU via the IMesh interface.
 * The underlying GPU resource is owned by this class.
 */
class Mesh {
public:
  Mesh();

  void uploadToGPU();

  // Access to the underlying IMesh for render device operations
  IMesh& getImpl() { return *m_impl; }
  const IMesh& getImpl() const { return *m_impl; }

  size_t indexCount() const;

  // Raw vertex/index data (used during parsing before GPU upload)
  std::vector<float> m_vertices;
  std::vector<unsigned int> m_indices;

private:
  std::unique_ptr<IMesh> m_impl;
};

} // namespace componeng::renderer
