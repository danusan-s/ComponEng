# Scene Editor Roadmap

## Goal

A separate `Editor` executable that opens, edits, and saves scene JSON: list the
entities in a scene, select one, inspect and edit its components, and move it —
first with numeric drag fields, then with a 3-axis translate gizmo in the
viewport.

Scenes today are built only in C++ (`example/src/main.cpp` hardcodes 200 random
bodies) and JSON round-tripping is driven by two file-scope bools. This is the
path to editing a scene without recompiling.

## Decisions

| Decision | Choice |
|---|---|
| Form factor | New `editor/` executable target; editor module inside the engine lib under `componeng/editor/`. `example/Demo` stays a pure game. |
| Moving objects | Inspector `DragFloat3` first, then a hand-rolled translate gizmo. No ImGuizmo, no new dependencies. |
| Selection | Hierarchy panel listing live entities, plus click-to-pick in the viewport via a mouse ray against colliders. |
| Persistence | Reuse `ecs::SceneSerializer::save` / `load`. |

## Status

**Phase 0 (engine bug fixes) is complete** — see the bottom of this document for
what changed. Phases 1–8 below are outstanding.

Each phase is independently runnable and testable. Phase order matters: Phase 7
(clear scene / delete entity) depends on the Phase 0 `destroyEntity` fix, and
Phase 5 (picking) depends on the Phase 2 viewport extraction.

---

## Phase 1 — DebugUI split, pause flag, editor target skeleton

### 1.1 `DebugUI` must stop owning a window

`src/componeng/core/debug_ui.cpp:36,40` calls `ImGui::Begin("Debug Info")` in
`beginFrame` and `ImGui::End()` in `endFrame`, so any editor panel opened from a
system nests inside it.

Lazy-`Begin` does **not** fix this: `CameraSystem` opens the window mid-frame,
before the panel systems run. Convert `DebugUI` into a **deferred draw buffer**
instead — `beginFrame` does the three `NewFrame` calls and clears a
`std::vector<DebugEntry>`; `add*` `snprintf` into that vector; `endFrame` emits
`Begin("Debug Info")` → entries → `End()` → `Render` → `RenderDrawData`.

The four existing callers (`engine.cpp:155,156`, `camera_system.cpp`,
`render_system.cpp`) stay untouched. `addSlider` stores the raw `float*` —
document in the header that it must outlive the frame.

Add two statics so no other engine file needs an ImGui include:

```cpp
static bool wantsMouse();     // ImGui::GetIO().WantCaptureMouse
static bool wantsKeyboard();  // ImGui::GetIO().WantCaptureKeyboard
```

### 1.2 Edit vs Play

New `include/componeng/core/simulation_state.hpp`:

```cpp
struct SimulationState {
  bool paused = false;
};
```

Guard the top of `PhysicsSystem::onUpdate`, *before* the time accumulation so no
debt builds up while paused:

```cpp
if (state.world->hasResource<core::SimulationState>() &&
    state.world->getResource<core::SimulationState>().paused) {
  return;
}
```

Chosen over skipping the whole Simulation group in `SystemManager::updateAll`
(`system_manager.hpp:178`), which would also kill `CameraSystem` — that writes
`viewProjectionMatrix`, so Culling/Batching/Render would consume a stale matrix
and the editor viewport would freeze. The resource flag is three lines, has no
effect on the Demo (resource absent → physics runs), and is a legitimate engine
feature rather than an editor hack, which is why it belongs in `core/` and keeps
`physics/` free of any editor dependency.

> Play → Stop leaves the scene mutated. Once Phase 7 lands, snapshot on Play to
> `assets/scenes/.play_snapshot.json` and `clearScene` + load on Stop (~6 lines).

### 1.3 CMake and target wiring

Root `CMakeLists.txt`:
- Add `"src/componeng/editor/*.cpp"` to the `file(GLOB ENGINE_SOURCES ...)` list
  — it enumerates directories explicitly, so a new one is invisible otherwise.
- `add_subdirectory(editor)` behind an `EXCLUDE_EDITOR` guard mirroring
  `EXCLUDE_EXAMPLE`.

`editor/CMakeLists.txt` mirrors `example/CMakeLists.txt` with target `Editor`.

`editor/src/main.cpp` — an `EditorApp : core::IGame`:

```cpp
void init(ecs::World &world) override {
  world.setResource(core::SimulationState{.paused = true});
  world.setResource(editor::EditorState{});
  world.setResource(editor::Selection{});
  editor::ensureSceneDirectory();
  core::Engine::get().m_window.setCloseOnEscape(false);

  world.registerSystem<editor::EditorCameraSystem>(ecs::SystemGroup::Simulation);
  world.registerSystem<editor::EditorToolSystem>(ecs::SystemGroup::Simulation);
  world.registerSystem<editor::EditorUISystem>(ecs::SystemGroup::Presentation);
  world.registerSystem<editor::GizmoRenderSystem>(ecs::SystemGroup::Presentation);
  world.addSystemDependencies<editor::GizmoRenderSystem, renderer::RenderSystem>();
}
```

