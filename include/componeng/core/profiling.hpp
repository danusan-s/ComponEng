#pragma once

#include <chrono>

namespace componeng::core {

/**
 * @brief Whether this build measures itself: GPU timer queries (frame, render
 * pass, shader), per-pass and per-system CPU timings, and the panes that show
 * them.
 *
 * Off by default. Configure with -DCOMPONENG_PROFILING=ON to measure; with it
 * off all of that is compiled out, which is what a benchmark or a profile (perf,
 * a GPU debugger) wants, since it does not pay for its own measurement. The counters in resources::RenderStats are plain increments and
 * are always filled in.
 *
 * Independent of the debug UI (COMPONENG_DEBUG_UI): the timing panes are drawn
 * whenever profiling is on, even with the debug panels off. Code that does
 * extra work only to measure should skip it with
 * `if constexpr (Profiling::kEnabled)`.
 */
struct Profiling {
#ifdef COMPONENG_DISABLE_PROFILING
  static constexpr bool kEnabled = false;
#else
  static constexpr bool kEnabled = true;
#endif
};

using ProfilingClock = std::chrono::steady_clock;

inline ProfilingClock::time_point profilingNow() noexcept {
  return ProfilingClock::now();
}

inline double millisecondsSince(ProfilingClock::time_point start) noexcept {
  return std::chrono::duration<double, std::milli>(ProfilingClock::now() - start)
      .count();
}

} // namespace componeng::core
