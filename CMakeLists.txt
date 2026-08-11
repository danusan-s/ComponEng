cmake_minimum_required(VERSION 3.10)
project(ComponEng)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Renderer backend selection: "opengl" or "vulkan"
set(RENDERER_BACKEND "opengl" CACHE STRING "Renderer backend to use (opengl or vulkan)")
set_property(CACHE RENDERER_BACKEND PROPERTY STRINGS "opengl" "vulkan")

# Find dependencies
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

# Split first-party from vendored so warning flags can target only our code
file(GLOB ENGINE_FIRST_PARTY_SOURCES
    "src/componeng/*.cpp"
    "src/componeng/*.c"
    "src/componeng/core/*.cpp"
    "src/componeng/renderer/*.cpp"
    "src/componeng/renderer/asset/*.cpp"
    "src/componeng/renderer/backend/opengl/*.cpp"
    "src/componeng/renderer/batching/*.cpp"
    "src/componeng/renderer/culling/*.cpp"
    "src/componeng/audio/*.cpp"
    "src/componeng/input/*.cpp"
    "src/componeng/camera/*.cpp"
    "src/componeng/physics/*.cpp"
    "src/componeng/ecs/*.cpp"
    "src/componeng/utils/*.cpp"
)
file(GLOB ENGINE_THIRD_PARTY_SOURCES
    "src/third_party/glad/*.c"
    "src/third_party/imgui/*.cpp"
)

# Remove main.cpp if it exists in src/
list(FILTER ENGINE_FIRST_PARTY_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

set(ENGINE_SOURCES ${ENGINE_FIRST_PARTY_SOURCES} ${ENGINE_THIRD_PARTY_SOURCES})

# Create STATIC library instead of executable
add_library(ComponEng STATIC ${ENGINE_SOURCES})

# Include directories (PUBLIC so game can access them)
target_include_directories(ComponEng
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/imgui
)

# GLM experimental
target_compile_definitions(ComponEng PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_compile_options(ComponEng PUBLIC
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
)
target_link_options(ComponEng PUBLIC
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# PRIVATE: don't force these diagnostics onto example/ and tests
target_compile_options(ComponEng PRIVATE
    -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
)
# Vendored code isn't ours to fix; silence it so it can't drown out our warnings
set_source_files_properties(${ENGINE_THIRD_PARTY_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)

# Link dependencies
target_link_libraries(ComponEng
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

if (EXCLUDE_EXAMPLE)
  message(STATUS "Excluding example executables")
else()
  add_subdirectory(example)
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

# Test executables - one per test file
enable_testing()

set(TEST_SOURCES
    tests/test_entity_manager.cpp
    tests/test_component_registry.cpp
    tests/test_archetype.cpp
    tests/test_query.cpp
    tests/test_collision.cpp
    tests/test_serialization.cpp
    tests/test_world.cpp
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE ComponEng gtest_main)
    target_compile_definitions(${TEST_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
