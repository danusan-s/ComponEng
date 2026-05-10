# ComponEng Design Document

## Overview

ComponEng is an archetype-based Entity-Component-System (ECS) game engine built in C++17. The architecture is designed around three core pillars:

1. **ECS Core** - Data-oriented entity management with cache-efficient archetype storage
2. **Rendering** - OpenGL 3.3 instanced rendering with dynamic batching
3. **Physics** - Fixed-timestep simulation with impulse-based collision resolution

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Engine                                  │
│                    (Singleton Entry Point)                      │
├─────────────────────────────────────────────────────────────────┤
│  m_world: World          │  m_window: Window                    │
│  m_render_device:        │  m_renderDevice: IRenderDevice*      │
│    IRenderDevice*        │                                      │
└─────────────────────────────────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────────────────────────────────┐
│                         World                                   │
│                 (Central ECS Coordinator)                       │
├─────────────────────────────────────────────────────────────────┤
│  ComponentRegistry    │  EntityManager                          │
│  ArchetypeManager     │  SystemManager                          │
│  ThreadPool           │  EventBus                               │
│  ResourceManager      │                                         │
└─────────────────────────────────────────────────────────────────┘
```

## Core Subsystems

### 1. Engine (`core/engine.hpp`)

The `Engine` class is a singleton that bootstraps the entire application:

- **Initialization**: Sets up GLFW window, OpenGL context via GLRenderDevice, initializes World
- **Asset Loading**: Loads default shaders ("diffuse"), textures ("white"), meshes ("cube", "sphere")
- **Component Registration**: Registers all built-in components (Transform, Mesh, Material, Camera, RigidBody, Collider)
- **System Registration**: Registers built-in systems (InputSystem, CameraSystem, PhysicsSystem, RenderSystem)
- **Main Loop**: Runs the frame loop with delta time tracking, system updates, and buffer swapping

**Key Methods**:
```cpp
void init();           // Load resources, init world and window
void run(IGame&);      // Run game init, main loop, shutdown
void shutdown();       // Cleanup
```

### 2. World (`ecs/world.hpp`)

The `World` class is the central coordinator that provides the public API for all ECS operations:

**Owned Subsystems**:
| Manager | Purpose |
|---------|---------|
| `ComponentRegistry` | Maps C++ types to compact ComponentIDs |
| `EntityManager` | Entity creation/destruction, ID recycling |
| `ArchetypeManager` | Creates/stores archetypes by signature |
| `SystemManager` | System registration and execution ordering |
| `ThreadPool` | Parallel task execution for systems |
| `EventBus` | Systems can emit and retreive events |
| `ResourceManager` | Singleton resources (InputState, MainCamera) |

**Key Template Methods**:
```cpp
EntityID createEntity();                    // Create new entity
void destroyEntity(EntityID);                // Destroy entity
void addComponent(EntityID, T);              // Add component to entity
void removeComponent<T>(EntityID);           // Remove component
bool hasComponent<T>(EntityID);              // Check component presence
T& getComponent<T>(EntityID);               // Get component reference
void registerComponent<T>();                 // Register component type
void registerSystem<T>(group);               // Register system
Query<Ts...> query();                        // Create typed query
void emit_event<T>(event);                   // Emit typed event
T& get_resource<T>();                        // Get singleton resource
```

### 3. Entity Management (`ecs/entity.hpp`)

**Type Definitions**:
```cpp
using EntityID    = std::uint32_t;   // Unique entity identifier
using ComponentID = std::uint8_t;    // Component type identifier
using ArchetypeID = std::uint8_t;    // Archetype identifier
using Signature   = std::bitset<MAX_COMPONENTS>;  // Component presence mask
```

**Constraints**:
- `MAX_ENTITIES = 10000` - Maximum concurrent entities
- `MAX_COMPONENTS = 32` - Maximum distinct component types
- `MAX_ARCHETYPES = 64` - Maximum unique archetype combinations
- Entity ID 0 is reserved as `INVALID_ENTITY`

### 4. EntityManager (`ecs/entity_manager.hpp`)

Manages entity lifecycle with a free-list for ID recycling:

```
┌────────────────────────────────────────┐
│           EntityManager                │
├────────────────────────────────────────┤
│  m_freeIDs: queue<EntityID>            │
│  m_entityRecords: array<EntityRecord>  │
│  m_livingEntityCount: uint32           │
└────────────────────────────────────────┘
```

**EntityRecord** (per-entity metadata):
```cpp
struct EntityRecord {
    std::size_t row;      // Row index in current archetype
    Signature signature; // Bitmask of present components
};
```

### 5. ComponentRegistry (`ecs/component_registry.hpp`)

Maps C++ types to runtime component IDs:

```
┌─────────────────────────────────────────┐
│         ComponentRegistry               │
├─────────────────────────────────────────┤
│  m_typeToID: unordered_map<type_index,  │
│              ComponentID>               │
│  m_componentInfos: array<ComponentInfo> │
│  m_nextComponentID: ComponentID         │
└─────────────────────────────────────────┘
```

**ComponentInfo** (runtime metadata per type):
```cpp
struct ComponentInfo {
    const char* name;        // Type name
    size_t size;            // sizeof(T)
    size_t alignment;       // alignof(T)
    vector<Field> fields;   // For serialization
};
```

### 6. Archetype System

The archetype system provides cache-efficient component storage:

#### ArchetypeManager (`ecs/archetype_manager.hpp`)

```cpp
class ArchetypeManager {
    array<Archetype, MAX_ARCHETYPES> m_archetypes;
    unordered_map<Signature, ArchetypeID> m_signatureToArchetypeID;
};
```

#### Archetype (`ecs/archetype.hpp`)

Entities with identical component sets are grouped into archetypes:

```
┌─────────────────────────────────────────────────┐
│                   Archetype                     │
├─────────────────────────────────────────────────┤
│  m_columns: vector<ComponentColumn>             │
│  m_entityToRow: unordered_map<EntityID, size_t> │
│  m_rowToEntity: unordered_map<size_t, EntityID> │
│  m_signature: Signature                         │
└─────────────────────────────────────────────────┘
```

**ComponentColumn** - Contiguous storage for one component type:
- Uses aligned allocation for proper type alignment
- Stores components as raw bytes with calculated stride
- Uses swap-remove for O(1) removal

**Entity Movement Flow**:
```
Entity with signature {Transform, Mesh} 
         │
         ▼ addComponent(RigidBody)
         │
         ▼ creates/moves to new archetype
         │
    New archetype {Transform, Mesh, RigidBody}