All registration must happen inside `IGame::init`, which `Engine::run:128` calls
before `createSystems():129`. `buildExecutionOrder` (`system_manager.hpp:72`)
reorders `m_systems` without updating `m_typeToIndex` / `m_dependencies`, so
registering anything afterwards silently corrupts the dependency graph.

`include/componeng/editor/editor_state.hpp`:

```cpp
struct EditorState {
  std::string currentScenePath;
};

struct Selection {
  ecs::EntityID entity = ecs::INVALID_ENTITY;
};
```

`Selection` as a resource follows the existing `MainCamera` / `RenderQueue`
pattern and lets the UI, tool, and gizmo systems share it without coupling.

### 1.4 Lint plumbing

`.github/workflows/ci.yml:52` and the `Makefile` `lint` / `format` targets glob
`include/componeng src/componeng tests example`. Add `editor` to both, or the new
files escape the clang-format `--Werror` job entirely.

**Exit:** `build/editor/Editor` launches; "Debug Info" and a stub "Hierarchy" are
two *separate* top-level windows.

---

## Phase 2 — Cursor mode and the editor fly-camera

### 2.1 `window.cpp` / `window.hpp`

Three problems in the current design:

- `g_mouseLocked` is a file static with no accessor (`window.cpp:12`).
- While unlocked, the `if (!g_mouseLocked) return;` at line 51 sits *above* the
  `GLFW_RELEASE` handling, so keys held at unlock time stay stuck `true`.
- Escape unconditionally closes the window (line 65).

Changes:

1. **Always record raw input.** Delete the `!g_mouseLocked` early-returns from
   `keyCallback:51`, `mouseButtonCallback:78`, `cursorPosCallback:88`.
   `RawInputState` becomes an honest hardware mirror — this alone fixes the
   stuck keys.
2. **Move the flag onto `Window`** and expose `isMouseLocked()`,
   `setMouseLocked(bool)`, `setCloseOnEscape(bool)`. `setMouseLocked(true)` sets
   `GLFW_CURSOR_DISABLED` **and** snaps `previous_state.mouseX/Y =
   current_state.mouseX/Y` — reuse the anti-spike logic already at
   `window.cpp:45`. Keep F11 as a toggle calling into it.
3. **Gate Escape** on `m_closeOnEscape` (default `true` → Demo unchanged; the
   editor sets it `false`).
4. **Extract the letterbox math.** `framebufferSizeCallback:14` and
   `Window::setViewport:149` are two copies of the same hardcoded 16:9
   computation, and picking needs a third consumer. In `window.hpp`:

   ```cpp
   struct ViewportRect {
     int x = 0, y = 0, width = 0, height = 0;
   };

   /// Letterboxed sub-rect of a framebuffer at the target aspect ratio.
   /// Single source of truth for glViewport/glScissor and for screen->ray
   /// unprojection.
   ViewportRect computeLetterboxViewport(int fbWidth, int fbHeight,
                                         float targetAspect = 16.0f / 9.0f);
   ```

   Route all three call sites through it. Guard `fbHeight <= 0` — a minimised
   window currently divides by zero at lines 16 and 151.

Do **not** gate on `io.WantCaptureMouse` inside the GLFW callbacks. ImGui's
chained handler calls the engine's callback *first* and only then updates its own
state, so the flag is a frame stale there. Gate at consumption time instead.

### 2.2 `editor::EditorCameraSystem` (Simulation)

The engine's `CameraSystem` only computes the view-projection matrix; movement
lives in the game layer (`example/src/player_controller.cpp`), so the editor
needs its own. Hold-RMB-to-look:

```cpp
bool rmb = input.current_state.mouseButtons[GLFW_MOUSE_BUTTON_RIGHT];

if (rmb && !m_looking) {
  if (core::DebugUI::wantsMouse()) return;   // RMB started over a panel
  window.setMouseLocked(true);
  m_looking = true;
} else if (!rmb && m_looking) {
  window.setMouseLocked(false);
  m_looking = false;
}
if (!m_looking) return;

// yaw/pitch from getMouseDeltaX/Y, pitch clamped to ±89
// WASD / Space / Shift from ActionState
```

Because movement only runs while RMB is held, `WantCaptureKeyboard` never needs
consulting — which also sidesteps `ImGuiConfigFlags_NavEnableKeyboard`
(`debug_ui.cpp:19`) stealing WASD from a focused panel. Reuse
`player_controller.cpp`'s constants but drop the move speed from 100 to ~15.

> ImGui's GLFW backend still delivers cursor-pos events under `CURSOR_DISABLED`,
> so panels may show phantom hover while looking around. If that is visible in
> practice, toggle `io.ConfigFlags |= ImGuiConfigFlags_NoMouse` on lock. Don't do
> it preemptively.

**Exit:** cursor visible; RMB + WASD flies; releasing RMB restores the cursor
with no stuck keys. Demo unchanged (F11, Escape-to-quit, cursor disabled at
startup).

---

## Phase 3 — Entity enumeration and the Hierarchy panel

### 3.1 Add a narrow public API to `World`

