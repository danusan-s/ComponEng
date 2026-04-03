#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Describes the layout of a single vertex attribute.
 */
struct VertexAttribute {
  std::string name;
  uint32_t offset;
  uint32_t componentCount;
  bool normalized;
};

/**
 * @brief Describes the full vertex layout: attributes list + stride in bytes.
 */
struct VertexLayout {
  std::vector<VertexAttribute> attributes;
  uint32_t stride;
};

/**
 * @brief Default mesh vertex layout: position (3), normal (3), uv (2).
 *        Stride = 32 bytes.
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
  enum class Usage { Static, Dynamic };

  virtual ~IBuffer() = default;

  /** Upload data to the buffer. */
  virtual void setData(const void* data, size_t sizeBytes, Usage usage = Usage::Dynamic) = 0;

  /** Upload a sub-region of the buffer (for dynamic updates). */
  virtual void setSubData(size_t offset, const void* data, size_t sizeBytes) = 0;

  /** Release GPU resources. */
  virtual void release() = 0;
};

/**
 * @brief API-agnostic shader interface.
 *
 * Wraps a compiled shader program with typed uniform setters.
 * Backends may load GLSL source (OpenGL) or SPIR-V binaries (Vulkan).
 */
class IShader {
public:
  virtual ~IShader() = default;

  /** Load shader from GLSL source strings (OpenGL path). */
  virtual void loadGLSL(const char* vertexSource, const char* fragmentSource,
                        const char* geometrySource = nullptr) = 0;

  /** Load shader from SPIR-V binary files (Vulkan path). */
  virtual void loadSPIRV(const char* vertexPath, const char* fragmentPath,
                         const char* geometryPath = nullptr) = 0;

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
  virtual ~ITexture() = default;

  /** Create the GPU texture from pixel data (RGBA or RGB, 8-bit). */
  virtual void generate(uint32_t width, uint32_t height,
                        const unsigned char* data, bool alpha) = 0;

  /** Bind this texture to the active texture unit / descriptor set. */
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
 */
class IMesh {
public:
  virtual ~IMesh() = default;

  /** Upload vertex and index data to the GPU using the given layout. */
  virtual void upload(const float* vertices, size_t vertexCount,
                      const uint32_t* indices, size_t indexCount,
                      const VertexLayout& layout) = 0;

  /** Bind the mesh for rendering (binds VAO / vertex buffers). */
  virtual void bind() const = 0;

  /** Release GPU resources. */
  virtual void release() = 0;

  /** Number of indices in the index buffer. */
  virtual size_t indexCount() const = 0;
};

/**
 * @brief API-agnostic render device interface.
 *
 * Manages the rendering API lifecycle: initialization, state, clear,
 * presentation, and resource creation.
 */
class IRenderDevice {
public:
  virtual ~IRenderDevice() = default;

  /** Initialize the rendering API. windowHandle is a GLFWwindow*. */
  virtual void init(void* windowHandle) = 0;

  /** Set the viewport rectangle. */
  virtual void setViewport(int x, int y, int w, int h) = 0;

  /** Set the scissor rectangle. */
  virtual void setScissor(int x, int y, int w, int h) = 0;

  /** Clear color and depth buffers. */
  virtual void clear(float r, float g, float b, float a) = 0;

  /** Present the rendered frame. */
  virtual void present(void* windowHandle) = 0;

  /** Check for API errors and log them. Returns error count. */
  virtual int checkErrors() const = 0;

  // Factory methods
  virtual std::unique_ptr<IShader> createShader() = 0;
  virtual std::unique_ptr<ITexture> createTexture() = 0;
  virtual std::unique_ptr<IMesh> createMesh() = 0;
  virtual std::unique_ptr<IBuffer> createBuffer() = 0;

  /** Configure instance vertex attributes on the currently bound mesh.
   *  The instanceBuffer holds per-instance model matrices + color data.
   *  This is an OpenGL-specific concept; Vulkan implementations should be
   *  no-ops since instance attributes are configured at pipeline creation.
   */
  virtual void setupInstanceAttributes(IBuffer& instanceBuffer) = 0;

  /** Unbind instance vertex attributes. No-op for Vulkan. */
  virtual void unbindInstanceAttributes() = 0;

  /** Issue a draw call: indexed instanced rendering. */
  virtual void drawIndexedInstanced(size_t indexCount,
                                    uint32_t instanceCount) = 0;
};
