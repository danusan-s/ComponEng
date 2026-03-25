cmake_minimum_required(VERSION 3.10)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)
set(PROJECT_NAME "GameEngine")

# Find dependencies
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

# Collect all source files in src/ folder
file(GLOB SOURCES "src/*.cpp" "src/*.c" "src/core/*.cpp" "src/renderer/*.cpp" "src/systems/*.cpp" "src/glad/*.c" "src/imgui/*.cpp")

# Include header files from the include/ folder
include_directories(include)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Enable GLM experimental extensions (required for gtx/quaternion etc.)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    glfw
    OpenGL::GL
)

# Include directories for the include/ folder
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/imgui
      )

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows-specific options
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux-specific options
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS-specific options
endif()

# cmake_minimum_required(VERSION 3.10)
# project(GameEngine)
#
# set(CMAKE_CXX_STANDARD 17)
#
# # Find dependencies
# find_package(glfw3 3.3 REQUIRED)
# find_package(OpenGL REQUIRED)
#
# # Collect all source files
# file(GLOB SOURCES
#     "src/*.cpp"
#     "src/*.c"
#     "src/core/*.cpp"
#     "src/renderer/*.cpp"
#     "src/systems/*.cpp"
#     "src/glad/*.c"
#     "src/imgui/*.cpp"
# )
#
# # Create STATIC library instead of executable
# add_library(GameEngine STATIC ${SOURCES})
#
# # Include directories (PUBLIC so game can access them)
# target_include_directories(GameEngine
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}/include
#         ${CMAKE_CURRENT_SOURCE_DIR}/include/imgui
# )
#
# # GLM experimental
# target_compile_definitions(GameEngine PUBLIC GLM_ENABLE_EXPERIMENTAL)
#
# # Link dependencies
# target_link_libraries(GameEngine
#     PUBLIC
#         glfw
#         OpenGL::GL
# )
#
# # Debug sanitizers
# if(CMAKE_BUILD_TYPE MATCHES "Debug")
#     target_compile_options(GameEngine PRIVATE -fsanitize=address -fno-omit-frame-pointer)
#     target_link_options(GameEngine PRIVATE -fsanitize=address)
# endif()
#
# # Platform-specific (optional)
# if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
# elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
# endif()
#
