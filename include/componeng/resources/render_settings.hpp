#pragma once

namespace componeng::resources {

/**
 * @brief Renderer switches the RenderSystem reads every frame.
 */
struct RenderSettings {
  /**
   * Draw opaque geometry twice: first depth only, with a trivial fragment
   * shader, then shaded with the depth test set to "equal" so only the
   * nearest surface of each pixel runs the real fragment shader. Pays an extra
   * vertex pass to save fragment work, so it wins when overdraw or fragment
   * cost is high. Vertex shaders must declare `invariant gl_Position;` so both
   * passes compute bit-identical depth. Alpha-tested materials
   * (MaterialType::Cutout) take part too, with an alpha-testing depth shader.
   */
  bool depthPrepass = true;
};

} // namespace componeng::resources