`World`'s `m_componentRegistry` / `m_archetypeManager` / `m_entityManager` are
private with `SceneSerializer` as the only `friend`. Three ways out, and the
choice matters:

- **A second `friend`** — works for one privileged class, but the editor is 4+
  classes and the list would grow with every editor file, forcing `world.hpp` to
  forward-declare `componeng::editor::*`. The engine ends up naming the editor.
  Rejected.
- **Public `componentRegistry()` / `archetypeManager()` accessors** — cheapest to
  write, worst to live with. It publishes the entire internal representation,
  and *const* accessors aren't actually available: `getEntityForRow`,
  `getColumn`, and `getComponentInfo` are all non-const and mutate their maps via
  `operator[]`. You'd either const-ify that whole chain or hand out mutable
  references to archetype storage. Rejected.
- **A narrow public API** — intention-revealing, four small functions, no new
  friends, and it puts "iterate archetypes correctly" in one place instead of
  copy-pasting `SceneSerializer::save`'s loop. **Chosen.**

In `world.hpp` / `world.cpp`, 5–10 lines each:

```cpp
/// Visits every live entity that has at least one component. Entities with an
/// empty signature belong to no archetype and are not visited.
/// The callback MUST NOT create or destroy entities — archetype storage uses
/// swap-remove, so mutating during iteration skips or double-visits rows.
/// Collect IDs first, mutate after.
void forEachEntity(const std::function<void(EntityID)> &fn);

/// Registry-owned name strings for each component on the entity.
std::vector<const char *> getComponentNames(EntityID entity);

/// Raw storage pointer for one component, or nullptr if the entity lacks it.
void *getComponentById(EntityID entity, ComponentID componentID);

const ComponentInfo &getComponentInfoById(ComponentID componentID);
ComponentID getComponentCount() const;
```

`getComponentById` is what makes the Inspector's generic fallback (§4) possible
without any field reflection.

> Escape hatch if you want Phase 3 with zero engine diff:
> `world.query<core::TransformComponent>().eachWithEntity(...)` already
> enumerates entities publicly today. It can't answer "which components does
> entity N have", silently omits entities without a Transform, and rebuilds a
> `Query` per call — but it works.

### 3.2 `editor::EditorUISystem` — Hierarchy panel

Registered in Presentation. Draws a top-level `ImGui::Begin("Hierarchy")`:

```cpp
std::vector<ecs::EntityID> ids;
world.forEachEntity([&](ecs::EntityID e) { ids.push_back(e); });

ecs::EntityID camera = world.getResource<camera::MainCamera>().entity;
auto &sel = world.getResource<editor::Selection>();

for (ecs::EntityID e : ids) {
  char label[64];
  snprintf(label, sizeof(label), "Entity %u%s", e,
           e == camera ? " (Camera)" : "");
  if (ImGui::Selectable(label, sel.entity == e)) {
    sel.entity = e;
  }
}
```

Buffer any delete request into a member and apply it *after* the loop, per the
`forEachEntity` contract.

**Exit:** Hierarchy lists entities, clicking selects, selection persists across
frames.

---

## Phase 4 — Inspector (the first "objects move" milestone)

### 4.1 Hand-written per-type drawers, deliberately

There is no field reflection — the field list exists only inside the
`SERIALIZABLE_COMPONENT` macro expansion in `component_serializer.hpp`. Adding
real reflection means extending that macro to emit a `visit_fields` and teaching
a visitor about `Vec3` / `Vec4` / `Name` / enum / nested components, which is
exactly where the vec4 bug fixed in Phase 0 came from. For nine registered types
a hand-written table is less code and gives better widgets (`ColorEdit4` over
four drag boxes, `Combo` over an int box).

`include/componeng/editor/inspector.hpp`:

```cpp
/// Draws the ImGui body for one component of one entity.
using ComponentDrawer = std::function<void(ecs::World &, ecs::EntityID)>;

class Inspector {
public:
  void registerDrawer(const char *componentName, ComponentDrawer drawer);
  /// Iterates world.getComponentNames(entity), one CollapsingHeader each.
  void draw(ecs::World &world, ecs::EntityID entity);

private:
  std::unordered_map<std::string, ComponentDrawer> m_drawers;
};

/// Installs drawers for all engine-registered component types.
void registerBuiltinDrawers(Inspector &inspector);
```

**The fallback is the extension point.** For any component with no registered
drawer, look up `getComponentInfoById`, call
`info.serializer(world.getComponentById(entity, id))`, and render `json.dump(2)`
read-only via `ImGui::TextWrapped`. A brand-new component type is visible
immediately with zero editor work, and gets real widgets by adding one
`registerDrawer` line.

Guard `json.is_null()`: `DirectionalLightComponent` is registered
(`engine.cpp:87`) but has **no** `SERIALIZABLE_COMPONENT` specialization, so its
serializer is the primary template that logs an error and returns null. That also
means it currently round-trips to garbage through scene save/load — worth a
separate small commit adding the specialization.

### 4.2 Built-in drawers

