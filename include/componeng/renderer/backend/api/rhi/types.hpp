#pragma once

#include <cstdint>
#include <type_traits>

namespace componeng::renderer::rhi {

/** A size in pixels. */
struct Extent2D {
  uint32_t width = 0;
  uint32_t height = 0;

  bool operator==(const Extent2D &o) const {
    return width == o.width && height == o.height;
  }
  bool operator!=(const Extent2D &o) const {
    return !(*this == o);
  }
};

struct Rect {
  int32_t x = 0;
  int32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

/** Which graphics API a device drives; decides which SPIR-V a shader needs. */
enum class Backend : uint8_t { Vulkan, OpenGL };

/** GPU formats only. Three-channel 8-bit data is expanded to RGBA8 on the CPU
 *  before upload. RGB32F is for vertex attributes only. */
enum class Format : uint8_t {
  Undefined,
  R8,
  RG8,
  RGBA8,
  RGBA8Srgb,
  BGRA8, // a swapchain's format; see IDevice::backbufferFormat
  R16F,
  RG16F,
  RGBA16F,
  R32F,
  RG32F,
  RGB32F,
  RGBA32F,
  D32F,
};

enum class BufferUsage : uint32_t {
  None = 0,
  Vertex = 1,
  Index = 2,
  Uniform = 4,
  Storage = 8,
  Indirect = 16,
  TransferSrc = 32,
  TransferDst = 64,
};

enum class ImageUsage : uint32_t {
  None = 0,
  Sampled = 1,
  ColorAttachment = 2,
  DepthAttachment = 4,
  Storage = 8,
  TransferSrc = 16,
  TransferDst = 32,
};

enum class StageFlags : uint8_t {
  None = 0,
  Vertex = 1,
  Fragment = 2,
  Compute = 4,
  AllGraphics = 3,
};

#define COMPONENG_RHI_FLAG_OPS(E)                                              \
  constexpr E operator|(E a, E b) {                                            \
    using U = std::underlying_type_t<E>;                                       \
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));              \
  }                                                                            \
  constexpr E operator&(E a, E b) {                                            \
    using U = std::underlying_type_t<E>;                                       \
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));              \
  }                                                                            \
  inline E &operator|=(E &a, E b) {                                            \
    return a = a | b;                                                          \
  }                                                                            \
  constexpr bool hasFlag(E value, E flag) {                                    \
    return (value & flag) == flag;                                             \
  }

COMPONENG_RHI_FLAG_OPS(BufferUsage)
COMPONENG_RHI_FLAG_OPS(ImageUsage)
COMPONENG_RHI_FLAG_OPS(StageFlags)

#undef COMPONENG_RHI_FLAG_OPS

/** Upload: the CPU writes it (IBuffer::write), the GPU reads it.
 *  GpuOnly: filled with IDevice::uploadBuffer, never written by the CPU.
 *  Readback: the GPU writes it, the CPU reads it. */
enum class MemoryKind : uint8_t { GpuOnly, Upload, Readback };

enum class Filter : uint8_t { Nearest, Linear };
enum class WrapMode : uint8_t { ClampToEdge, Repeat, MirroredRepeat };

enum class ShaderStage : uint8_t { Vertex, Fragment, Compute };

enum class IndexType : uint8_t { U16, U32 };
enum class VertexInputRate : uint8_t { PerVertex, PerInstance };

/** Which triangle faces are discarded (front faces are counter-clockwise). */
enum class CullMode : uint8_t { None, Back, Front };

/** Depth comparison used by the depth test. */
enum class DepthFunc : uint8_t { Less, LessEqual, Equal };

/** How a fragment's colour is combined with what is already in the target. */
enum class BlendMode : uint8_t {
  None,     // overwrite
  Alpha,    // src * a + dst * (1 - a)
  Additive, // src * a + dst
};

/**
 * @brief Fixed-function state baked into a graphics pipeline.
 *
 * The defaults are the engine's baseline (back-face culling, depth test and
 * write with Less, alpha blending). Viewport and scissor are not here: they
 * are set while recording.
 */
struct RenderState {
  CullMode cull = CullMode::Back;
  bool depthTest = true;
  bool depthWrite = true;
  DepthFunc depthFunc = DepthFunc::Less;
  BlendMode blend = BlendMode::Alpha;
  // False masks out every colour channel: the draw only touches depth.
  bool colorWrite = true;

  bool operator==(const RenderState &o) const {
    return cull == o.cull && depthTest == o.depthTest &&
           depthWrite == o.depthWrite && depthFunc == o.depthFunc &&
           blend == o.blend && colorWrite == o.colorWrite;
  }
  bool operator!=(const RenderState &o) const {
    return !(*this == o);
  }
};

} // namespace componeng::renderer::rhi
