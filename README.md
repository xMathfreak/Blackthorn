# Blackthorn

Blackthorn is a simple 2D game engine written in modern C++20, designed for learning and experimentation.
The project is split into a sample game application and a reusable library.

## Dependencies

### Required
- CMake >= 3.16
- C++20-compatible compiler
- OpenGL
- SDL3
- SDL_image
- SDL_ttf

These are expected to be installed on your system and discoverable via `find_package`.

### Included
- glm
- glfw
- glad

## Getting the Source
Clone the repository with submodules:
```bash
git clone --recursive https://github.com/xMathfreak/Blackthorn.git
```
If you already cloned without submodules:
```bash
git submodule update --init --recursive
```

## Building
### Debug Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```
This build:
- Enables debug symbols
- Disables optimizations
- Defines `BLACKTHORN_DEBUG`

### Release Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
This build:
- Enables optimizations and LTO (when supported)
- Defines `BLACKTHORN_RELEASE`

### Compile Commands (Optional)
To generate `compile_commands.json` for IDEs and tooling:
```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
```
This file will appear in the `/build` directory.
### Run the executable
```bash
./build/bin/Game   # Linux/macOS
build\bin\Game.exe # Windows
```

### Note
- The executable is output to `build/bin/`
- Assets are copied automatically after build
- On Windows, required DLLs are copied to the runtime directory