| Component | Widgets |
|---|---|
| `TransformComponent` | `DragFloat3` position (0.1), rotation (1.0, degrees — correct after the Phase 0 radians fix), scale (0.05) |
| `MeshComponent` | `InputText` into `meshName.value` (`core::Name` is a raw `char[64]`, so ImGui writes into it directly). **On change set `meshID = 0`** so `BatchingSystem:63` re-resolves. `visible` read-only. |
| `MaterialComponent` | Same; on change zero `materialID` / `textureID` / `shaderID` (`BatchingSystem:53`) |
| `ColorComponent` | `ColorEdit4` |
| `RigidBodyComponent` | `Combo` type, `DragFloat3` velocity, `DragFloat` mass (min 0), `SliderFloat` restitution 0..1 |
| `ColliderComponent` | `Combo` shape, `Checkbox` trigger, `DragFloat3` offset and **half-extents** — that is how `testBoxBox` and `CullingSystem` treat `transform.scale`, so label it accordingly |
| `CameraComponent` | `DragFloat` fov / near / far; aspect ratio and matrix read-only |
| `AudioComponent` | name, `playOnAwake` / `loop` / `is3D`, volume / pitch, min / max distance |

Panel: `ImGui::Begin("Inspector")`, "No selection" when `INVALID_ENTITY`.
`Inspector` is a member of `EditorUISystem`, populated in `onCreate`.

> `AssetManager` has no "list registered names" API, so mesh and material are
> text fields rather than combos. Adding
> `std::vector<std::string> AssetManager::meshNames() const` is ~4 lines if
> combos are wanted — optional.

**Exit:** select the camera in the Hierarchy, drag its Position, the view moves.

---

## Phase 5 — Ray picking

### 5.1 Engine-level ray math

New `include/componeng/physics/raycast.hpp` + `src/componeng/physics/raycast.cpp`
— in `physics/` rather than `editor/` because it is covered by the existing glob,
reusable by gameplay, and unit-testable headlessly. Nothing in
`physics/collision_detection.hpp` is reusable; it is all shape-vs-shape overlap
with no ray functions.

```cpp
struct Ray {
  core::Vec3 origin;
  core::Vec3 direction;   // must be normalized
};

/// tHit is the near intersection (0 if the origin is inside).
bool rayIntersectsAABB(const Ray &ray, const core::Vec3 &min,
                       const core::Vec3 &max, float &tHit);
bool rayIntersectsSphere(const Ray &ray, const core::Vec3 &center,
                         float radius, float &tHit);
```

AABB is the slab method with an **explicit parallel-axis guard** rather than
relying on `±inf` — an origin exactly on a slab produces `inf * 0 = NaN` and the
comparison silently fails:

```cpp
float t0 = 0.0f, t1 = std::numeric_limits<float>::max();
for (int i = 0; i < 3; ++i) {
  if (std::abs(ray.direction[i]) < 1e-8f) {
    if (ray.origin[i] < min[i] || ray.origin[i] > max[i]) return false;
    continue;
  }
  float inv = 1.0f / ray.direction[i];
  float ta = (min[i] - ray.origin[i]) * inv;
  float tb = (max[i] - ray.origin[i]) * inv;
  if (ta > tb) std::swap(ta, tb);
  t0 = std::max(t0, ta);
  t1 = std::min(t1, tb);
  if (t0 > t1) return false;
}
tHit = t0;
return true;
```

Sphere, geometric form:

```cpp
core::Vec3 m = ray.origin - center;
float b = glm::dot(m, ray.direction);
float c = glm::dot(m, m) - radius * radius;
if (c > 0.0f && b > 0.0f) return false;   // origin outside, pointing away
float disc = b * b - c;
if (disc < 0.0f) return false;
tHit = std::max(0.0f, -b - std::sqrt(disc));
return true;
```

Add `tests/test_raycast.cpp` (hit / miss / origin-inside / parallel-axis /
behind-camera) to `TEST_SOURCES`.

### 5.2 Screen → world ray

`include/componeng/editor/picking.hpp`:

```cpp
struct PickResult {
  ecs::EntityID entity = ecs::INVALID_ENTITY;
  float distance = 0.0f;
};

/// @param cursorX,cursorY  GLFW window coords, origin top-left (InputState).
/// @param windowSize       glfwGetWindowSize result.
/// @param framebufferSize  glfwGetFramebufferSize result (differs on Retina).
/// @return false if the cursor is in a letterbox bar (no valid ray).
bool screenPointToRay(float cursorX, float cursorY,
                      const glm::ivec2 &windowSize,
                      const glm::ivec2 &framebufferSize,
                      const core::Mat4 &viewProj, physics::Ray &outRay);

PickResult pickEntity(ecs::World &world, const physics::Ray &ray);
```

Three traps, all handled by that signature:

1. **Window coords ≠ framebuffer pixels.** `glViewport` is in framebuffer pixels,
   `glfwGetCursorPos` is in window coords — 2× apart on a Retina display. Scale
   first: `px = cursorX * fb.x / win.x`.
