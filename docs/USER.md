# ComponEng User Guide

This guide covers what you need to know to build a game or application using ComponEng.

## Table of Contents

- [Quick Start](#quick-start)
- [The IGame Interface](#the-igame-interface)
- [Creating Components](#creating-components)
- [Creating Systems](#creating-systems)
- [Using Queries](#using-queries)
- [Working with Resources](#working-with-resources)
- [Working with Assets](#working-with-assets)
- [Emitting and Handling Events](#emitting-and-handling-events)

---

## Quick Start

1. **Create a game class** that inherits from `IGame`
2. **Implement `init()`** to create entities and register your systems
3. **Implement `shutdown()`** for cleanup (optional)
4. **Pass your game to `Engine::run()`**

```cpp
#include "componeng/core/engine.hpp"
#include "componeng/core/game.hpp"

class MyGame : public componeng::core::IGame {
public:
    void init(componeng::ecs::World& world) override {
        // Create entities, add components, register systems
    }
    
    void shutdown(componeng::ecs::World& world) override {
        // Cleanup
    }
};

int main() {
    componeng::core::Engine::get().init();
    MyGame game;
    componeng::core::Engine::get().run(game);
    componeng::core::Engine::get().shutdown();
    return 0;
}
```

---

## The IGame Interface

Your game class is the entry point for initializing your world:

```cpp
#include "componeng/core/game.hpp"

class MyGame : public componeng::core::IGame {
public:
    // Called once after engine init, before the main loop
    void init(ecs::World& world) override {
        // Create entities
        // Add components
        // Register custom systems
    }

    // Called once after the main loop ends, before engine shutdown
    void shutdown(ecs::World& world) override {
        // Cleanup resources, save state, etc.
    }
};
```

**What to do in `init()`:**

- Create entities with `world.createEntity()`
- Add components with `world.addComponents(entity, component1, component2, ...)`
- Register your custom systems with `world.registerSystem<MySystem>(group)`
- Set up resources with `world.set_resource(MyResource{...})`

---

## Creating Components

Components are plain old data (POD) structs. They define the data associated with entities.

### Basic Component

```cpp
// file: include/my_components.hpp
#pragma once
#include "componeng/core/types.hpp"  // Provides Vec3, Mat4, etc.

namespace mygame {

struct HealthComponent {
    float current;
    float max;
    bool invulnerable = false;
};

struct VelocityComponent {
    componeng::core::Vec3 linear;
    componeng::core::Vec3 angular;
};

} // namespace mygame
```

### Registering Components

Components must be registered before use. In your game's `init()`:

```cpp
void init(ecs::World& world) override {
    // Register your custom components
    world.registerComponents<
        mygame::HealthComponent,
        mygame::VelocityComponent
    >();
    
    // ... rest of init
}
```

**Note:** Built-in components (TransformComponent, MeshComponent, etc.) are already registered by the engine.

### Adding Components to Entities

```cpp
void init(ecs::World& world) override {
    ecs::EntityID player = world.createEntity();
    
    // Add single component
    world.addComponent(player, mygame::HealthComponent{100.0f, 100.0f});
    
    // Add multiple components at once (more efficient)
    world.addComponents(player,
        componeng::components::TransformComponent{
            .position = {0.0f, 0.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        },
        mygame::HealthComponent{100.0f, 100.0f},
        mygame::VelocityComponent{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}
    );
}
```

### Accessing Components

```cpp
// Get component (throws if entity doesn't have it)
mygame::HealthComponent& health = world.getComponent<mygame::HealthComponent>(entity);

// Check if entity has component
if (world.hasComponent<mygame::HealthComponent>(entity)) {
    // ...
}

// Remove component
world.removeComponent<mygame::HealthComponent>(entity);
```

---

## Creating Systems

Systems contain the logic that operates on entities. They inherit from `ecs::ISystem`.

### Basic System

```cpp
// file: include/my_systems.hpp
#pragma once
#include "componeng/ecs/system.hpp"

class HealthSystem : public componeng::ecs::ISystem {
public:
    void onUpdate(const componeng::ecs::SystemState& state) override;
};
```

```cpp
// file: src/my_systems.cpp
#include "my_systems.hpp"
#include "my_components.hpp"
#include "componeng/ecs/world.hpp"

void HealthSystem::onUpdate(const componeng::ecs::SystemState& state) {
    // Query all entities with HealthComponent
    state.world->query<mygame::HealthComponent>().each(
        [](mygame::HealthComponent& health) {
            if (health.current <= 0.0f) {
                health.current = 0.0f;
                health.invulnerable = true;
            }
        }
    );
}
```

### System Lifecycle

Systems have three lifecycle methods:

```cpp
class MySystem : public componeng::ecs::ISystem {
public:
    // Called once when systems are created (before first frame)
    void onCreate(const componeng::ecs::SystemState& state) override {
        // Initialize system resources, cache data, etc.
    }

    // Called every frame
    void onUpdate(const componeng::ecs::SystemState& state) override {
        // Main logic here
    }

    // Called once when systems are destroyed (during shutdown)
    void onDestroy(const componeng::ecs::SystemState& state) override {
        // Cleanup
    }
};
```

### System Groups

Systems are executed in three groups in this order:

| Group | When | Common Systems |
|-------|------|----------------|
| `SystemGroup::Initialization` | First | Input processing |
| `SystemGroup::Simulation` | Second | Physics, AI, game logic |
| `SystemGroup::Presentation` | Third | Rendering |

### Registering Systems

In your game's `init()`:

```cpp
void init(ecs::World& world) override {
    // Register your system with a group
    world.registerSystem<mygame::HealthSystem>(ecs::SystemGroup::Simulation);
    world.registerSystem<mygame::DamageSystem>(ecs::SystemGroup::Simulation);
}
```

### Accessing World and DeltaTime

The `SystemState` gives you access to the world and frame timing:

```cpp
void MySystem::onUpdate(const ecs::SystemState& state) {
    // Access world for queries, resources, etc.
    ecs::World* world = state.world;
    
    // Get frame delta time in seconds
    float dt = state.deltaTime;
    
    // Use in calculations
    float velocity = 100.0f * dt;
}
```

---

## Using Queries

Queries let you iterate over entities that have a specific set of components.

### Basic Query

```cpp
// Query all entities with HealthComponent
world.query<mygame::HealthComponent>().each(
    [](mygame::HealthComponent& health) {
        // Process each entity
    }
);
```

### Query with Multiple Components

```cpp
// Query entities with BOTH TransformComponent AND HealthComponent
world.query<componeng::components::TransformComponent, mygame::HealthComponent>()
    .each([](componeng::components::TransformComponent& transform,
              mygame::HealthComponent& health) {
        // Process entities with both components
    });
```

### Query with Entity ID

If you need the entity ID (e.g., for adding/removing components):

```cpp
world.query<componeng::components::TransformComponent, mygame::HealthComponent>()
    .eachWithEntity([](ecs::EntityID entity,
                        componeng::components::TransformComponent& transform,
                        mygame::HealthComponent& health) {
        if (health.current <= 0) {
            // Can now destroy entity if needed
        }
    });
```

### Excluding Components

Exclude entities that have certain components:

```cpp
// Query all entities with TransformComponent EXCEPT those with ColliderComponent
world.query<componeng::components::TransformComponent>()
    .exclude<componeng::components::ColliderComponent>()
    .each([](componeng::components::TransformComponent& transform) {
        // ...
    });
```

### Parallel Execution

For large numbers of entities, use `eachParallel` to distribute work across threads:

```cpp
world.query<mygame::HealthComponent>()
    .eachParallel(state.world->threadPool(),
        [](mygame::HealthComponent& health) {
            // Processed in parallel
        });
```

**Note:** Avoid modifying components that other systems read simultaneously. If systems need to write to the same data, synchronize carefully or process serially.

---

## Working with Resources

Resources are singleton or shared data accessible by any system. They're stored in the World and accessed by type.

### Built-in Resources

| Resource | Purpose |
|----------|---------|
| `resources::InputState` | Keyboard/mouse input state |
| `resources::ActionState` | Mapped action states (WASD, etc.) |
| `resources::MainCamera` | Entity ID of the main camera |

### Accessing Resources

```cpp
void MySystem::onUpdate(const ecs::SystemState& state) {
    // Get input state
    componeng::resources::InputState& input = 
        state.world->get_resource<componeng::resources::InputState>();
    
    // Check keys
    if (input.isKeyDown(GLFW_KEY_W)) {
        // W is pressed
    }
    
    // Get mouse delta
    float mouseX = input.getMouseDeltaX();
    float mouseY = input.getMouseDeltaY();
}
```

### Using ActionState

```cpp
void MySystem::onUpdate(const ecs::SystemState& state) {
    auto& actions = state.world->get_resource<componeng::resources::ActionState>();
    
    // Check mapped actions
    if (actions.down(componeng::resources::Action::MoveForward)) {
        // Move forward
    }
    
    if (actions.pressed(componeng::resources::Action::Jump)) {
        // Jump just pressed this frame
    }
}
```

### Setting Resources

In your game's `init()`:

```cpp
void init(ecs::World& world) override {
    // Set the main camera entity
    ecs::EntityID camera = world.createEntity();
    world.addComponent(camera, componeng::components::TransformComponent{});
    world.addComponent(camera, componeng::components::CameraComponent{});
    
    world.set_resource(componeng::resources::MainCamera{camera});
}
```

---

## Working with Assets

The AssetManager provides static methods to load and access shaders, textures, and meshes.

### Loading Assets

Assets are loaded on first access and cached. Load them early in your game's `init()`:

```cpp
void init(ecs::World& world) override {
    using namespace componeng::renderer;
    
    // Load shaders (vertex, fragment, optional geometry)
    AssetManager::loadShader(
        "assets/shaders/my_shader.vert",
        "assets/shaders/my_shader.frag",
        nullptr,
        "my_shader"
    );
    
    // Load texture (alpha = true for RGBA, false for RGB)
    AssetManager::loadTexture("assets/textures/player.png", true, "player");
    
    // Load mesh (wavefront .obj)
    AssetManager::loadMesh("assets/models/player.obj", "player");
}
```

### Accessing Assets

```cpp
using namespace componeng::renderer;

// Get asset IDs by name
ShaderID shaderID = AssetManager::getShaderID("my_shader");
TextureID textureID = AssetManager::getTextureID("player");
MeshID meshID = AssetManager::getMeshID("player");

// Get const references to use
const Shader& shader = AssetManager.getShader(shaderID);
const Texture2D& texture = AssetManager.getTexture(textureID);
const Mesh& mesh = AssetManager.getMesh(meshID);
```

### Using Assets with Components

```cpp
void init(ecs::World& world) override {
    using namespace componeng;
    using namespace componeng::components;
    using namespace componeng::renderer;
    
    ecs::EntityID entity = world.createEntity();
    world.addComponents(entity,
        TransformComponent{
            .position = {0.0f, 0.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        },
        MeshComponent{
            .meshID = AssetManager::getMeshID("player"),
            .visible = true
        },
        MaterialComponent{
            .color = {1.0f, 1.0f, 1.0f},
            .textureID = AssetManager::getTextureID("player"),
            .shaderID = AssetManager::getShaderID("my_shader")
        }
    );
}
```

---

## Emitting and Handling Events

The event system allows loose coupling between systems.

### Defining Events

```cpp
// file: include/my_events.hpp
#pragma once
#include "componeng/ecs/entity.hpp"

namespace mygame {

struct DamageEvent {
    componeng::ecs::EntityID target;
    componeng::ecs::EntityID attacker;
    float amount;
};

struct DeathEvent {
    componeng::ecs::EntityID entity;
};

} // namespace mygame
```

### Emitting Events

```cpp
void MySystem::onUpdate(const ecs::SystemState& state) {
    // Emit an event
    state.world->emit_event<mygame::DamageEvent>({
        .target = targetEntity,
        .attacker = attackerEntity,
        .amount = 10.0f
    });
}
```

### Handling Events

Events are stored double-buffered. Read them in a system:

```cpp
void DamageSystem::onUpdate(const ecs::SystemState& state) {
    // Get all DamageEvents from this frame
    const auto& events = state.world->get_events<mygame::DamageEvent>();
    
    for (const auto& event : events) {
        // Process each event
        auto& health = state.world->getComponent<mygame::HealthComponent>(event.target);
        health.current -= event.amount;
        
        if (health.current <= 0) {
            // Emit death event
            state.world->emit_event<mygame::DeathEvent>({event.target});
        }
    }
}
```

---

## Complete Example

Here's a simplified version of how the example game works:

```cpp
// main.cpp
#include "componeng/core/engine.hpp"
#include "componeng/core/game.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/rigidbody_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/renderer/asset_manager.hpp"

#include "player_controller.hpp"  // Custom system

class Game : public componeng::core::IGame {
public:
    void init(componeng::ecs::World& world) override {
        // Create 100 cubes with physics
        for (int i = 0; i < 100; ++i) {
            componeng::ecs::EntityID entity = world.createEntity();
            
            float x = static_cast<float>(i % 10) * 2.0f;
            float z = static_cast<float>(i / 10) * 2.0f;
            
            world.addComponents(entity,
                componeng::components::TransformComponent{
                    .position = {x, 10.0f, z},
                    .rotation = {0.0f, 0.0f, 0.0f},
                    .scale = {1.0f, 1.0f, 1.0f}
                },
                componeng::components::MeshComponent{
                    .meshID = componeng::renderer::AssetManager::getMeshID("cube"),
                    .visible = true
                },
                componeng::components::MaterialComponent{
                    .color = {1.0f, 0.5f, 0.2f, 1.0f},
                    .textureID = componeng::renderer::AssetManager::getTextureID("white"),
                    .shaderID = componeng::renderer::AssetManager::getShaderID("default")
                },
                componeng::components::RigidBodyComponent{
                    .type = componeng::components::RigidBodyComponent::Dynamic,
                    .mass = 1.0f,
                    .restitution = 0.5f
                },
                componeng::components::ColliderComponent{
                    .type = componeng::components::ColliderType::Box,
                    .transform = {
                        .position = {0.0f, 0.0f, 0.0f},
                        .rotation = {0.0f, 0.0f, 0.0f},
                        .scale = {1.0f, 1.0f, 1.0f}
                    }
                }
            );
        }
        
        // Register custom system
        world.registerSystem<PlayerController>(componeng::ecs::SystemGroup::Simulation);
    }
    
    void shutdown(componeng::ecs::World& world) override {
        // Cleanup if needed
    }
};

int main() {
    componeng::core::Engine::get().init();
    Game game;
    componeng::core::Engine::get().run(game);
    componeng::core::Engine::get().shutdown();
    return 0;
}
```

---

## Common Patterns

### Adding/Removing Components at Runtime

```cpp
void PowerUpSystem::onUpdate(const ecs::SystemState& state) {
    state.world->query<mygame::PowerUpComponent>().each(
        [state](mygame::PowerUpComponent& powerUp) {
            powerUp.duration -= state.deltaTime;
            if (powerUp.duration <= 0) {
                // Remove component - entity moves to different archetype
                state.world->removeComponent<mygame::PowerUpComponent>(/* entity */);
            }
        }
    );
}
```

### Creating Entities from Templates

```cpp
ecs::EntityID createEnemy(ecs::World& world, const Vec3& position) {
    ecs::EntityID enemy = world.createEntity();
    world.addComponents(enemy,
        TransformComponent{.position = position},
        MeshComponent{.meshID = AssetManager::getMeshID("enemy")},
        HealthComponent{50.0f, 50.0f},
        AIComponent{}
    );
    return enemy;
}
```

### Frame-Rate Independent Movement

```cpp
void MovementSystem::onUpdate(const ecs::SystemState& state) {
    state.world->query<TransformComponent, VelocityComponent>().each(
        [state](TransformComponent& transform, VelocityComponent& velocity) {
            transform.position += velocity.linear * state.deltaTime;
        }
    );
}
```