#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Describes the layout of a single vertex attribute (e.g. position, normal, UV).
 *
 * Each attribute has a semantic name, a byte offset into the vertex, a component count,
 * and whether the data should be normalized when consumed by the shader.
 */
struct VertexAttribute {
  std::string name;
  uint32_t offset;
  uint32_t componentCount; // 1, 2, 3, or 4
  bool normalized;
};

/**
 * @brief Describes the full vertex layout: a list of attributes and the total stride.
 *
 * The stride is the byte offset between consecutive vertices. Each attribute's
 * offset is relative to the start of a vertex.
 */
struct VertexLayout {
  std::vector<VertexAttribute> attributes;
  uint32_t stride;
};

/**
 * @brief Convenience factory for the default mesh vertex layout:
 *        position (3 floats), normal (3 floats), uv (2 floats).
 *        Stride = 32 bytes, 8 floats per vertex.
 */
inline VertexLayout defaultMeshLayout() {
  return VertexLayout{
      .attributes = {
          {"position", 0, 3, false},
          {"normal", 12, 3, false},
          {"uv", 24, 2, false},
      },
      .stride = 32,
  };
}

/**
 * @brief API-agnostic buffer interface.
 *
 * Represents a GPU buffer (vertex, index, uniform, or instance data).
 */
class IBuffer {
public:
  virtual ~IBuffer() = default;

  /** Upload data to the buffer. Resizes if needed. */
  virtual void setData(const void* data, size_t sizeBytes) = 0;

  /** Upload a sub-region of the buffer (for dynamic updates). */
  virtual void setSubData(size_t offset, const void* data, size_t sizeBytes) = 0;

  /** Bind the buffer for use. */
  virtual void bind() = 0;

  /** Unbind the buffer. */
  virtual void unbind() = 0;

  /** Release GPU resources. */
  virtual void release() = 0;
};

/**
 * @brief API-agnostic shader interface.
 *
 * Wraps a compiled shader program with typed uniform setters.
 */
class IShader {
public:
  virtual ~IShader() = default;

  /** Compile from source strings. */
  virtual void compile(const char* vertexSource, const char* fragmentSource,
                       const char* geometrySource = nullptr) = 0;

  /** Activate this shader for subsequent uniform/draw calls. */
  virtual void use() const = 0;

  /** Release GPU resources. */
  virtual void release() = 0;

  // Uniform setters
  virtual void setFloat(const char* name, float value) const = 0;
  virtual void setInteger(const char* name, int value) const = 0;
  virtual void setVector2f(const char* name, float x, float y) const = 0;
  virtual void setVector3f(const char* name, float x, float y, float z) const = 0;
  virtual void setVector4f(const char* name, float x, float y, float z,
                           float w) const = 0;
  virtual void setMatrix4(const char* name, const float* matrix) const = 0;
};

/**
 * @brief API-agnostic 2D texture interface.
 */
class ITexture {
public:
  struct Config {
    uint32_t width;
    uint32_t height;
    bool alpha;
  };

  virtual ~ITexture() = default;

  /** Create the GPU texture from pixel data (RGBA or RGB, 8-bit). */
  virtual void generate(uint32_t width, uint32_t height,
                        const unsigned char* data, bool alpha) = 0;

  /** Bind this texture to the active texture unit. */
  virtual void bind() const = 0;

  /** Release GPU resources. */
  virtual void release() = 0;

  uint32_t width() const { return m_width; }
  uint32_t height() const { return m_height; }

protected:
  uint32_t m_width = 0;
  uint32_t m_height = 0;
};

/**
 * @brief API-agnostic mesh interface.
 *
 * Holds vertex and index data along with a vertex layout descriptor.
 * The backend implementation uploads to GPU and manages the VAO equivalent.
 */
class IMesh {
public:
  virtual ~IMesh() = default;

  /** Upload vertex and index data to the GPU using the given layout. */
  virtual void upload(const float* vertices, size_t vertexCount,
                      const uint32_t* indices, size_t indexCount,
                      const VertexLayout& layout) = 0;

  /** Bind the mesh for rendering. */
  virtual void bind() const = 0;

  /** Release GPU resources. */
  virtual void release() = 0;

  /** Number of indices in the index buffer. */
  virtual size_t indexCount() const = 0;
};

/**
 * @brief API-agnostic render device interface.
 *
 * Manages the lifecycle of the rendering API: initialization, state,
 * clear, and presentation. Also acts as a factory for buffers, shaders,
 * textures, and meshes.
 */
class IRenderDevice {
public:
  virtual ~IRenderDevice() = default;

  /** Initialize the rendering API (e.g. glad, GL state). */
  virtual void init(void* windowHandle) = 0;

  /** Set the viewport rectangle. */
  virtual void setViewport(int x, int y, int w, int h) = 0;

  /** Set the scissor rectangle. */
  virtual void setScissor(int x, int y, int w, int h) = 0;

  /** Clear color and depth buffers. */
  virtual void clear(float r, float g, float b, float a) = 0;

  /** Swap front/back buffers (presentation). */
  virtual void present(void* windowHandle) = 0;

  /** Check for API errors and log them. Returns error count. */
  virtual int checkErrors() const = 0;

  // Factory methods
  virtual std::unique_ptr<IShader> createShader() = 0;
  virtual std::unique_ptr<ITexture> createTexture() = 0;
  virtual std::unique_ptr<IMesh> createMesh() = 0;
  virtual std::unique_ptr<IBuffer> createBuffer() = 0;

  // Instance attribute setup (called once per batch to configure VAO attributes)
  virtual void setupInstanceAttributes(const void* meshHandle,
                                       IBuffer& instanceBuffer) = 0;
  virtual void unbindInstanceAttributes(const void* meshHandle) = 0;

  // Draw call
  virtual void drawIndexedInstanced(size_t indexCount,
                                    uint32_t instanceCount) = 0;
};