2. **Y flip.** GLFW is top-left origin, GL is bottom-left: `glY = fb.y - py`.
3. **Letterbox offset.** Use `core::computeLetterboxViewport(fb.x, fb.y)` from
   Phase 2.1.4 — the *same* function the renderer uses, so they cannot drift.

```cpp
core::ViewportRect vp = core::computeLetterboxViewport(fb.x, fb.y);
float u = (px  - vp.x) / static_cast<float>(vp.width);
float v = (glY - vp.y) / static_cast<float>(vp.height);
if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;  // letterbox bar

core::Mat4 invVP = glm::inverse(viewProj);
core::Vec4 pNear = invVP * core::Vec4(u * 2 - 1, v * 2 - 1, -1.0f, 1.0f);
core::Vec4 pFar  = invVP * core::Vec4(u * 2 - 1, v * 2 - 1,  1.0f, 1.0f);
core::Vec3 nearW = core::Vec3(pNear) / pNear.w;
core::Vec3 farW  = core::Vec3(pFar)  / pFar.w;
outRay = {nearW, glm::normalize(farW - nearW)};
```

`farPlane` is 10000 (`engine.cpp:121`), so the far unprojection is numerically
coarse — but only its *direction* is used, which is fine.

### 5.3 `pickEntity`

```cpp
world.query<core::TransformComponent, physics::ColliderComponent>()
    .eachWithEntity([&](ecs::EntityID e, auto &t, auto &col) {
      core::Vec3 center = t.position + col.transform.position;
      core::Vec3 half   = col.transform.scale * t.scale;   // matches CullingSystem
      float tHit;
      bool hit = (col.type == physics::ColliderType::Box)
                   ? physics::rayIntersectsAABB(ray, center - half, center + half, tHit)
                   : physics::rayIntersectsSphere(ray, center, half.x, tHit);
      if (hit && (best.entity == ecs::INVALID_ENTITY || tHit < best.distance)) {
        best = {e, tHit};
      }
    });
```

`cube.obj` spans ±1, so `scale` is a half-extent — the same convention
`testBoxBox` and `CullingSystem` use. `half.x` as the sphere radius mirrors
`testSphereSphere`. Rotation is ignored, consistent with the AABB-only physics
layer; document that a rotated box picks against its unrotated AABB.

**Only collider-bearing entities are pickable** — which is why "New Entity" must
always add one (Phase 7).

### 5.4 Click-to-pick wiring

In `EditorToolSystem::onUpdate`, before the gizmo logic:

```cpp
if (core::DebugUI::wantsMouse()) return;   // click landed on a panel
if (!input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) return;
// build the ray from input.current_state.mouseX/Y
sel.entity = pickEntity(world, ray).entity;   // INVALID_ENTITY clears selection
```

Edge detection works because `Engine::run:160-161` swaps input buffers before
polling, so `previous_state` really is last frame's.

**Exit:** click a cube, it highlights in the Hierarchy and fills the Inspector.
Verify near the letterbox edges and, on Retina, that the hit lands under the
cursor rather than at half the offset.

---

## Phase 6 — Play / Pause toolbar

Trivial after Phase 1.2: `ImGui::Begin("Toolbar")` with a Play / Pause button
toggling `SimulationState::paused`. Add the snapshot round-trip described in 1.2
once Phase 7's `clearScene` / `openScene` exist.

---

## Phase 7 — Scene toolbar, create and delete entity

`include/componeng/editor/scene_commands.hpp`:

```cpp
/// Creates assets/scenes if missing. Call once at startup.
void ensureSceneDirectory();

/// *.json filenames (not full paths) in assets/scenes.
std::vector<std::string> listScenes();

/// Destroys every entity except the main camera; clears Selection.
void clearScene(ecs::World &world);

bool openScene(ecs::World &world, const std::string &path);   // clearScene + load
bool saveScene(ecs::World &world, const std::string &path);

/// Cube 10 units in front of the camera, with a Box collider (required for
/// picking) and a Static rigidbody.
ecs::EntityID createDefaultCube(ecs::World &world);

/// destroyEntity + clear Selection if it pointed at that entity.
void deleteEntity(ecs::World &world, ecs::EntityID entity);
```

`clearScene` **must be two-pass** — collect victims via `forEachEntity`, then
destroy — because archetype removal is swap-remove:

```cpp
ecs::EntityID camera = world.hasResource<camera::MainCamera>()
                         ? world.getResource<camera::MainCamera>().entity
                         : ecs::INVALID_ENTITY;
std::vector<ecs::EntityID> victims;
world.forEachEntity([&](ecs::EntityID e) {
  if (e != camera) victims.push_back(e);
});
for (ecs::EntityID e : victims) world.destroyEntity(e);
world.getResource<Selection>().entity = ecs::INVALID_ENTITY;
```

`openScene` needs the clear because `SceneSerializer::load` *adds* to the
existing world rather than replacing it.

`ensureSceneDirectory` is
`std::filesystem::create_directories(utils::Utils::getAssetPath("assets/scenes"))`.
`getAssetPath` resolves relative to the source root via `__FILE__` and uses
`weakly_canonical`, which is fine for a not-yet-existing path.

