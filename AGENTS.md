---
trigger: always_on
glob:
description:
---

# AGENTS.md

This file provides guidance to AI assistants (Claude Code, Antigravity, etc.) when working with code in this repository.

## Project Overview

DaltonLens is a cross-platform desktop utility for colorblind accessibility. It runs as a system tray application providing:
- Screen region grabbing with color overlay analysis
- GPU-accelerated color filters (Daltonize algorithm, HSV transforms, red-blue flip)
- Similar color highlighting for charts/plots
- Color vision deficiency simulation

**Platforms**: macOS (10.14+), Windows (8.1+), Linux (GTK3-based)
**Language**: C++17
**License**: BSD 2-Clause

## Build Commands

### macOS (Recommended - Xcode)
```bash
cd DaltonLens/macOS
xcodebuild clean build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
```

### macOS/Linux (CMake)
```bash
# Prefer Ninja generator for best build performance
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target glfw-deps clip-deps
cmake --build build --target all
```

> **Note**: If glfw-deps or clip-deps fail, prebuilt libraries may already exist in `deps/prebuilt/`. You can skip directly to building `all`.

### Linux Dependencies
```bash
sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libappindicator3-dev libxcb1-dev libgtk-3-dev
```

### Windows
```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Running Tests

```bash
cd build
ctest --verbose -C Release

# Exclude GPU tests (for headless environments):
ctest --verbose --exclude-regex test_Filters
```

Test framework: **utest** (header-only, in `deps/utest/`)
Test files: `tests/test_Utils.cpp`, `tests/test_Filters.cpp`
Ground truth images: `tests/images/`

## Architecture

```
Dalton/           Core image processing library
  - Image.h       Template-based image containers (ImageSRGBA, ImageLinearRGB, etc.)
  - Filters.h     GPU filter base class and implementations
  - ColorConversion.h  Color space transformations (sRGB, LMS, HSV, Lab)
  - OpenGL.h      Shader, texture, framebuffer abstractions
  - OpenGL_Shaders.cpp  GLSL shader source code (inline strings)

DaltonGUI/        GUI layer (ImGui + GLFW)
  - DaltonLensGUI       Main application controller
  - ImageViewerWindow   Image display with zoom/pan
  - GrabScreenAreaWindow  Screen region selection overlay
  - ImageViewerControlsWindow  Filter parameter UI
  - PlatformSpecific_*.cpp  Per-platform implementations (macOS/Linux/Windows)

DaltonLens/       Application entry points
  - other/main.cpp      Windows/Linux entry
  - macOS/              Xcode project and macOS-specific code

deps/             All dependencies embedded (no package managers)
  - prebuilt/     Pre-compiled binaries per OS
```

### Key Patterns

- **Pimpl idiom**: Public APIs hide implementation details (e.g., `DaltonLensGUI::Impl`)
- **Platform abstraction**: Complete separate implementations in `PlatformSpecific_*.cpp` files
- **GPU-first processing**: Filters implement both `applyCPU()` and `enableGLShader()` methods
- **Color pipeline**: sRGB → Linear RGB → LMS → filter → back to sRGB

### Data Flow

1. `ScreenGrabber` (platform-specific) captures screen → `ImageSRGBA` + `GLTexture`
2. `Filter_*` classes apply GPU shaders via `GLFilterProcessor`
3. `GLImageRenderer` displays filtered result as fullscreen quad
4. ImGui handles all UI overlays and controls

## Dependencies

All embedded in `deps/` - no external package managers. Key libraries:
- GLFW (windowing), imgui (GUI), gl3w (OpenGL loader), stb (image I/O)
- tray (system tray), clip (clipboard), nativefiledialog-extended (file dialogs)

Rebuild prebuilt binaries: `deps/update_dependencies_prebuilt_binaries.sh`