```

### 7. Query System (`ecs/query.hpp`)

Type-safe iteration over entities matching component requirements:

```cpp
// Query all entities with Transform AND Mesh components
auto query = world.query<TransformComponent, MeshComponent>();

// Iterate with callback
query.each([](TransformComponent& t, MeshComponent& m) {
    // Process entity
});

// Iterate with EntityID
query.eachWithEntity([](EntityID e, TransformComponent& t, MeshComponent& m) {
    // Process entity with ID
});

// Parallel execution using ThreadPool
query.eachParallel(threadPool, [](TransformComponent& t, MeshComponent& m) {
    // Process in parallel
});

// Exclude entities with certain components
query.exclude<ColliderComponent>();
```

### 8. System Architecture (`ecs/system.hpp`, `ecs/system_manager.hpp`)

#### System Interface
```cpp
class ISystem {
public:
    virtual void onUpdate(const SystemState& state) = 0;
    virtual void onCreate(const SystemState& state);  // Optional
    virtual void onDestroy(const SystemState& state); // Optional
};

struct SystemState {
    World* world;       // Pointer to world for queries
    float deltaTime;   // Frame delta time
};
```

#### System Groups (Execution Order)
```cpp
enum SystemGroup {
    Initialization, // Runs first, e.g., InputSystem
    Simulation,    // Physics, logic, e.g., CameraSystem, PhysicsSystem
    Presentation   // Rendering, e.g., RenderSystem
};
```

#### SystemManager
- Prevents duplicate system registration
- Creates all systems with world reference
- Updates all systems each frame in group order
- Destroys all systems on shutdown

### 9. Thread Pool (`ecs/thread_pool.hpp`)

Fixed-size worker thread pool for parallel task execution:

```cpp
class ThreadPool {
    vector<thread> m_workers;
    queue<function<void()>> m_tasks;
};
```

Used by Query::eachParallel() for distributing entity processing across threads.

### 10. Events (`events/event_bus.hpp`)

Double-buffered event system:

```cpp
class EventBus {
    unordered_map<type_index, unique_ptr<IEventQueue>> m_queues;
};
```

**EventQueue** (per-event-type):
```cpp
template<typename EventType> class EventQueue {
    vector<EventType> m_read;   // Read buffer
    vector<EventType> m_write;  // Write buffer
};
```

Usage:
```cpp
world.emit_event<CollisionEvent>(event);
const auto& events = world.get_events<CollisionEvent>();
```

---

## Components

| Component | Header | Fields |
|-----------|--------|--------|
| `TransformComponent` | `components/transform_component.hpp` | `position`, `rotation`, `scale` (Vec3 each) |
| `MeshComponent` | `components/mesh_component.hpp` | `meshID`, `visible` |
| `MaterialComponent` | `components/material_component.hpp` | `color`, `textureID`, `shaderID` |
| `CameraComponent` | `components/camera_component.hpp` | `fov`, `aspectRatio`, `nearPlane`, `farPlane`, `viewProjectionMatrix` |
| `RigidBodyComponent` | `components/rigidbody_component.hpp` | `type` (Static/Dynamic/Kinematic), `velocity`, `mass`, `restitution` |
| `ColliderComponent` | `components/collider_component.hpp` | `type` (Box/Sphere), `transform` |

---

## Systems

### InputSystem (`systems/input_system.hpp`)
- **Group**: Initialization
- **Purpose**: Polls raw GLFW input, updates InputComponent and MouseInputComponent
- **Runs**: Before simulation systems

### CameraSystem (`systems/camera_system.hpp`)
- **Group**: Simulation
- **Purpose**: WASD movement, mouse look, computes view/projection matrices

### PhysicsSystem (`physics/physics_system.hpp`)
- **Group**: Simulation
- **Purpose**: Fixed 60Hz timestep, gravity integration, collision detection/response
- **Features**:
  - Accumulator pattern for fixed timestep
  - Impulse-based collision resolution
  - Parallel gravity integration via ThreadPool

### RenderSystem (`renderer/render_system.hpp`)
- **Group**: Presentation
- **Purpose**: Renders all visible entities with batching
- **Features**:
  - Frustum culling
  - Dynamic batching by (mesh, texture, shader)
  - GPU instancing via glDrawElementsInstanced

---

## Rendering Pipeline

### Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                      RenderSystem                           │
├─────────────────────────────────────────────────────────────┤
│  m_batches: unique_ptr<BatchMap>                           │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                       BatchMap                              │
│  map<DrawKey, BatchData>                                    │
├─────────────────────────────────────────────────────────────┤
│  DrawKey = { meshID, textureID, shaderID }                  │
│  BatchData = { instanceBuffer, instanceDatas[] }            │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   IRenderDevice                             │
│              (Abstract Rendering API)                       │
├─────────────────────────────────────────────────────────────┤
│  createShader(), createTexture(), createMesh(),            │
│  createBuffer(), setupInstanceAttributes(),                │
│  drawIndexedInstanced()                                    │
└─────────────────────────────────────────────────────────────┘
                            │
                    ┌───────┴───────┐
                    ▼               ▼
┌────────────────────────┐  ┌────────────────────────┐
│    GLRenderDevice      │  │  (Future: Vulkan impl)  │
│    (OpenGL 3.3)        │  │                        │
└────────────────────────┘  └────────────────────────┘
```