`createDefaultCube` spawns at `camPos + front * 10` — recompute `front` from
yaw/pitch exactly as `camera_system.cpp:15` does — with Transform, Mesh `"cube"`,
Material `"default_diffuse"`, white Color, **Box Collider**, and a Static
RigidBody. The collider is non-negotiable: without it the entity is unpickable
*and* `CullingSystem` never writes `MeshComponent::visible`, because its query
requires a collider (`culling_system.cpp:32`).

**Menu bar** — no native file dialog, no new dependencies:

- **New** → `clearScene`, clear `currentScenePath`
- **Open** → popup listing `listScenes()` as `Selectable`s
- **Save** → falls through to Save As when `currentScenePath` is empty
- **Save As** → `InputText` for a filename, append `.json`
- **Entity ▸ New Cube / Delete Selected** → also bind the Delete key, gated on
  `!DebugUI::wantsKeyboard()`

Surface `saveScene`'s `bool` as an ImGui error line.

`.gitignore` currently ignores only `/assets/scenes/test_scene.json`, so other
scenes *would* be tracked. Decide deliberately: widen to `/assets/scenes/` and
keep the `.gitkeep`, or leave it and let scenes be committed.

**Exit:** New → Add Cube → drag in the Inspector → Save As → New → Open — the
cube is back with its colour intact.

---

## Phase 8 — Translate gizmo

### 8.1 Geometry and constant screen size

Three boxes built from the existing `"cube"` mesh (spans ±1, so `scale` is a
half-extent):

| Axis | Handle centre | Handle half-extents | Colour |
|---|---|---|---|
| X | `pivot + (L/2, 0, 0)` | `(L/2, w, w)` | red |
| Y | `pivot + (0, L/2, 0)` | `(w, L/2, w)` | green |
| Z | `pivot + (0, 0, L/2)` | `(w, w, L/2)` | blue |

`L` must scale with distance so the gizmo subtends a fixed pixel height. GLM's
`perspective(radians(fov), ...)` takes a **vertical** FOV and the engine passes
`camera.fov` (`camera_system.cpp:49`), so:

```cpp
float distance = glm::length(pivot - cameraWorldPos);
if (distance < 1e-3f) return;                       // camera inside the object
float halfHeight   = distance * std::tan(core::radians(cam.fov) * 0.5f);
float worldPerPixel = (2.0f * halfHeight) / static_cast<float>(vp.height);

const float L = 90.0f * worldPerPixel;   // handle length, in pixels
const float w =  3.0f * worldPerPixel;   // handle thickness, in pixels
```

`vp.height` is in **framebuffer** pixels, so on Retina the gizmo renders at half
the apparent logical size. Either accept it or divide by the framebuffer/window
scale — decide and comment it, don't leave it accidental.

Model matrix per handle is `translate(centre) * scale(halfExtents)`. No rotation.

### 8.2 Rendering — a dedicated pass, not entities

Three real ECS entities would render for free (collider-less, so `CullingSystem`
skips them and `visible` stays true, and `BatchingSystem` picks them up). But
`SceneSerializer::save` walks every archetype (`scene_serializer.cpp:18`) and
would persist the gizmo handles into every saved scene, plus clutter the
Hierarchy. Working around that means destroying and recreating them around every
save. **Rejected.**

Instead add `editor::GizmoRenderSystem` (Presentation, depends on
`RenderSystem`), ~60 lines adapted from `render_system.cpp:87-118`:

```
if (selection == INVALID_ENTITY) return;
renderDevice.clearDepth();
shader.use();
shader.setMatrix4("viewProj", viewProj);
shader.setVector3f("lightDir", -cameraFront);
shader.setVector3f("lightColor", 1, 1, 1);
texture("white").bind();
mesh("cube").getImpl().bind();
// pack 3 instances against material.getVertexLayout(): mat4 rows + vec4 colour
buf = renderDevice.createBuffer();
buf->setData(...);
renderDevice.setupInstanceAttributes(*buf, layout);
renderDevice.drawIndexedInstanced(mesh.indexCount(), 3);
renderDevice.unbindInstanceAttributes(layout);
```

Requires one engine addition so the gizmo isn't occluded by geometry — add to
`IRenderDevice` and `GLRenderDevice`:

```cpp
/// Clear only the depth buffer (for overlay passes).
virtual void clearDepth() = 0;   // GL: glClear(GL_DEPTH_BUFFER_BIT);
```

Only one implementation exists (the Vulkan backend is commented out in the root
CMakeLists), so a pure virtual is safe.

All three handles go in **one** instanced draw because `instanceColor` is
per-instance (`assets/shaders/diffuse.vert:8`). `IMaterial::buildInstanceDataFloats`
can't be reused (it needs an `EntityID`), so write a ~20-line local packer that
walks `layout.attributes` and memcpys mat4 rows (`attr.matrixRow >= 0`) and the
colour — copy the shape from `material.hpp:64-117`.

