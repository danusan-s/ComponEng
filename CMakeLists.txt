cmake_minimum_required(VERSION 3.10)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Find dependencies
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB ENGINE_SOURCES
    "src/*.cpp"
    "src/*.c"
    "src/core/*.cpp"
    "src/renderer/*.cpp"
    "src/renderer/opengl/*.cpp"
    "src/systems/*.cpp"
    "src/physics/*.cpp"
    "src/ecs/*.cpp"
    "src/glad/*.c"
    "src/imgui/*.cpp"
)

# Remove main.cpp if it exists in src/
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# Create STATIC library instead of executable
add_library(GameEngine STATIC ${ENGINE_SOURCES})

# Include directories (PUBLIC so game can access them)
target_include_directories(GameEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui
)

# GLM experimental
target_compile_definitions(GameEngine PUBLIC GLM_ENABLE_EXPERIMENTAL)

# Link dependencies
target_link_libraries(GameEngine
    PUBLIC
        glfw
        OpenGL::GL
)

add_executable(GameEngineTest src/main.cpp)

target_link_libraries(GameEngineTest
    PRIVATE GameEngine
)

# Debug sanitizers
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(GameEngine PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(GameEngine PRIVATE -fsanitize=address)
else()
    target_compile_options(GameEngine PRIVATE -O3)
endif()


# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(EcsTests
    tests/test_entity_manager.cpp
    tests/test_component_registry.cpp
    tests/test_archetype.cpp
    tests/test_query.cpp
    tests/test_collision.cpp
)
target_link_libraries(EcsTests PRIVATE GameEngine gtest_main)
target_compile_definitions(EcsTests PRIVATE GLM_ENABLE_EXPERIMENTAL)

enable_testing()
add_test(NAME EcsTests COMMAND EcsTests)
