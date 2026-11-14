#pragma once

#include "componeng/core/profiling.hpp"
#include "componeng/core/types.hpp"

#include <string>

namespace componeng::core {

#ifdef COMPONENG_DISABLE_DEBUG_UI
inline constexpr bool kDebugPanelsEnabled = false;
#else
inline constexpr bool kDebugPanelsEnabled = true;
#endif

namespace detail {
// The ImGui-backed implementations, always built (ImGui is always linked).
// Call them through DebugUI or ProfilerUI, which compile the call away.
void addText(const std::string &label, const std::string &value);
void addValue(const std::string &label, float v);
void addVec3(const std::string &label, const Vec3 &v);
bool addSection(const std::string &label);
bool beginPane(const std::string &label);
void endPane();
void addSlider(const std::string &label, float *v, float min, float max);
} // namespace detail

/**
 * @brief The calls that put values on screen. `Active` false makes every one
 * an empty inline, so call sites cost nothing (no string is built, no call is
 * made) and nothing is drawn.
 *
 * Everything lands in the default "Debug Info" window unless a pane is open:
 * beginPane()/endPane() wrap a group of add* calls in a window of its own, so
 * panels sit beside each other instead of stacking in one tall list. Panes are
 * ordinary movable windows; ImGui remembers where you left them.
 */
template <bool Active> struct PanelApi {
  static void addText(const std::string &label, const std::string &value) {
    if constexpr (Active)
      detail::addText(label, value);
  }
  static void addValue(const std::string &label, float v) {
    if constexpr (Active)
      detail::addValue(label, v);
  }
  static void addVec3(const std::string &label, const Vec3 &v) {
    if constexpr (Active)
      detail::addVec3(label, v);
  }
  static void addSlider(const std::string &label, float *v, float min,
                        float max) {
    if constexpr (Active)
      detail::addSlider(label, v, min, max);
  }

  /** Starts a collapsible group (open by default); the add* calls that follow
   *  until the next section belong to it. Returns false when it is collapsed,
   *  so the caller can skip building its contents. */
  static bool addSection(const std::string &label) {
    if constexpr (Active)
      return detail::addSection(label);
    return false;
  }

  /** Opens (or reopens, within the frame) the window called `label`; the add*
   *  calls until endPane() go into it. Returns false while it is collapsed,
   *  but endPane() must be called either way:
   *
   *      if (DebugUI::beginPane("Render passes")) { ...add*... }
   *      DebugUI::endPane();
   *
   *  Only valid between beginFrame() and endFrame(). */
  static bool beginPane(const std::string &label) {
    if constexpr (Active)
      return detail::beginPane(label);
    return false;
  }
  static void endPane() {
    if constexpr (Active)
      detail::endPane();
  }
};

/**
 * @brief Wrapper around ImGui for showing debug information: the frame
 * lifecycle, an FPS readout, and (PanelApi) the add* / pane calls.
 *
 * Two flags decide what a build shows:
 *  - COMPONENG_DEBUG_UI (default ON): the debug panels, i.e. everything a game
 *    or system adds through DebugUI. OFF turns those calls into empty inlines
 *    and leaves only the small FPS window.
 *  - COMPONENG_PROFILING (default ON, see Profiling): the timing panes, drawn
 *    through ProfilerUI.
 * ImGui itself is always built, for the FPS window.
 *
 * Code that calls ImGui directly (rather than through DebugUI / ProfilerUI)
 * must stay between beginFrame() and endFrame().
 */
class DebugUI : public PanelApi<kDebugPanelsEnabled> {
public:
  /** False in builds configured with -DCOMPONENG_DEBUG_UI=OFF. Code that does
   *  extra work only to feed the debug panels can skip it with `if constexpr`.
   *  The FPS window is shown either way. */
  static constexpr bool kEnabled = kDebugPanelsEnabled;

  // Setup and teardown of ImGui context and OpenGL bindings
  static void init();
  static void shutdown();

  // Convenience functions to wrap ImGui frame calls
  static void beginFrame();
  static void endFrame();

  /** Shows the frame rate: two lines in "Debug Info" when the debug panels are
   *  on, otherwise a small window of its own in the top-left corner. */
  static void showFps(float fps, float avgFps);
};

/**
 * @brief The same calls as DebugUI, for the measurement panes (frame, pass and
 * shader timings). Live exactly when Profiling::kEnabled, whatever
 * COMPONENG_DEBUG_UI says.
 */
using ProfilerUI = PanelApi<Profiling::kEnabled>;

} // namespace componeng::core
