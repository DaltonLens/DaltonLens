# Homebrew Distribution Migration - Status

## Current State (January 2025)

The macOS CMake build is **working**. DaltonLens.app can now be built without Xcode using:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build DaltonLens
# Output: build/DaltonLens/macOS-cmake/DaltonLens.app
```

## Completed Work

### Phase 1: CMake macOS Entry Point ✅
- Created `DaltonLens/macOS-cmake/` directory with:
  - `main.mm` - Objective-C++ entry point with NSStatusItem menu bar
  - `CMakeLists.txt` - Bundle configuration
  - `Info.plist.in` - Template with LSUIElement=true (menu bar app)
  - `DaltonLens.icns` - App icon
- Implemented SMAppService for launch-at-startup (macOS 13+)
- Updated root `CMakeLists.txt` for macOS 13.0 deployment target

### Phase 2: Build System Integration ✅
- Added MASShortcut library sources to CMake build
- Added GLKit and Carbon frameworks
- Fixed void* → CF type casts in MASShortcut files for modern compilers
- Updated AGENTS.md with Ninja build preference

## Remaining Work

### Phase 3: Homebrew Formula
- [ ] Create `packaging/homebrew/daltonlens.rb` formula
- [ ] Define dependencies (cmake, ninja)
- [ ] Add caveats for required permissions:
  - Screen Recording (System Preferences → Privacy & Security)
  - Accessibility (if needed)
- [ ] Test local installation: `brew install --build-from-source ./daltonlens.rb`
- [ ] Submit to homebrew-core or create homebrew-daltonlens tap

### Phase 4: Documentation
- [ ] Update README.md with Homebrew installation instructions
- [ ] Document permission requirements for first-time users

## Technical Notes

- **Minimum macOS**: 13.0 (Ventura) - required for SMAppService API
- **Architecture**: Universal (arm64 + x86_64) when built with appropriate flags
- **No code signing required**: Homebrew builds from source on user's machine
- **Linker warnings**: The macOS version mismatch warnings are benign (prebuilt deps built on newer macOS)
