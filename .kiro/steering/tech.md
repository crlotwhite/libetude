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
- **Windows 컴파일러**: MSBuild (MSVC) 권장, MinGW-GCC 대안 지원
- **Platform Support**:
  - Desktop: Windows (MSVC/MinGW), macOS, Linux
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

#### Windows 빌드 환경 선택

**권장: MSBuild (Visual Studio) - 최고 성능**
```cmd
# Visual Studio 2019/2022 사용 (권장)
cmake -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# 또는 Ninja 사용 (더 빠른 빌드)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**대안: MinGW-GCC - 크로스 플랫폼 일관성**
```bash
# MSYS2 환경에서
cmake -B build -G "Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**Windows 컴파일러 선택 가이드:**
- **MSBuild (MSVC) 사용 시기:**
  - 최고 성능이 필요한 상용 배포
  - Windows 전용 최적화 활용 (/arch:AVX2)
  - Visual Studio 디버깅 도구 사용
  - CI에서 자동으로 빌드 및 테스트됨
- **Intel C++ Compiler 사용 시기:**
  - 극한 성능 최적화 필요 (/O3 /Qipo)
  - Intel 프로세서 전용 최적화
  - 상용 배포용 특별 빌드
- **MinGW-GCC 사용 시기:**
  - 크로스 플랫폼 개발팀
  - 오픈소스 배포 우선
  - CI/CD 자동화
  - 런타임 의존성 최소화

**macOS 컴파일러 최적화:**
- **Universal Binary**: x86_64 + arm64 지원
- **Apple Silicon 네이티브**: -march=armv8-a 최적화
- **Metal GPU 가속**: 활성화됨

#### 기타 플랫폼

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

## Windows 빌드 최적화 팁

### SIMD 최적화 활성화
```cmd
# SIMD 최적화 활성화 (성능 향상)
cmake -B build -DLIBETUDE_ENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Release
```

### Intel C++ Compiler 사용 (MSVC 환경)
```cmd
# Intel oneAPI 설치 후
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat"
cmake -B build -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="/arch:AVX2 /O3 /Qipo" -DCMAKE_CXX_FLAGS="/arch:AVX2 /O3 /Qipo"
```

### macOS Apple Silicon 최적화
```bash
# Apple Silicon 네이티브 빌드
cmake -B build -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_C_FLAGS="-march=armv8-a -mtune=native -O3" -DCMAKE_CXX_FLAGS="-march=armv8-a -mtune=native -O3" -DCMAKE_BUILD_TYPE=Release

# Universal Binary (Intel + Apple Silicon)
cmake -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_BUILD_TYPE=Release
```

### 정적 링킹 (MinGW 환경)
```bash
# 런타임 의존성 최소화
cmake -B build -DCMAKE_C_FLAGS="-static-libgcc" -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++" -DCMAKE_BUILD_TYPE=Release
```