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
    "src/componeng/*.cpp"
    "src/componeng/*.c"
    "src/componeng/core/*.cpp"
    "src/componeng/renderer/*.cpp"
    "src/componeng/renderer/opengl/*.cpp"
    "src/componeng/systems/*.cpp"
    "src/componeng/physics/*.cpp"
    "src/componeng/ecs/*.cpp"
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

# Test executables - one per test file
enable_testing()

set(TEST_SOURCES
    tests/test_entity_manager.cpp
    tests/test_component_registry.cpp
    tests/test_archetype.cpp
    tests/test_query.cpp
    tests/test_collision.cpp
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE GameEngine gtest_main)
    target_compile_definitions(${TEST_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