`lightDir = -cameraFront` matters: `diffuse.frag:20` has a hardcoded 0.1 ambient,
so faces pointing away would render near-black. Lighting from the camera keeps
every visible face bright. Highlight the hovered/active axis in yellow.

### 8.3 Hit-testing and the drag state machine

`include/componeng/editor/gizmo.hpp`:

```cpp
enum class GizmoAxis { None, X, Y, Z };

struct GizmoState {
  GizmoAxis hovered = GizmoAxis::None;
  GizmoAxis active  = GizmoAxis::None;   // != None while dragging
  float dragStartParam = 0.0f;
  core::Vec3 dragStartPosition{0.0f};
};

struct GizmoGeometry {
  core::Vec3 pivot;
  float axisLength, axisThickness;
  core::Vec3 handleMin[3], handleMax[3];
};

GizmoGeometry computeGizmoGeometry(const core::Vec3 &pivot,
                                   const core::Vec3 &cameraPos,
                                   float fovDegrees, int viewportHeightPx);

GizmoAxis pickGizmoAxis(const GizmoGeometry &g, const physics::Ray &ray);

/// Parameter s of the point on line (origin + s*axisDir) closest to the ray.
/// Returns false when the ray is nearly parallel to the axis.
bool closestParamOnAxis(const physics::Ray &ray, const core::Vec3 &origin,
                        const core::Vec3 &axisDir, float &outParam);
```

Hit-testing reuses `physics::rayIntersectsAABB` against the three handle AABBs,
**padded** by `axisThickness` on each side so thin axes are grabbable; smallest
`t` wins.

**Drag math — closest point between two skew lines.** Axis line `A + s·U` (U
unit), mouse ray `O + t·V` (V unit). With `b = U·V`, `r = A − O`, `c = U·r`,
`f = V·r`:

```
denom = 1 − b²
if |denom| < 1e-5  →  bail (looking down the axis; the solution is unbounded)
s = (b·f − c) / denom
```

Sanity check: camera at the origin looking down −Z, axis +X through
`A = (0, 0, −10)`, mouse tilted so `V ≈ (0.0995, 0, −0.995)`. Then `b = 0.0995`,
`denom = 0.990`, `c = 0`, `f = 9.95`, `s = 1.0` — and the ray does cross
`z = −10` at `x = 1`.

**Use the delta, not the absolute.** Setting `position = A + s·U` snaps the
object origin to the cursor. Capture `dragStartParam` and `dragStartPosition` at
mouse-down and each frame set:

```cpp
transform.position =
    state.dragStartPosition + axisDir * (s - state.dragStartParam);
```

anchoring `closestParamOnAxis` at `dragStartPosition`, **not** the live pivot —
anchoring at the live pivot makes the parameter drift as the object moves and
produces runaway drag.

State machine in `EditorToolSystem::onUpdate` (Simulation, so Presentation sees
the updated transform in the same frame):

```
1. no selection, or selection lacks Transform  -> active = None; return
2. DebugUI::wantsMouse() and not dragging      -> return
3. build the mouse ray; bail if in a letterbox bar
4. geo = computeGizmoGeometry(...)
5. if active == None:
     hovered = pickGizmoAxis(geo, ray)
     if LMB pressed:
       if hovered != None:
         active = hovered
         dragStartPosition = transform.position
         closestParamOnAxis(ray, pivot, axisDir(active), dragStartParam)
       else:
         selection = pickEntity(world, ray).entity    // gizmo wins ties
   else:
     if LMB released -> active = None
     else if closestParamOnAxis(ray, dragStartPosition, axisDir(active), s):
       transform.position = dragStartPosition + axisDir(active) * (s - dragStartParam)
       // optional: snap each component to 0.25 while Ctrl is held
```

> **Riskier than it looks:** when you orbit until you're sighting straight down
> the axis you're dragging, `denom → 0` and the object either freezes or
> teleports. The `1e-5` bail is correct but reads as "the gizmo is broken" to a
> user who doesn't know why. Do what real editors do — grey out and skip
> hit-testing any axis whose `|dot(axisDir, ray.direction)| > 0.99`.

**Exit:** three handles at constant apparent size regardless of distance,
hovering highlights, dragging moves along exactly one axis and tracks the cursor
without jumping, and the Inspector's `DragFloat3` updates live.

---

## Verification

```
make build                       # Release: build/editor/Editor + build/example/Demo
make run_tests                   # ctest --output-on-failure
make lint                        # after adding editor/ to the glob (Phase 1.4)
BUILD_TYPE=Debug make rebuild    # ASan (PUBLIC on ComponEng, so it propagates)
```

Run the **ASan** build through New Scene → add 20 cubes → delete several → New
Scene. That's the exercise that catches any remaining record/row problem.

Manual end-to-end checklist:

1. `./build/editor/Editor` — "Debug Info", "Hierarchy", "Inspector", "Toolbar"
   are four *separate* top-level windows, not nested.
2. Hold RMB: cursor vanishes, mouse-look and WASD work. Release mid-stride: the
   cursor returns and the camera stops immediately (no stuck keys).
