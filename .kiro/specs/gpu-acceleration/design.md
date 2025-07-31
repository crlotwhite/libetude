# 설계 문서 (Design Document)

## 개요 (Overview)

LibEtude GPU 가속 기능은 CUDA, Apple Metal, OpenCL 등 다양한 GPU 백엔드를 지원하여 음성 합성 성능을 크게 향상시키는 시스템입니다. 기존 CPU 기반 아키텍처와 완전히 호환되면서도 GPU 하드웨어가 사용 가능할 때 자동으로 가속을 적용하는 투명한 가속 레이어를 제공합니다.

핵심 설계 원칙:
- **투명성**: 기존 API 변경 없이 GPU 가속 적용
- **호환성**: GPU 미지원 환경에서 CPU 폴백 보장
- **확장성**: 새로운 GPU 백엔드 쉽게 추가 가능
- **성능**: 메모리 전송 최소화 및 커널 최적화

## 아키텍처 (Architecture)

### 계층 구조 (Layered Architecture)

```
┌─────────────────────────────────────────────────────────────┐
│                    LibEtude Public API                     │
├─────────────────────────────────────────────────────────────┤
│                GPU Acceleration Layer                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │ Hardware Detect │  │ Backend Manager │  │ Memory Mgr  │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    GPU Backend Layer                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ CUDA Backend│  │Metal Backend│  │ OpenCL Backend      │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                   Kernel Execution Layer                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │Matrix Kernels│  │ DSP Kernels │  │ Memory Kernels      │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    CPU Fallback Layer                      │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │            Existing CPU Implementation             │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 주요 컴포넌트 (Key Components)

#### 1. GPU 하드웨어 감지 시스템 (Hardware Detection System)
- **목적**: 시스템 초기화 시 사용 가능한 GPU 하드웨어 자동 감지
- **구현**: 각 백엔드별 감지 로직 (CUDA Runtime API, Metal Device 쿼리, OpenCL Platform 검색)
- **우선순위**: NVIDIA GPU (CUDA) > Apple Silicon (Metal) > AMD GPU (OpenCL) > CPU 폴백

#### 2. 백엔드 관리자 (Backend Manager)
- **목적**: 여러 GPU 백엔드 중 최적의 백엔드 선택 및 관리
- **기능**: 백엔드 초기화, 전환, 상태 모니터링
- **인터페이스**: 통일된 GPU 연산 인터페이스 제공

#### 3. GPU 메모리 관리자 (GPU Memory Manager)
- **목적**: GPU 메모리 할당, 해제, 전송 최적화
- **기능**: 메모리 풀링, 자동 가비지 컬렉션, CPU-GPU 데이터 동기화
- **최적화**: 메모리 재사용, 비동기 전송, 메모리 압축

## 컴포넌트 및 인터페이스 (Components and Interfaces)

### 1. 핵심 인터페이스 정의

```c
// GPU 백엔드 추상화 인터페이스
typedef struct {
    // 백엔드 초기화/정리
    LibEtudeError (*initialize)(void** context);
    LibEtudeError (*cleanup)(void* context);

    // 메모리 관리
    LibEtudeError (*malloc)(void* context, void** ptr, size_t size);
    LibEtudeError (*free)(void* context, void* ptr);
    LibEtudeError (*memcpy_h2d)(void* context, void* dst, const void* src, size_t size);
    LibEtudeError (*memcpy_d2h)(void* context, void* dst, const void* src, size_t size);

    // 커널 실행
    LibEtudeError (*execute_kernel)(void* context, const char* kernel_name,
                                   void** args, size_t* arg_sizes, int arg_count,
                                   int grid_x, int grid_y, int grid_z,
                                   int block_x, int block_y, int block_z);

    // 동기화
    LibEtudeError (*synchronize)(void* context);

    // 성능 모니터링
    LibEtudeError (*get_memory_info)(void* context, size_t* free, size_t* total);
    LibEtudeError (*get_execution_time)(void* context, float* time_ms);
} GPUBackendInterface;
```

### 2. CUDA 백엔드 구현

```c
// CUDA 백엔드 컨텍스트
typedef struct {
    CUdevice device;
    CUcontext context;
    CUstream stream;
    CUmodule module;

    // 성능 측정
    CUevent start_event;
    CUevent stop_event;

    // 메모리 풀
    void* memory_pool;
    size_t pool_size;
} CUDABackendContext;

