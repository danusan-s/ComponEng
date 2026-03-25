# ComponEng

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586A4?logo=opengl&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.10%2B-064F8C?logo=cmake&logoColor=white)
![GLFW](https://img.shields.io/badge/GLFW-3.3-20232A)
![Status](https://img.shields.io/badge/Project-Learning%20Project-6C8E5E)

An ECS-based game engine built in C++ using modern OpenGL.

## What This Project Does

ComponEng is a learning project demonstrating core game engine architecture through an **Entity-Component-System (ECS)** pattern. It features an archetype-based ECS (similar to Unity DOTS or Flecs) with cache-efficient component storage, instanced rendering, and a debug overlay.

## Why ?

Well the project initially started for a really silly reason. Unity was buggy on linux wayland and I couldn't get my lsp to work with unity packages. So I decided to just build my own engine because why not. I was always intrigued by how the big engines work and had found that most big games use their own engines. Having your own engine comes with many advantages, no license fees, full control over the codebase, and a deeper understanding of how games work under the hood.

## Features

- **Archetype-based ECS** - Entities with identical component signatures share archetypes with column-oriented component storage for cache efficiency
- **Instanced Rendering** - GPU instancing via `glDrawElementsInstanced` for efficient batch rendering
- **Dynamic Batching** - Automatic batching by model, texture, and shader combination
- **Diffuse Lighting** - Ambient + lambertian diffuse shading
- **Debug UI** - ImGui overlay showing camera inspector and real-time FPS


## Tech Stack

- C++17
- OpenGL 3.3 Core Profile
- GLFW 3.3
- GLAD (OpenGL loader)
- GLM (math library)
- Dear ImGui (debug UI)
- stb_image (texture loading)
- CMake 3.10+

## Dependencies

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
cd Game-Engine
mkdir build && cd build
cmake ..
make
```

## Running the Simulation

You can run the executable from any directory. For example, from the project root:

```bash
./build/GameEngine
```

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
| RigidBodyComponent | Velocity, acceleration, mass |
| BoundingBoxComponent | AABB for frustum culling |

### ECS Systems
| System | Description |
|--------|-------------|
| InputSystem | Reads raw input into InputComponent |
| CameraSystem | WASD movement, mouse look, view/projection matrices |
| PhysicsSystem | Velocity/acceleration integration with gravity |
| RenderSystem | Instanced rendering with batching and frustum culling |

