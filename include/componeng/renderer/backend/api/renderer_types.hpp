#pragma once

#include "componeng/core/types.hpp"
#include "componeng/renderer/backend/api/pass_id.hpp"
#include "componeng/renderer/backend/api/rhi/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace componeng::renderer::api {

struct VertexAttribute {
  core::Name name;
  uint32_t offset = 0;
  uint32_t componentCount = 0;
  bool normalized = false;
  int matrixRow = -1;
  int location = -1;
};

struct VertexLayout {
  std::vector<VertexAttribute> attributes;
  uint32_t stride = 0;
};

constexpr uint32_t kFirstInstanceAttribLocation = 3;

enum class TextureWrap { ClampToEdge, Repeat, MirroredRepeat };
enum class TextureFilter { Nearest, Linear };

enum class TextureFormat {
  R8,
  RG8,
  RGB8,
  RGBA8,
  R16F,
  RG16F,
  RGBA16F,
  R32F,
  RG32F,
  RGBA32F
};

constexpr uint32_t textureChannels(TextureFormat format) {
  switch (format) {
    case TextureFormat::R8:
    case TextureFormat::R16F:
    case TextureFormat::R32F:
      return 1;
    case TextureFormat::RG8:
    case TextureFormat::RG16F:
    case TextureFormat::RG32F:
      return 2;
    case TextureFormat::RGB8:
      return 3;
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA16F:
    case TextureFormat::RGBA32F:
      return 4;
  }
  return 0;
}

constexpr bool textureIsFloat(TextureFormat format) {
  return format != TextureFormat::R8 && format != TextureFormat::RG8 &&
         format != TextureFormat::RGB8 && format != TextureFormat::RGBA8;
}

constexpr size_t textureCpuPixelBytes(TextureFormat format) {
  return textureChannels(format) * (textureIsFloat(format) ? 4 : 1);
}

struct TextureDesc {
  TextureWrap wrap = TextureWrap::ClampToEdge;
  TextureFilter minFilter = TextureFilter::Linear;
  TextureFilter magFilter = TextureFilter::Linear;
  bool mipmaps = true;
  float anisotropy = 1.0f;
  bool srgb = false;
};

using CullMode = rhi::CullMode;
using DepthFunc = rhi::DepthFunc;
using BlendMode = rhi::BlendMode;
using RenderState = rhi::RenderState;

} // namespace componeng::renderer::api