// CUDA 커널 실행 구현
LibEtudeError cuda_execute_kernel(void* context, const char* kernel_name,
                                 void** args, size_t* arg_sizes, int arg_count,
                                 int grid_x, int grid_y, int grid_z,
                                 int block_x, int block_y, int block_z) {
    CUDABackendContext* cuda_ctx = (CUDABackendContext*)context;
    CUfunction kernel;

    // 커널 함수 가져오기
    CUresult result = cuModuleGetFunction(&kernel, cuda_ctx->module, kernel_name);
    if (result != CUDA_SUCCESS) {
        return LIBETUDE_ERROR_GPU_KERNEL_NOT_FOUND;
    }

    // 커널 실행
    result = cuLaunchKernel(kernel,
                           grid_x, grid_y, grid_z,
                           block_x, block_y, block_z,
                           0, cuda_ctx->stream, args, NULL);

    return (result == CUDA_SUCCESS) ? LIBETUDE_SUCCESS : LIBETUDE_ERROR_GPU_EXECUTION;
}
```

### 3. Metal 백엔드 구현

```objc
// Metal 백엔드 컨텍스트 (Objective-C)
@interface MetalBackendContext : NSObject
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLLibrary> library;
@property (nonatomic, strong) NSMutableDictionary<NSString*, id<MTLComputePipelineState>>* pipelineStates;
@property (nonatomic, strong) id<MTLBuffer> memoryPool;
@end

