# LibEtude Technical Stack & Build System

## Core Technologies

- **Programming Languages**: C/C++ for core engine, with bindings for other languages
- **SIMD Optimization**: SSE/AVX for x86, NEON for ARM
- **GPU Acceleration**: CUDA, OpenCL, Metal support
- **Audio Processing**: Specialized DSP kernels for voice synthesis
- **Memory Management**: Custom memory pool and allocation system
- **Math Libraries**: FastApprox-based optimized math functions

## Build System

- **CMake**: Cross-platform build system for all targets
- **Platform Support**:
  - Desktop: Windows, macOS, Linux
  - Mobile: Android, iOS
  - Embedded: Various ARM platforms

## Dependencies

- Minimal external dependencies to maintain control over optimization
- Core math functions implemented in-house for maximum performance
- Platform-specific audio backends (DirectSound/WASAPI, Android/iOS native APIs)

## Common Commands

### Building the Project

```bash
# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Install
cmake --install build
```

### Platform-Specific Builds

```bash
# Android build
cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=<path-to-android-ndk>/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a

# iOS build
cmake -B build-ios -DCMAKE_TOOLCHAIN_FILE=ios.toolchain.cmake -DPLATFORM=OS64

# Embedded build
cmake -B build-embedded -DCMAKE_TOOLCHAIN_FILE=<embedded-toolchain>.cmake -DLIBETUDE_MINIMAL=ON
```

### Running Tests

```bash
# Run all tests
cd build && ctest

# Run specific test suite
cd build && ctest -R kernel_tests

# Run benchmarks
cd build && ./bin/libetude_benchmarks
```

### Profiling

```bash
# Generate performance profile
./bin/libetude_profile --model=<model_path> --output=profile.json

# Memory usage analysis
./bin/libetude_memcheck --model=<model_path>
```