### Rendering Flow
1. **Query**: Query all entities with TransformComponent, MeshComponent, MaterialComponent
2. **Culling**: Test each entity's AABB against view frustum planes
3. **Batching**: Group visible entities by (meshID, textureID, shaderID)
4. **Instance Data**: For each batch, upload model matrices and colors to instance buffer
5. **Draw**: Use glDrawElementsInstanced for single-draw-call rendering

### BatchMap (`renderer/batch_map.hpp`)

```cpp
struct DrawKey {
    MeshID meshID;
    TextureID textureID;
    ShaderID shaderID;
};

struct InstanceData {
    Mat4 modelMatrix;  // Per-instance model transform
    Vec3 color;        // Per-instance color
};

class BatchMap {
    unordered_map<DrawKey, BatchData, DrawKeyHash> map_;
};
```

### Instance Buffer Layout
Instance data is uploaded as 4 vec4 attributes (model matrix) + 1 vec3 (color):
- stride = 80 bytes
- vertexAttribDivisor = 1 (per-instance)

---

## Physics System

### Collision Detection (`physics/collision_detection.hpp`)

**Supported Tests**:
| Test | Function |
|------|----------|
| AABB vs AABB | `testBoxBox()` |
| Sphere vs Sphere | `testSphereSphere()` |
| AABB vs Sphere | `testBoxSphere()` |
| Sphere vs AABB | `testSphereBox()` (reversed normal) |

