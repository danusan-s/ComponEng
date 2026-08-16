# ComponEng

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586A4?logo=opengl&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.10%2B-064F8C?logo=cmake&logoColor=white)
![GLFW](https://img.shields.io/badge/GLFW-3.3-20232A)
![Status](https://img.shields.io/badge/Project-Learning%20Project-6C8E5E)

An ECS-based game engine built in C++ using modern OpenGL, featuring an ImGui debug overlay.

## What This Project Does

ComponEng is a learning project demonstrating core game engine architecture through an **Entity-Component-System (ECS)** pattern. It features an archetype-based ECS (similar to Unity DOTS or Flecs or EnTT) with cache-efficient component storage, instanced rendering, and a debug overlay.

## Why?

Well the project initially started for a really silly reason. I was kinda tired of reading through documentation and I was overwhelmed from all the different features engines like unity provide. So I decided to just build my own engine because it was simpler to understand what I wrote. I was always intrigued by how the big engines work and had found that most big games use their own engines. Having your own engine comes with many advantages, no license fees, full control over the codebase, and a deeper understanding of how games work under the hood.

## Features

- **Archetype-based ECS** - Entities with identical component signatures share archetypes with column-oriented component storage for cache efficiency
- **Instanced Rendering** - GPU instancing via `glDrawElementsInstanced` for efficient batch rendering
- **Dynamic Batching** - Automatic batching by model, texture, and shader combination
- **Frustum Culling** - View-frustum culling to skip off-screen objects before rendering
- **Physics System** - Fixed-timestep physics with gravity, velocity integration, and impulse-based collision resolution
- **Collision Detection** - AABB vs AABB, Sphere vs Sphere, and AABB vs Sphere collision tests with proper contact normal and penetration depth calculation
- **Lighting** - Ambient + lambertian diffuse shading, with directional and point light components
- **Audio** - Sound playback driven by an `AudioComponent`, backed by miniaudio
- **Scene Serialization** - Save/load whole scenes to JSON, with reflection-driven component (de)serialization
- **Multi-threaded Systems** - A `ThreadPool` powers parallel query iteration (`eachParallel`) in physics and culling
- **Debug UI** - ImGui overlay showing real-time FPS and average FPS

https://github.com/user-attachments/assets/9a45ef11-346c-4247-b8d2-d867f22ae4d3

## Architecture Highlights

### Entity-Component-System (ECS)

The ECS is built from scratch with no external dependencies. Key design decisions:

- **Archetype Storage**: Entities are grouped into archetypes based on their component signature. Each archetype stores components in contiguous, column-oriented arrays, enabling cache-friendly iteration during system updates.
- **Bitset Signatures**: Component presence is tracked via `std::bitset<32>`, enabling O(1) signature comparison and fast archetype lookups.
- **Type-Safe Queries**: The query system uses variadic templates and `std::index_sequence` for compile-time type resolution. Systems declare exactly which components they need, and the query engine iterates only matching archetypes.
- **Entity Movement**: Adding or removing components moves entities between archetypes via `memcpy` of raw component bytes, with swap-remove to maintain dense storage.
- **Resource Manager**: Singleton or shared resources such as main camera entity or input state are stored in a `ResourceManager` accessible by systems during updates.
- **Interned Names**: Because entities are relocated with `memcpy`, components must be trivially copyable -- an owning member like `std::string` would double-free. `core::Name` is therefore a 4-byte handle into a process-wide string table: no length limit, integer comparison instead of `strcmp`, and it keeps `MeshComponent` at 12 bytes instead of 72 so archetype columns stay cache-dense.

### Rendering Pipeline

- **Batch Grouping**: Draw calls are grouped by (mesh, texture, shader) tuple with a custom hash function. Each batch creates a single instance VBO and uses `glDrawElementsInstanced`.
- **Frustum Culling**: The view-projection matrix is decomposed into 6 frustum planes. Each entity's AABB is tested against all planes before being added to a batch.
- **Matrix Upload**: Instance model matrices are uploaded as 4 `vec4` attributes with `glVertexAttribDivisor(1)` for per-instance data.
- **Asset Management**: Shaders, textures, meshes and audio are loaded on demand and cached in an `AssetManager`, keyed by interned `core::Name` so lookups allocate nothing.
- **Backend Abstraction**: Rendering goes through an `IRenderDevice` interface with an OpenGL implementation behind it, leaving room for a second backend.

### Physics

- **Fixed Timestep**: Physics runs at a fixed 60Hz using an accumulator pattern, decoupling simulation from render framerate.
- **Impulse Resolution**: Collisions are resolved using impulse-based response with mass-weighted velocity changes and position correction (Baumgarte stabilization).
- **Collision Matrix**: Supports AABB-AABB, Sphere-Sphere, and AABB-Sphere pairs with proper normal flipping for bidirectional tests.

## Tech Stack

- **C++17** - Variadic templates, `std::unique_ptr`, fold expressions, structured bindings
- **OpenGL 3.3 Core Profile** - Instanced rendering, VAOs/VBOs, shader compilation
- **GLFW 3.3** - Window management, input handling
- **GLAD** - OpenGL function loading
- **GLM** - Vector/matrix math
- **Dear ImGui** - Debug overlay UI
- **stb_image** - Texture loading
- **miniaudio** - Audio playback
- **nlohmann/json** - Scene serialization
- **CMake 3.10+** - Build system

## C++ Techniques Demonstrated

| Technique | Where Used |
|-----------|-----------|
| **Variadic Templates** | `World::registerComponents<Ts...>()`, `Query<Req...>`, `World::addComponents<Ts...>()` |
| **Fold Expressions** | Component registration `(this->registerComponent<Components>(), ...)` |
| **`std::index_sequence`** | Query iteration with compile-time tuple unpacking |
| **`std::unique_ptr`** | RAII management of registry, entity manager, system manager, archetype manager |
| **Placement `new`** | Component construction in pre-allocated archetype memory |
| **`std::type_index`** | Runtime type-to-ID mapping in `ComponentRegistry` |
| **`std::bitset`** | Component signature tracking and archetype matching |
| **Template Metaprogramming** | Signature generation, query type resolution, exclude patterns |
| **Move Semantics** | Component forwarding with `std::forward<T>` |
| **CRTP-like Patterns** | System registration with type name deduplication |
| **Custom Hash Functions** | `DrawKeyHash` for batch grouping in render system |

## Dependencies

I tried to keep dependencies minimal to focus on learning core engine architecture.
GLM will likely be phased out in the future when I implement my own math library and OpenGL might be replaced with Vulkan or added as a separate implementation.

Ensure these are available before building:

- `CMake` 3.10+
- `OpenGL` 3.3+
- `GLFW` 3.3
- `GLM` installed system-wide
- `GLAD`, `Dear ImGui`, `stb_image`, `miniaudio` and `nlohmann/json` are vendored under `include/third_party/` and `src/third_party/`

## Build Instructions

```bash
git clone <repository-url>
cd ComponEng
make build
```

## Running the Example

You can run the executable from any directory. For example, from the project root:

```bash
./build/example/Demo
```

## Running Tests

The project includes unit tests for the ECS core and collision detection, built with Google Test.

Individual test executables are created for each test suite:

```bash
cd build
./test_entity_manager
./test_component_registry
./test_archetype
./test_query
./test_collision
./test_serialization
./test_world
```

Or via CMake's test runner:

```bash
cd build
ctest --output-on-failure
```

### Test Coverage

| Test Suite | What It Tests |
|------------|--------------|
| `EntityManagerTest` | Entity ID allocation, recycling, record management |
| `ComponentRegistryTest` | Type-to-ID mapping, signature generation, destructor storage for non-trivial types |
| `ArchetypeTest` | Component column storage, entity add/remove, swap-remove behavior, multi-component archetypes |
| `QueryTest` | Required/excluded component matching, iteration over matching archetypes, `exclude()` returning reference |
| `CollisionTest` | AABB-AABB, Sphere-Sphere, AABB-Sphere overlap detection, collision normals, edge cases |
| `WorldTest` | Entity lifecycle, add/remove components, archetype migration, swap-remove record repointing |
| `SerializationTest` | Component round-tripping through JSON, scene save/load |

For testing the graphical parts, run the example scene:

```bash
./build/example/Demo
```

## Controls for Example

| Input | Action |
|-------|--------|
| `W` / `A` / `S` / `D` | Move camera forward/left/backward/right |
| `Space` | Move camera up |
| `Left Shift` | Move camera down |
| `Mouse` | Look around |
| `F11` | Toggle mouse cursor lock |
| `Escape` | Close window |

## Components and Systems

You can create your own game or world by defining components and systems (extends System class) and registering them with the ECS in game init.
The following are the components and systems along with the example scene that are implemented in this project. You can modify or add new components/systems as needed.

### ECS Components
| Component | Description |
|-----------|-------------|
| TransformComponent | Position, rotation (Euler), scale |
| MeshComponent | Mesh name and resolved handle |
| MaterialComponent | Material name, texture and shader handles |
| ColorComponent | Per-instance tint |
| CameraComponent | FOV, aspect ratio, near/far planes, view-projection matrix |
| RigidBodyComponent | Type (Static/Dynamic/Kinematic), velocity, mass, restitution |
| ColliderComponent | Shape type (Box, Sphere) |
| AudioComponent | Sound name and playback state |
| DirectionalLightComponent | Direction, colour, intensity |
| PointLightComponent | Position, colour, attenuation |

Input is not a component: raw keyboard/mouse state and the mapped action state
live in the `ResourceManager` as `InputState` and `ActionState`, since they are
per-world singletons rather than per-entity data.

### ECS Systems
Systems are assigned to a `SystemGroup` (`Initialization`, `Simulation`,
`Presentation`) and run in that order each frame.

| System | Group | Description |
|--------|-------|-------------|
| InputSystem | Initialization | Maps raw input into the action state |
| PhysicsSystem | Simulation | Fixed-timestep integration, collision detection and resolution |
| CameraSystem | Simulation | WASD movement, mouse look, view/projection matrices |
| AudioSystem | Simulation | Resolves and plays sounds for AudioComponents |
| CullingSystem | Presentation | Frustum-culls entities, marks visibility |
| BatchingSystem | Presentation | Groups visible entities into instanced draw batches |
| RenderSystem | Presentation | Issues the instanced draw calls |

## Project Structure

Headers under `include/componeng/` mirror sources under `src/componeng/`,
grouped by subsystem. Vendored dependencies live in their own `third_party`
trees so they can be excluded from formatting, linting and warning flags.

```
ComponEng/
├── include/
│   ├── componeng/
│   │   ├── audio/       # Audio engine and audio system
│   │   ├── camera/      # Camera component and system
│   │   ├── core/        # Engine, window, types, string interner, debug UI
│   │   ├── ecs/         # World, entity, query, registry, systems, serializer
│   │   │   └── archetype/   # Archetype storage, component columns
│   │   ├── events/      # Event bus
│   │   ├── input/       # Raw input and action state
│   │   ├── physics/     # Collision detection, physics system
│   │   ├── renderer/    # Asset manager, render/culling/batching systems
│   │   │   ├── asset/       # Mesh, shader, texture, material
│   │   │   ├── backend/     # IRenderDevice interface + OpenGL backend
│   │   │   ├── batching/    # Draw-call batching
│   │   │   ├── component/   # Render-related components
│   │   │   └── culling/     # Frustum culling
│   │   └── utils/       # Logger, path helpers
│   └── third_party/     # glad, imgui, stb, miniaudio, nlohmann/json
├── src/                 # Mirrors include/, same layout
├── tests/               # GoogleTest suites
├── example/             # Demo game built on the engine
├── assets/              # Shaders, textures, models, audio, scenes
└── CMakeLists.txt
```

## Development

The engine builds warning-clean and is checked by three CI jobs: build+test,
`clang-format`, and `clang-tidy`.

```sh
make build                    # Release by default
make BUILD_TYPE=Debug build   # adds -g and AddressSanitizer
make run_tests                # build + ctest
make lint                     # clang-format check + clang-tidy
make format                   # apply clang-format in place
```

- **Warnings as errors**: first-party sources compile with `-Wall -Wextra
  -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Werror`. These are `PRIVATE`
  to the engine target, and vendored code is compiled with `-w`, so third-party
  warnings never mask ours.
- **Static analysis**: `.clang-tidy` enables a curated check set (resource
  ownership, `override`/`nullptr` consistency, unnecessary copies) rather than a
  blanket one; the file documents what is deliberately disabled and why.
- **Editor support**: configuring generates `build/compile_commands.json`, and
  `.clangd` points at it -- so clangd sees the exact per-file flags the build
  uses. Just run `cmake -S . -B build` once.

Conventions and the reasoning behind them are in [CONTRIBUTING.md](CONTRIBUTING.md).
Deeper architecture notes are in [docs/DESIGN.md](docs/DESIGN.md), and usage
documentation in [docs/USER.md](docs/USER.md).

## Future Plans

- [ ] Replace GLM with custom math library
- [ ] Vulkan renderer backend (alongside OpenGL)
- [ ] Spatial partitioning (BVH, octree) for physics broad phase
- [x] Serialization system for saving/loading scenes
- [ ] Animation system with skeletal animation
- [ ] Scripting language integration (Lua or custom DSL)
- [x] Multi-threaded system execution (thread pool + parallel queries)
- [ ] Material system with PBR shading
- [ ] Scene editor