// Metal 커널 실행 구현
LibEtudeError metal_execute_kernel(void* context, const char* kernel_name,
                                  void** args, size_t* arg_sizes, int arg_count,
                                  int grid_x, int grid_y, int grid_z,
                                  int block_x, int block_y, int block_z) {
    MetalBackendContext* metal_ctx = (__bridge MetalBackendContext*)context;

    NSString* kernelName = [NSString stringWithUTF8String:kernel_name];
    id<MTLComputePipelineState> pipelineState = metal_ctx.pipelineStates[kernelName];

    if (!pipelineState) {
        return LIBETUDE_ERROR_GPU_KERNEL_NOT_FOUND;
    }

    id<MTLCommandBuffer> commandBuffer = [metal_ctx.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

    [encoder setComputePipelineState:pipelineState];

    // 인자 설정
    for (int i = 0; i < arg_count; i++) {
        [encoder setBytes:args[i] length:arg_sizes[i] atIndex:i];
    }

    // 스레드 그룹 설정
    MTLSize threadsPerThreadgroup = MTLSizeMake(block_x, block_y, block_z);
    MTLSize threadgroupsPerGrid = MTLSizeMake(grid_x, grid_y, grid_z);

    [encoder dispatchThreadgroups:threadgroupsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
    [encoder endEncoding];
    [commandBuffer commit];

    return LIBETUDE_SUCCESS;
}
```

### 4. 통합 GPU 관리자

```c
// GPU 관리자 구조체
typedef struct {
    GPUBackendType active_backend;
    GPUBackendInterface* backend_interface;
    void* backend_context;

    // 설정
    bool gpu_enabled;
    bool auto_fallback;
    GPUBackendType preferred_backend;

    // 성능 통계
    GPUPerformanceStats stats;
} GPUManager;

// GPU 관리자 초기화
LibEtudeError gpu_manager_initialize(GPUManager** manager, const GPUConfig* config) {
    *manager = (GPUManager*)calloc(1, sizeof(GPUManager));
    if (!*manager) return LIBETUDE_ERROR_OUT_OF_MEMORY;

    GPUManager* mgr = *manager;
    mgr->gpu_enabled = config ? config->enabled : true;
    mgr->auto_fallback = config ? config->auto_fallback : true;
    mgr->preferred_backend = config ? config->preferred_backend : GPU_BACKEND_AUTO;

    // 하드웨어 감지 및 백엔드 선택
    GPUBackendType detected_backend = detect_best_gpu_backend();
    if (mgr->preferred_backend != GPU_BACKEND_AUTO) {
        detected_backend = mgr->preferred_backend;
    }

    // 백엔드 초기화
    LibEtudeError error = initialize_gpu_backend(mgr, detected_backend);
    if (error != LIBETUDE_SUCCESS && mgr->auto_fallback) {
        // CPU 폴백
        mgr->active_backend = GPU_BACKEND_CPU;
        return LIBETUDE_SUCCESS;
    }

    return error;
}
```

## 데이터 모델 (Data Models)

### 1. GPU 텐서 구조체

```c
typedef struct {
    // 기본 텐서 정보
    LibEtudeTensor base;

    // GPU 특화 정보
    GPUBackendType backend_type;
    void* gpu_data;           // GPU 메모리 포인터
    void* cpu_data;           // CPU 메모리 포인터 (동기화용)
    bool data_on_gpu;         // 데이터가 GPU에 있는지 여부
    bool data_on_cpu;         // 데이터가 CPU에 있는지 여부
    bool dirty_gpu;           // GPU 데이터가 수정되었는지
    bool dirty_cpu;           // CPU 데이터가 수정되었는지

    // 메모리 관리
    size_t gpu_memory_size;
    GPUMemoryPool* memory_pool;
} GPUTensor;
```

### 2. GPU 커널 정의

```c
typedef struct {
    char name[64];
    GPUBackendType backend_type;

    union {
        struct {
            CUfunction cuda_function;
            int min_grid_size;
            int block_size;
        } cuda;

        struct {
            void* metal_pipeline_state;  // id<MTLComputePipelineState>
            MTLSize threadgroup_size;
        } metal;

        struct {
            cl_kernel opencl_kernel;
            size_t work_group_size;
        } opencl;
    } backend_data;

    // 커널 메타데이터
    int num_parameters;
    GPUKernelParameter parameters[16];
} GPUKernel;
```

### 3. 성능 통계 구조체

```c
typedef struct {
    // 실행 통계
    uint64_t total_kernel_executions;
    uint64_t total_memory_transfers;
    double total_execution_time_ms;
    double total_transfer_time_ms;

    // 메모리 통계
    size_t peak_gpu_memory_usage;
    size_t current_gpu_memory_usage;
    size_t total_gpu_memory;

    // 성능 지표
    double average_kernel_time_ms;
    double average_transfer_time_ms;
    double gpu_utilization_percent;

    // 백엔드별 통계
    struct {
        uint64_t cuda_executions;
        uint64_t metal_executions;
        uint64_t opencl_executions;
        uint64_t cpu_fallbacks;
    } backend_stats;
} GPUPerformanceStats;
```

## 오류 처리 (Error Handling)

### 1. GPU 특화 오류 코드

```c
typedef enum {
    // 기존 LibEtude 오류 코드들...

    // GPU 관련 오류
    LIBETUDE_ERROR_GPU_NOT_AVAILABLE = 1000,
    LIBETUDE_ERROR_GPU_INITIALIZATION_FAILED,
    LIBETUDE_ERROR_GPU_OUT_OF_MEMORY,
    LIBETUDE_ERROR_GPU_KERNEL_NOT_FOUND,
    LIBETUDE_ERROR_GPU_KERNEL_EXECUTION_FAILED,
    LIBETUDE_ERROR_GPU_MEMORY_TRANSFER_FAILED,
    LIBETUDE_ERROR_GPU_SYNCHRONIZATION_FAILED,
    LIBETUDE_ERROR_GPU_BACKEND_NOT_SUPPORTED,
    LIBETUDE_ERROR_GPU_CONTEXT_LOST,
    LIBETUDE_ERROR_GPU_DRIVER_VERSION_MISMATCH
} LibEtudeError;
```

### 2. 오류 복구 전략

```c
// GPU 오류 처리 및 복구
LibEtudeError handle_gpu_error(GPUManager* manager, LibEtudeError error) {
    switch (error) {
        case LIBETUDE_ERROR_GPU_OUT_OF_MEMORY:
            // 메모리 정리 시도
            gpu_memory_cleanup(manager);
            return LIBETUDE_SUCCESS;

        case LIBETUDE_ERROR_GPU_CONTEXT_LOST:
            // 컨텍스트 재초기화
            return reinitialize_gpu_context(manager);

        case LIBETUDE_ERROR_GPU_KERNEL_EXECUTION_FAILED:
            // CPU 폴백 활성화
            if (manager->auto_fallback) {
                manager->active_backend = GPU_BACKEND_CPU;
                return LIBETUDE_SUCCESS;
            }
            return error;

        default:
            return error;
    }
}
```

## 테스트 전략 (Testing Strategy)

### 1. 단위 테스트 (Unit Tests)

- **하드웨어 감지 테스트**: 다양한 GPU 환경에서 올바른 백엔드 선택 확인
- **메모리 관리 테스트**: GPU 메모리 할당/해제, 누수 검사
- **커널 실행 테스트**: 각 백엔드별 커널 실행 정확성 검증
- **오류 처리 테스트**: 다양한 오류 상황에서 적절한 복구 동작 확인

### 2. 통합 테스트 (Integration Tests)

- **CPU-GPU 호환성 테스트**: 동일한 입력에 대해 CPU와 GPU 결과 비교
- **백엔드 전환 테스트**: 런타임에 백엔드 변경 시 안정성 확인
- **성능 테스트**: GPU 가속으로 인한 성능 향상 측정
- **메모리 압박 테스트**: GPU 메모리 부족 상황에서 폴백 동작 확인

### 3. 성능 테스트 (Performance Tests)

```c
// 성능 벤치마크 예시
typedef struct {
    const char* test_name;
    void (*cpu_function)(const float* input, float* output, size_t size);
    void (*gpu_function)(GPUManager* manager, const float* input, float* output, size_t size);
    size_t input_size;
    int iterations;
} PerformanceBenchmark;

void run_performance_benchmark(const PerformanceBenchmark* benchmark) {
    // CPU 성능 측정
    double cpu_time = measure_cpu_execution(benchmark);

    // GPU 성능 측정
    double gpu_time = measure_gpu_execution(benchmark);

    // 결과 비교 및 리포트
    double speedup = cpu_time / gpu_time;
    printf("Benchmark: %s\n", benchmark->test_name);
    printf("CPU Time: %.3f ms\n", cpu_time);
    printf("GPU Time: %.3f ms\n", gpu_time);
    printf("Speedup: %.2fx\n", speedup);
}
```

### 4. 플랫폼별 테스트

- **Windows + CUDA**: NVIDIA GPU 환경에서 CUDA 백엔드 테스트
- **macOS + Metal**: Apple Silicon 및 Intel Mac에서 Metal 백엔드 테스트
- **Linux + OpenCL**: AMD GPU 환경에서 OpenCL 백엔드 테스트
- **크로스 플랫폼**: 동일한 코드가 모든 플랫폼에서 동작하는지 확인

## 성능 최적화 전략

### 1. 메모리 최적화

- **메모리 풀링**: 자주 사용되는 크기의 메모리 블록 미리 할당
- **비동기 전송**: CPU-GPU 메모리 전송과 커널 실행 오버랩
- **메모리 압축**: 대용량 텐서 데이터 압축 저장
- **지연 동기화**: 필요할 때만 CPU-GPU 데이터 동기화

### 2. 커널 최적화

- **커널 융합**: 여러 작은 커널을 하나의 큰 커널로 결합
- **스레드 블록 최적화**: 하드웨어별 최적 스레드 블록 크기 사용
- **공유 메모리 활용**: GPU 공유 메모리를 활용한 데이터 재사용
- **텐서 코어 활용**: NVIDIA Tensor Core 등 특화 하드웨어 활용

### 3. 실행 최적화

- **스트림 병렬화**: 여러 CUDA 스트림 또는 Metal 커맨드 큐 활용
- **파이프라이닝**: 데이터 전송과 커널 실행 파이프라인화
- **동적 로드 밸런싱**: CPU와 GPU 간 작업 분배 최적화
- **적응적 백엔드 선택**: 작업 특성에 따른 최적 백엔드 동적 선택