**CollisionInfo**:
```cpp
struct CollisionInfo {
    Vec3 normal;    // Direction from A to B
    float penetration;  // Overlap depth
};
```

### Physics Flow
```
1. Query all entities with TransformComponent, RigidBodyComponent
2. Apply gravity (parallel via ThreadPool)
3. Integrate velocity: position += velocity * dt
4. Broad-phase: Simple O(n²) (future: spatial partitioning)
5. Narrow-phase: Dispatch to appropriate collision test
6. Response: Impulse-based with position correction
```

### Fixed Timestep
```cpp
double accumulator = 0.0;
while (accumulator >= FIXED_DT) {
    physicsSystem.update(FIXED_DT);
    accumulator -= FIXED_DT;
}
```

---

## Resources

### ResourceManager (`resources/resource_manager.hpp`)

Singleton/shared resources accessible by systems:

```cpp
world.set_resource(InputState{...});
world.set_resource(MainCamera{...});

InputState& input = world.get_resource<InputState>();
MainCamera& cam = world.get_resource<MainCamera>();
```

### AssetManager (`renderer/asset_manager.hpp`)

Static caching for loaded assets:

```
┌─────────────────────────────────────────────┐
│              AssetManager                   │
├─────────────────────────────────────────────┤
│  s_shaders: map<string, ShaderID>          │
│  s_textures: map<string, TextureID>       │
│  s_meshes: map<string, MeshID>            │
│  s_shaderResources: map<ShaderID, unique_ptr<Shader>>   │
│  s_textureResources: map<TextureID, unique_ptr<Texture2D>>│
│  s_meshResources: map<MeshID, unique_ptr<Mesh>>         │
└─────────────────────────────────────────────┘
```

Loads assets on first request, caches by string name, returns const references.

---

## Render Device Abstraction

### IRenderDevice (`renderer/api/irender_device.hpp`)

Abstract interface for GPU operations:

```cpp
class IRenderDevice {
    virtual void init(void* windowHandle) = 0;
    virtual void setViewport(int x, int y, int w, int h) = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void present(void* windowHandle) = 0;

    virtual unique_ptr<IShader> createShader() = 0;
    virtual unique_ptr<ITexture> createTexture() = 0;
    virtual unique_ptr<IMesh> createMesh() = 0;
    virtual unique_ptr<IBuffer> createBuffer() = 0;

    virtual void setupInstanceAttributes(IBuffer&) = 0;
    virtual void drawIndexedInstanced(size_t, uint32_t) = 0;
};
```

### Sub-interfaces

| Interface | Purpose |
|-----------|---------|
| `IShader` | Compiled shader program, uniform setters |
| `ITexture` | 2D texture, bind/generate |
| `IMesh` | Vertex/index data upload, bind for rendering |
| `IBuffer` | GPU buffer (vertex/index/uniform/instance) |

### GLRenderDevice (`renderer/opengl/gl_render_device.hpp`)

OpenGL 3.3 Core Profile implementation. Creates OpenGL-backed resources, manages global GL state.

