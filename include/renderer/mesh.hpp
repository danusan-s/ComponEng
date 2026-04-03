#pragma once

#include "core/types.hpp"
#include "renderer/api/irender_device.hpp"
#include <memory>
#include <string>
#include <vector>

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

  // Helper to parse OBJ data and populate m_vertices and m_indices
  void generateFromWavefrontObj(const std::string& data);

  // Access to the underlying IMesh for render device operations
  IMesh& getImpl() { return *m_impl; }
  const IMesh& getImpl() const { return *m_impl; }

  // Convenience handle for VAO-based operations (GL-specific, but needed for
  // instance attribute setup). Returns an opaque pointer the device can use.
  const void* getHandle() const;

  size_t indexCount() const;

  // Raw vertex/index data (used during parsing before GPU upload)
  std::vector<float> m_vertices;
  std::vector<unsigned int> m_indices;

private:
  std::unique_ptr<IMesh> m_impl;
};
