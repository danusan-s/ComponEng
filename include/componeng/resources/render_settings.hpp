#pragma once

namespace componeng::resources {

enum class AntiAliasing {
  None,
  Fxaa, // post-process edge smoothing, see FxaaPass
};

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

  /**
   * Cross-fade between LodComponent levels instead of switching in one frame:
   * both meshes are drawn, each discarding the complement of the other's
   * dither pattern (no blending, no sorting), at the cost of one extra
   * instance per fading entity. Needs a shader with `instanceFade` (the
   * bundled ones have it); other materials keep the hard switch.
   */
  bool lodDither = true;

  /** How long a LOD cross-fade lasts, in seconds. 0 switches instantly. */
  float lodFadeSeconds = 0.35f;

  /**
   * Fxaa draws the scene into an off-screen target the size of the window's
   * viewport, then smooths its edges while copying it to the window. It costs
   * one extra full-screen pass, works on every backend, and also softens the
   * edges multisampling would miss (alpha-tested leaves, LOD dithering). It
   * needs the "fxaa" shader (RenderPlugin loads it); without one it is off.
   */
  AntiAliasing antiAliasing = AntiAliasing::Fxaa;
};

} // namespace componeng::resources