---

## Data Flow Diagram

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Game Init  │────▶│  Register   │────▶│   Create    │
│  (IGame)    │     │Components   │     │   Systems   │
└─────────────┘     └─────────────┘     └─────────────┘
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    │                         │                         │
                    ▼                         ▼                         ▼
             ┌──────────────┐           ┌────────────┐           ┌────────────┐
             │Initialization│           │Simulation  │           │Presentation│
             │   Systems    │           │   Systems  │           │   Systems  │
             └──────────────┘           └────────────┘           └────────────┘
                    │                         │                         │
                InputSystem              CameraSystem              RenderSystem
                                        PhysicsSystem
                    │                         │                         │
                    ▼                         ▼                         ▼
              ┌────────────┐           ┌────────────┐           ┌────────────┐
              │InputState  │           │  Collision │           │   Render   │
              │Resource    │           │   Events   │           │   Output   │
              └────────────┘           └────────────┘           └────────────┘
```

---

## Class Relationship Summary

| Class | Owned By | Dependencies |
|-------|----------|--------------|
| `Engine` | - | `World`, `Window`, `IRenderDevice` |
| `World` | `Engine` | All ECS managers |
| `ComponentRegistry` | `World` | `Entity` types |
| `EntityManager` | `World` | `EntityRecord` |
| `ArchetypeManager` | `World` | `Archetype` |
| `Archetype` | `ArchetypeManager` | `ComponentColumn`, `AlignedBuffer` |
| `SystemManager` | `World` | `ISystem` |
| `ThreadPool` | `World` | std::thread |
| `EventBus` | `World` | `EventQueue` |
| `ResourceManager` | `World` | `IResource` |
| `Window` | `Engine` | `GLFW`, `IRenderDevice` |
| `GLRenderDevice` | `Engine` | OpenGL 3.3 |
| `RenderSystem` | `World` | `BatchMap` |
| `PhysicsSystem` | `World` | `CollisionDetection` |

---

## File Structure

```
include/componeng/
├── components/
│   ├── camera_component.hpp
│   ├── collider_component.hpp
│   ├── material_component.hpp
│   ├── mesh_component.hpp
│   ├── rigidbody_component.hpp
│   └── transform_component.hpp
├── core/
│   ├── engine.hpp
│   ├── game.hpp
│   ├── scene.hpp
│   ├── types.hpp
│   ├── window.hpp
│   ├── debug_ui.hpp
│   ├── raw_input_state.hpp
│   └── (utils/)
├── ecs/
│   ├── archetype.hpp
│   ├── archetype_manager.hpp
│   ├── component_registry.hpp
│   ├── entity.hpp
│   ├── entity_manager.hpp
│   ├── query.hpp
│   ├── system.hpp
│   ├── system_manager.hpp
│   ├── thread_pool.hpp
│   └── world.hpp
├── events/
│   ├── collision_event.hpp
│   ├── entity_event.hpp
│   └── event_bus.hpp
├── physics/
│   ├── collision_detection.hpp
│   └── physics_system.hpp
├── renderer/
│   ├── api/irender_device.hpp
│   ├── asset_manager.hpp
│   ├── batch_map.hpp
│   ├── mesh.hpp
│   ├── render_system.hpp
│   ├── shader.hpp
│   ├── texture.hpp
│   └── opengl/
│       ├── gl_buffer.hpp
│       ├── gl_mesh.hpp
│       ├── gl_render_device.hpp
│       ├── gl_shader.hpp
│       └── gl_texture.hpp
├── resources/
│   ├── action_state.hpp
│   ├── input_state.hpp
│   ├── main_camera.hpp
│   └── resource_manager.hpp
└── systems/
    ├── camera_system.hpp
    └── input_system.hpp
```

---

## Future Architecture Considerations

From README.md future plans:
- Custom math library (replace GLM)
- Vulkan renderer backend alongside OpenGL
- Spatial partitioning (BVH, octree) for physics broad-phase
- Serialization system
- Animation system (skeletal)
- Scripting integration (Lua)
- Multi-threaded system execution
- PBR material system
