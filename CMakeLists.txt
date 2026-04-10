cmake_minimum_required(VERSION 3.10)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# Renderer backend selection: "opengl" or "vulkan"
set(RENDERER_BACKEND "opengl" CACHE STRING "Renderer backend to use (opengl or vulkan)")
set_property(CACHE RENDERER_BACKEND PROPERTY STRINGS "opengl" "vulkan")

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

# Vulkan backend setup - TEMPORARILY DISABLED as GLSL to SPIR-V conversion is buggy
# and Vulkan support is not yet implemented
# if(RENDERER_BACKEND STREQUAL "vulkan")
#     find_package(Vulkan REQUIRED)
#
#     find_program(GLSLC glslc)
#     if(NOT GLSLC)
#         message(FATAL_ERROR "glslc not found. Install shaderc or Vulkan SDK.")
#     endif()
#
#     file(GLOB GLSL_SOURCES "assets/shaders/*.vert" "assets/shaders/*.frag")
#     set(SPIRV_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/spirv")
#     file(MAKE_DIRECTORY ${SPIRV_OUTPUT_DIR})
#
#     set(SPIRV_FILES "")
#     foreach(glsl ${GLSL_SOURCES})
#         get_filename_component(filename ${glsl} NAME)
#         set(spirv "${SPIRV_OUTPUT_DIR}/${filename}.spv")
#         add_custom_command(
#             OUTPUT ${spirv}
#             COMMAND ${GLSLC} -o ${spirv} ${glsl}
#             DEPENDS ${glsl}
#             COMMENT "Compiling ${filename} → ${filename}.spv"
#         )
#         list(APPEND SPIRV_FILES ${spirv})
#     endforeach()
#
#     add_custom_target(CompileShaders DEPENDS ${SPIRV_FILES})
#     add_dependencies(GameEngine CompileShaders)
#
#     target_link_libraries(GameEngine PUBLIC Vulkan::Vulkan)
#     target_compile_definitions(GameEngine PUBLIC RENDERER_BACKEND_VULKAN)
# endif()

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
