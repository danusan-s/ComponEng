# ComponEng

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586A4?logo=opengl&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.10%2B-064F8C?logo=cmake&logoColor=white)
![GLFW](https://img.shields.io/badge/GLFW-3.3-20232A)
![Status](https://img.shields.io/badge/Project-Learning%20Project-6C8E5E)

An ECS-based game engine built in C++ using modern OpenGL. With an ImGui editor further down the line.

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
- **Diffuse Lighting** - Ambient + lambertian diffuse shading
- **Debug UI** - ImGui overlay showing camera inspector and real-time FPS

## Architecture Highlights

### Entity-Component-System (ECS)

The ECS is built from scratch with no external dependencies. Key design decisions:

- **Archetype Storage**: Entities are grouped into archetypes based on their component signature. Each archetype stores components in contiguous, column-oriented arrays, enabling cache-friendly iteration during system updates.
- **Bitset Signatures**: Component presence is tracked via `std::bitset<32>`, enabling O(1) signature comparison and fast archetype lookups.
- **Type-Safe Queries**: The query system uses variadic templates and `std::index_sequence` for compile-time type resolution. Systems declare exactly which components they need, and the query engine iterates only matching archetypes.
- **Entity Movement**: Adding or removing components moves entities between archetypes via `memcpy` of raw component bytes, with swap-remove to maintain dense storage.
- **Singleton Support**: Special singleton components (e.g., `MainCameraSingleton`) are stored separately for global access patterns.

### Rendering Pipeline

- **Batch Grouping**: Draw calls are grouped by (mesh, texture, shader) tuple with a custom hash function. Each batch creates a single instance VBO and uses `glDrawElementsInstanced`.
- **Frustum Culling**: The view-projection matrix is decomposed into 6 frustum planes. Each entity's AABB is tested against all planes before being added to a batch.
- **Matrix Upload**: Instance model matrices are uploaded as 4 `vec4` attributes with `glVertexAttribDivisor(1)` for per-instance data.

### Physics

- **Fixed Timestep**: Physics runs at a fixed 60Hz using an accumulator pattern, decoupling simulation from render framerate.
- **Impulse Resolution**: Collisions are resolved using impulse-based response with mass-weighted velocity changes and position correction (Baumgarte stabilization).
- **Collision Matrix**: Supports AABB-AABB, Sphere-Sphere, and AABB-Sphere pairs with proper normal flipping for bidirectional tests.

## Tech Stack

- **C++17** - Variadic templates, `std::variant`, `std::unique_ptr`, fold expressions, structured bindings
- **OpenGL 3.3 Core Profile** - Instanced rendering, VAOs/VBOs, shader compilation
- **GLFW 3.3** - Window management, input handling
- **GLAD** - OpenGL function loading
- **GLM** - Vector/matrix math
- **Dear ImGui** - Debug overlay UI
- **stb_image** - Texture loading
- **CMake 3.10+** - Build system

## C++ Techniques Demonstrated

| Technique | Where Used |
|-----------|-----------|
| **Variadic Templates** | `World::registerComponents<Ts...>()`, `Query<Req...>`, `World::addComponents<Ts...>()` |
| **Fold Expressions** | Component registration `(this->registerComponent<Components>(), ...)` |
| **`std::index_sequence`** | Query iteration with compile-time tuple unpacking |
| **`std::variant`** | `ColliderComponent` shape storage (AABB, Sphere, OBB, Capsule, Plane) |
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
- `GLAD` included in `include/glad`
- `stb_image` included in `include/stb_image.h`

## Build Instructions

```bash
git clone <repository-url>
cd ComponEng
mkdir build && cd build
cmake ..
make
```

## Running the Simulation

You can run the executable from any directory. For example, from the project root:

```bash
./build/GameEngine
```

## Running Tests

The project includes unit tests for the ECS core and collision detection, built with Google Test.

```bash
cd build
./EcsTests
```

Or via CMake's test runner:

```bash
cd build
ctest --output-on-failure
```

For testing the graphical parts, run the example scene:

```bash
cd build
./GameEngineTest
```

### Test Coverage

| Test Suite | What It Tests |
|------------|--------------|
| `EntityManagerTest` | Entity ID allocation, recycling, record management |
| `ComponentRegistryTest` | Type-to-ID mapping, signature generation, destructor storage for non-trivial types |
| `ArchetypeTest` | Component column storage, entity add/remove, swap-remove behavior, multi-component archetypes |
| `QueryTest` | Required/excluded component matching, iteration over matching archetypes, `exclude()` returning reference |
| `CollisionTest` | AABB-AABB, Sphere-Sphere, AABB-Sphere overlap detection, collision normals, edge cases |

## Controls

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
| MeshComponent | Model reference |
| MaterialComponent | Color, texture, shader |
| CameraComponent | FOV, aspect ratio, near/far planes |
| InputComponent | Keyboard state (WASD, jump, crouch) |
| MouseInputComponent | Mouse delta and button state |
| RigidBodyComponent | Type (Static/Dynamic/Kinematic), velocity, mass, restitution |
| ColliderComponent | Shape variant (AABB, Sphere, OBB, Capsule, Plane) |

### ECS Systems
| System | Description |
|--------|-------------|
| InputSystem | Reads raw input into InputComponent |
| CameraSystem | WASD movement, mouse look, view/projection matrices |
| PhysicsSystem | Velocity/acceleration integration with gravity |
| RenderSystem | Instanced rendering with batching and frustum culling |

## Project Structure

```
ComponEng/
├── include/
│   ├── components/     # Component structs (POD data)
│   ├── core/           # Engine, window, logger, utils, debug UI
│   ├── ecs/            # Entity, archetype, query, registry, systems
│   ├── physics/        # Collision detection, physics system
│   ├── renderer/       # Shader, mesh, texture, resource manager
│   └── systems/        # Input, camera system headers
├── src/
│   ├── core/           # Core implementations
│   ├── physics/        # Physics and collision implementations
│   ├── renderer/       # Rendering implementations
│   └── systems/        # System implementations
├── assets/             # Shaders, textures, models
└── CMakeLists.txt      # Build configuration
```

## Future Plans

- [ ] Replace GLM with custom math library
- [ ] Vulkan renderer backend (alongside OpenGL)
- [ ] Spatial partitioning (BVH, octree) for physics broad phase
- [ ] Serialization system for saving/loading scenes
- [ ] Animation system with skeletal animation
- [ ] Scripting language integration (Lua or custom DSL)
- [ ] Multi-threaded system execution
- [ ] Material system with PBR shading
