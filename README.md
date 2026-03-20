# Planetary Simulation

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586A4?logo=opengl&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.10%2B-064F8C?logo=cmake&logoColor=white)
![GLFW](https://img.shields.io/badge/GLFW-3.3-20232A)
![Status](https://img.shields.io/badge/Project-Learning%20Project-6C8E5E)

An ECS based game engine built in C++.

## Screenshots and GIFs


## What This Project Does


## Features


## Tech Stack

- C++17
- OpenGL 3.3 Core Profile
- GLFW
- GLAD
- GLM
- FreeType (for text rendering, not implemented yet)
- stb_image
- CMake

## Project Structure


## Dependencies

Make sure these are available before building:

- `CMake` 3.10+
- `OpenGL` 3.3+
- `GLFW` 3.3
- `FreeType`
- `GLAD` included in `include/glad`
- `GLM` installed system-wide or otherwise available to the compiler
- `stb_image` included in `include/stb_image.h`

## Build Instructions

```bash
git clone <repository-url>
cd Game-Engine
cd mkdir build
cd build
cmake ..
make
```

## Running the Simulation

You can run the executable from any directory. For example, from the project root:

```bash
./build/GameEngine
```

## Controls

- `W`, `A`, `S`, `D` - move the camera
- Mouse - look around
- `Left Ctrl` - release or lock the cursor
- `Esc` - quit the application
