cmake_minimum_required(VERSION 3.10)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)
set(PROJECT_NAME "GameEngine")

# Find dependencies
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

# Collect all source files in src/ folder
file(GLOB SOURCES "src/*.cpp" "src/*.c" "src/imgui/*.cpp")

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