3. `Entity ▸ New Cube` ×3 — visible in the viewport and in the Hierarchy.
4. Click a cube → selects. Click empty space → deselects. Click inside a panel →
   selection unchanged (the `wantsMouse` gate).
5. Resize very wide (thick top/bottom bars), then very tall (thick side bars):
   picking still lands under the cursor, and clicking a bar does nothing.
6. Inspector: drag position / rotation / scale, edit colour, change the mesh name
   from `cube` to `sphere` — the mesh must actually change, proving the
   `meshID = 0` invalidation.
7. Gizmo: drag each axis at 5 units and at 500 units from the camera — same
   apparent handle size, same drag feel.
8. Save As "smoke" → New → Open "smoke" — positions, scales, and colours all
   restored.
9. Play → the cubes fall. Pause → they stop. In Edit mode nothing ever moves on
   its own.
10. `./build/example/Demo` — unchanged: F11 toggles the cursor, Escape quits,
    bodies bounce, the scene saves on exit.

`.clang-format` is LLVM base with `IndentWidth: 2` and
`AllowShortFunctionsOnASingleLine: None`, so even one-line accessors must be
split. Run `make format` before committing.

---

## Risk register

| Risk | Why it bites |
|---|---|
| **Window vs framebuffer coordinates** | On a Retina display picking is off by exactly 2× and it *looks* like the ray math is wrong. Three scale factors (window, framebuffer, letterbox) must all be right. |
| **Two copies of the letterbox math** | `framebufferSizeCallback` and `Window::setViewport` already duplicate it; a third copy in the editor guarantees drift. Extract before Phase 5. |
| **Gizmo near-parallel axis** | Correct math still feels broken. Grey out axes at `\|U·V\| > 0.99`. |
| **`MAX_ARCHETYPES = 64`, never recycled** | `ArchetypeManager::getOrCreate` *throws* on overflow and archetypes are never freed. A long session adding and removing components can take the app down. Not cheaply fixable — just know it exists. |
| **`memcpy` + `delete` in scene load** | Correct only while every component is trivially copyable. Commented at `ComponentInfo::deleter`. |
| **Play mutates the scene** | Play → Stop does not restore until the snapshot round-trip lands. |
| **`DirectionalLightComponent` has no serializer** | Registered but with no `SERIALIZABLE_COMPONENT`, so it round-trips to garbage today and shows as a null in the Inspector fallback. |

---

## Appendix: Phase 0, completed

Four engine bugs on the critical path, all fixed and covered by tests.

**`World::destroyEntity` corrupted a sibling entity.** `world.cpp` called
`Archetype::removeEntity` and discarded its return value. Archetype storage is
swap-remove: the entity in the last row is moved into the freed row and its
`EntityRecord::row` must be repointed. `addComponents` did this; `destroyEntity`
did not, so the swapped entity's record pointed at a row it no longer occupied.
"Delete entity" and "clear scene" are exactly this path.

The subtlety worth remembering: swap-remove copies the last row down and
decrements the count *without scrubbing the tail*, so a stale record keeps
reading its own old bytes and the bug stays invisible until something else claims
that row. `tests/test_world.cpp` therefore respawns into the vacated row before
asserting — without that it passes even against the broken code.

Also changed the three `moved != entity` checks in `world.hpp` to
`moved != INVALID_ENTITY`. The old form wrote `getRecord(0).row` whenever the
removed row was the last one; entity 0 is never allocated so it was harmless, but
it was scribbling on a reserved record.

**`glm::vec4` never deserialised.** `component_serializer.hpp` defined `is_vec4`
and serialised it, but `deserialize_field` had no matching branch — it fell into
the generic `else` and hit the not-implemented primary template. Every loaded
scene came back black. Added the branch; covered by
`SerializationTest.RoundTripColorComponent`, which fails against the old code.

**Scene load leaked one allocation per component.** `ComponentInfo::deserializer`
returns a heap `new T` and `SceneSerializer::load` never freed it. Added
`ComponentInfo::deleter` alongside it and call it after `addComponentById`.

The comment at `deleter` is the important part: `addComponentById` memcpys
`info.size` bytes and we then delete the source, which is correct *only* because
every component is trivially copyable — the reason `core::Name` exists instead of
`std::string`. A component with an owning member would turn this into a
double-free.

**Rotation was degrees in the component, radians in the renderer.**
`TransformComponent::rotation` is documented as degrees but
`DiffuseMaterial::buildInstanceData` passed it straight to `glm::rotate`.
Invisible today because nothing sets a non-zero rotation — and the first thing an
editor user does is drag the rotation field, which would have spun ~57× too fast.
Wrapped in `core::radians()`. Also deleted the dead `static getModelMatrix` in
`batching_system.cpp`, which had the same bug and was never called.

**Also:** `SceneSerializer::save` / `load` now return `bool` (and `load` catches
JSON parse errors) so the editor can surface failures; created `assets/scenes/`
with a `.gitkeep`, since it was gitignored and absent, making the Demo's save
silently fail.

Verified: all 7 test binaries pass under the Debug ASan build, and the changed
files are clang-format clean.
