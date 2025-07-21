# Project Structure

## Directory Organization

```
libetude/
├── CMakeLists.txt                # Main CMake configuration
├── include/                      # Public API headers
│   └── libetude/                 # Main include directory
│       ├── api.h                 # Core C API
│       ├── config.h              # Configuration options
│       ├── types.h               # Common type definitions
│       └── cpp/                  # C++ bindings
├── src/                          # Implementation source files
│   ├── core/                     # Core engine components
│   │   ├── kernels/              # Hardware-specific kernels
│   │   │   ├── cpu/              # CPU kernels
│   │   │   ├── simd/             # SIMD optimized kernels
│   │   │   └── gpu/              # GPU kernels (CUDA, OpenCL, Metal)
│   │   ├── tensor/               # Tensor operations
│   │   ├── graph/                # Graph execution engine
│   │   ├── memory/               # Memory management
│   │   └── math/                 # Optimized math functions
│   ├── lef/                      # LEF format implementation
│   │   ├── format/               # Format definitions
│   │   ├── loader/               # Model loading
│   │   └── extensions/           # Extension model support
│   ├── runtime/                  # Runtime system
│   │   ├── scheduler/            # Task scheduling
│   │   ├── profiler/             # Performance profiling
│   │   └── error/                # Error handling
│   ├── audio/                    # Audio processing
│   │   ├── io/                   # Audio I/O
│   │   ├── dsp/                  # DSP algorithms
│   │   └── vocoder/              # Vocoder implementation
│   └── platform/                 # Platform-specific code
│       ├── windows/              # Windows implementation
│       ├── macos/                # macOS implementation
│       ├── linux/                # Linux implementation
│       ├── android/              # Android implementation
│       └── ios/                  # iOS implementation
├── bindings/                     # Language bindings
│   ├── cpp/                      # C++ bindings
│   ├── jni/                      # Android JNI bindings
│   └── objc/                     # Objective-C bindings
├── tools/                        # Development tools
│   ├── model_converter/          # Model conversion tools
│   ├── profiler/                 # Profiling tools
│   └── benchmark/                # Benchmark suite
├── tests/                        # Test suite
│   ├── unit/                     # Unit tests
│   ├── integration/              # Integration tests
│   └── performance/              # Performance tests
├── examples/                     # Example applications
│   ├── basic_tts/                # Basic text-to-speech example
│   ├── streaming/                # Streaming synthesis example
│   └── plugins/                  # Plugin usage examples
└── docs/                         # Documentation
    ├── api/                      # API documentation
    ├── architecture/             # Architecture documentation
    └── examples/                 # Example documentation
```

## Architecture Layers

The project follows a layered architecture approach:

1. **Kernel Layer**: Low-level hardware-optimized operations
2. **Math/Linear Algebra Layer**: Fast math functions and matrix operations
3. **Tensor Layer**: Multi-dimensional data processing and memory management
4. **Graph Layer**: Deep learning model computation graph execution
5. **Runtime Layer**: Task scheduling, memory allocation, profiling
6. **LEF Format Layer**: Model serialization, compression, loading
7. **Audio Layer**: Voice-specific DSP processing
8. **External Interface Layer**: Various language and platform bindings

## Code Style and Conventions

- **C Code**: Follow C11 standard with platform-specific extensions when necessary
- **C++ Code**: Follow C++17 standard for bindings and tools
- **Naming Conventions**:
  - Functions: `lowercase_with_underscores`
  - Types/Structs: `PascalCase`
  - Constants: `UPPERCASE_WITH_UNDERSCORES`
  - Internal functions: `_prefixed_with_underscore`
- **Error Handling**: Error codes with detailed error messages
- **Documentation**: Doxygen-style comments for all public APIs
- **Testing**: Unit tests for all components, integration tests for workflows