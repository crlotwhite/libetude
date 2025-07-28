/**
 * @file desktop_optimization.c
 * @brief 데스크톱 환경 최적화 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 데스크톱 환경에서의 멀티코어 최적화, GPU 가속 통합, 오디오 백엔드 최적화를 구현합니다.
 */

#include "libetude/desktop_optimization.h"
#include "libetude/error.h"
#include "libetude/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <pthread.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/thread_policy.h>
    #include <time.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <sched.h>
    #include <sys/resource.h>
    #include <unistd.h>
    #include <time.h>
#endif

// ============================================================================
// 내부 함수 선언
// ============================================================================

static LibEtudeErrorCode _init_multicore_threads(LibEtudeMulticoreOptimizer* multicore);
static void _destroy_multicore_threads(LibEtudeMulticoreOptimizer* multicore);
static void* _worker_thread_function(void* arg);
static LibEtudeErrorCode _set_thread_priority(pthread_t thread, int priority);
static LibEtudeErrorCode _set_thread_affinity(pthread_t thread, uint32_t affinity_mask);

static LibEtudeErrorCode _init_gpu_context(LibEtudeGPUAccelerator* gpu_accel);
static void _destroy_gpu_context(LibEtudeGPUAccelerator* gpu_accel);
static LibEtudeErrorCode _create_gpu_memory_pool(LibEtudeGPUAccelerator* gpu_accel);

static LibEtudeErrorCode _init_audio_buffers(LibEtudeAudioBackendOptimizer* audio_opt);
static void _destroy_audio_buffers(LibEtudeAudioBackendOptimizer* audio_opt);
static void _audio_callback_wrapper(float* buffer, int num_frames, void* user_data);

// ============================================================================
// 데스크톱 최적화 초기화 및 해제
// ============================================================================

LibEtudeErrorCode libetude_desktop_optimizer_init(LibEtudeDesktopOptimizer* optimizer) {
    if (!optimizer) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(optimizer, 0, sizeof(LibEtudeDesktopOptimizer));

    // 하드웨어 정보 감지
    LibEtudeErrorCode result = libetude_hardware_detect(&optimizer->hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    // 멀티코어 최적화 초기화
    result = libetude_multicore_optimizer_init(&optimizer->multicore, &optimizer->hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    // GPU 가속 초기화 (GPU가 사용 가능한 경우)
    if (optimizer->hardware_info.gpu.available) {
        result = libetude_gpu_accelerator_init(&optimizer->gpu_accel, &optimizer->hardware_info);
        if (result != LIBETUDE_SUCCESS) {
            // GPU 초기화 실패는 치명적이지 않음
            printf("Warning: GPU acceleration initialization failed, continuing with CPU-only mode\n");
        }
    }

    // 오디오 백엔드 최적화 초기화
    result = libetude_audio_backend_optimizer_init(&optimizer->audio, &optimizer->hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        libetude_multicore_optimizer_destroy(&optimizer->multicore);
        if (optimizer->gpu_accel.initialized) {
            libetude_gpu_accelerator_destroy(&optimizer->gpu_accel);
        }
        return result;
    }

    // 자동 최적화 수행
    result = libetude_desktop_optimizer_auto_optimize(optimizer);
    if (result != LIBETUDE_SUCCESS) {
        libetude_desktop_optimizer_destroy(optimizer);
        return result;
    }

    optimizer->initialized = true;
    return LIBETUDE_SUCCESS;
}

void libetude_desktop_optimizer_destroy(LibEtudeDesktopOptimizer* optimizer) {
    if (!optimizer || !optimizer->initialized) {
        return;
    }

    // 각 컴포넌트 해제
    libetude_audio_backend_optimizer_destroy(&optimizer->audio);

    if (optimizer->gpu_accel.initialized) {
        libetude_gpu_accelerator_destroy(&optimizer->gpu_accel);
    }

    libetude_multicore_optimizer_destroy(&optimizer->multicore);

    // 구조체 초기화
    memset(optimizer, 0, sizeof(LibEtudeDesktopOptimizer));
}
/
/ ============================================================================
// 멀티코어 최적화 구현
// ============================================================================

LibEtudeErrorCode libetude_multicore_optimizer_init(LibEtudeMulticoreOptimizer* multicore,
                                                    const LibEtudeHardwareInfo* hardware_info) {
    if (!multicore || !hardware_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(multicore, 0, sizeof(LibEtudeMulticoreOptimizer));

    // 기본 설정 초기화
    multicore->config.worker_thread_count = libetude_hardware_get_optimal_thread_count(&hardware_info->cpu);
    multicore->config.audio_thread_priority = 95;  // 높은 우선순위
    multicore->config.compute_thread_priority = 50; // 보통 우선순위
    multicore->config.enable_numa_optimization = (hardware_info->cpu.physical_cores > 8);
    multicore->config.enable_cpu_affinity = true;
    multicore->config.cpu_affinity_mask = 0; // 자동 설정

    // 작업 스케줄러 생성
    multicore->scheduler = et_task_scheduler_create(1024, multicore->config.worker_thread_count);
    if (!multicore->scheduler) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 워커 스레드 초기화
    LibEtudeErrorCode result = _init_multicore_threads(multicore);
    if (result != LIBETUDE_SUCCESS) {
        libetude_task_scheduler_destroy(multicore->scheduler);
        return result;
    }

    // CPU 친화성 자동 설정
    result = libetude_multicore_auto_configure(multicore, &hardware_info->cpu);
    if (result != LIBETUDE_SUCCESS) {
        libetude_multicore_optimizer_destroy(multicore);
        return result;
    }

    return LIBETUDE_SUCCESS;
}

void libetude_multicore_optimizer_destroy(LibEtudeMulticoreOptimizer* multicore) {
    if (!multicore) {
        return;
    }

    // 워커 스레드 해제
    _destroy_multicore_threads(multicore);

    // 작업 스케줄러 해제
    if (multicore->scheduler) {
        et_task_scheduler_destroy(multicore->scheduler);
        multicore->scheduler = NULL;
    }

    // 구조체 초기화
    memset(multicore, 0, sizeof(LibEtudeMulticoreOptimizer));
}

LibEtudeErrorCode libetude_multicore_auto_configure(LibEtudeMulticoreOptimizer* multicore,
                                                    const LibEtudeHardwareCPUInfo* cpu_info) {
    if (!multicore || !cpu_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 최적 스레드 수 계산
    uint32_t optimal_threads = libetude_hardware_get_optimal_thread_count(cpu_info);

    // 성능 등급에 따른 조정
    if (cpu_info->physical_cores >= 8) {
        // 고성능 CPU: 더 많은 스레드 사용
        optimal_threads = cpu_info->physical_cores;
    } else if (cpu_info->physical_cores >= 4) {
        // 중간 성능 CPU: 물리 코어 수 사용
        optimal_threads = cpu_info->physical_cores;
    } else {
        // 저성능 CPU: 논리 코어 수 사용하되 최대 4개로 제한
        optimal_threads = (cpu_info->logical_cores > 4) ? 4 : cpu_info->logical_cores;
    }

    multicore->config.worker_thread_count = optimal_threads;

    // CPU 친화성 마스크 계산
    uint32_t affinity_mask = 0;
    for (uint32_t i = 0; i < optimal_threads && i < 32; i++) {
        affinity_mask |= (1U << i);
    }
    multicore->config.cpu_affinity_mask = affinity_mask;

    // NUMA 최적화 설정
    multicore->config.enable_numa_optimization = (cpu_info->physical_cores > 8);

    return LIBETUDE_SUCCESS;
}

LibEtudeErrorCode libetude_multicore_set_cpu_affinity(LibEtudeMulticoreOptimizer* multicore,
                                                      uint32_t affinity_mask) {
    if (!multicore) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    multicore->config.cpu_affinity_mask = affinity_mask;

    // 기존 스레드들에 친화성 적용
    if (multicore->worker_threads && multicore->config.enable_cpu_affinity) {
        for (uint32_t i = 0; i < multicore->config.worker_thread_count; i++) {
            if (multicore->thread_active[i]) {
                _set_thread_affinity(multicore->worker_threads[i], affinity_mask);
            }
        }
    }

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// GPU 가속 구현
// ============================================================================

LibEtudeErrorCode libetude_gpu_accelerator_init(LibEtudeGPUAccelerator* gpu_accel,
                                                const LibEtudeHardwareInfo* hardware_info) {
    if (!gpu_accel || !hardware_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(gpu_accel, 0, sizeof(LibEtudeGPUAccelerator));

    // GPU 정보 복사
    gpu_accel->gpu_info = hardware_info->gpu;

    // GPU가 사용 불가능한 경우
    if (!hardware_info->gpu.available) {
        return LIBETUDE_ERROR_HARDWARE;
    }

    // 기본 설정 초기화
    gpu_accel->config.preferred_backend = hardware_info->gpu.backend;
    gpu_accel->config.enable_mixed_precision = true;
    gpu_accel->config.enable_tensor_cores = (hardware_info->gpu.backend == LIBETUDE_GPU_CUDA);
    gpu_accel->config.gpu_memory_limit_mb = (uint32_t)(hardware_info->gpu.total_memory / (1024 * 1024) * 0.8); // 80% 사용
    gpu_accel->config.gpu_utilization_target = 0.85f; // 85% 목표 사용률

    // GPU 컨텍스트 초기화
    LibEtudeErrorCode result = _init_gpu_context(gpu_accel);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    // GPU 메모리 풀 생성
    result = _create_gpu_memory_pool(gpu_accel);
    if (result != LIBETUDE_SUCCESS) {
        _destroy_gpu_context(gpu_accel);
        return result;
    }

    gpu_accel->initialized = true;
    return LIBETUDE_SUCCESS;
}

void libetude_gpu_accelerator_destroy(LibEtudeGPUAccelerator* gpu_accel) {
    if (!gpu_accel || !gpu_accel->initialized) {
        return;
    }

    // GPU 메모리 풀 해제
    if (gpu_accel->gpu_memory_pool) {
        // GPU 메모리 풀 해제 로직 (백엔드별로 다름)
        gpu_accel->gpu_memory_pool = NULL;
    }

    // GPU 컨텍스트 해제
    _destroy_gpu_context(gpu_accel);

    // 구조체 초기화
    memset(gpu_accel, 0, sizeof(LibEtudeGPUAccelerator));
}

void* libetude_gpu_allocate_memory(LibEtudeGPUAccelerator* gpu_accel, size_t size) {
    if (!gpu_accel || !gpu_accel->initialized || size == 0) {
        return NULL;
    }

    // 메모리 제한 확인
    size_t limit = (size_t)gpu_accel->config.gpu_memory_limit_mb * 1024 * 1024;
    if (gpu_accel->allocated_memory + size > limit) {
        return NULL;
    }

    void* ptr = NULL;

    // 백엔드별 메모리 할당
    switch (gpu_accel->gpu_info.backend) {
        case LIBETUDE_GPU_CUDA:
            // CUDA 메모리 할당 (실제 구현에서는 cudaMalloc 사용)
            ptr = malloc(size); // 임시 구현
            break;

        case LIBETUDE_GPU_OPENCL:
            // OpenCL 메모리 할당 (실제 구현에서는 clCreateBuffer 사용)
            ptr = malloc(size); // 임시 구현
            break;

        case LIBETUDE_GPU_METAL:
            // Metal 메모리 할당 (실제 구현에서는 MTLDevice newBufferWithLength 사용)
            ptr = malloc(size); // 임시 구현
            break;

        default:
            return NULL;
    }

    if (ptr) {
        gpu_accel->allocated_memory += size;
        if (gpu_accel->allocated_memory > gpu_accel->peak_memory_usage) {
            gpu_accel->peak_memory_usage = gpu_accel->allocated_memory;
        }
    }

    return ptr;
}

void libetude_gpu_free_memory(LibEtudeGPUAccelerator* gpu_accel, void* ptr) {
    if (!gpu_accel || !gpu_accel->initialized || !ptr) {
        return;
    }

    // 백엔드별 메모리 해제
    switch (gpu_accel->gpu_info.backend) {
        case LIBETUDE_GPU_CUDA:
            // CUDA 메모리 해제 (실제 구현에서는 cudaFree 사용)
            free(ptr); // 임시 구현
            break;

        case LIBETUDE_GPU_OPENCL:
            // OpenCL 메모리 해제 (실제 구현에서는 clReleaseMemObject 사용)
            free(ptr); // 임시 구현
            break;

        case LIBETUDE_GPU_METAL:
            // Metal 메모리 해제 (자동 관리)
            free(ptr); // 임시 구현
            break;

        default:
            break;
    }

    // 메모리 사용량 업데이트 (실제로는 할당 시 크기를 추적해야 함)
    // 여기서는 간단한 구현으로 생략
}

LibEtudeErrorCode libetude_gpu_execute_kernel(LibEtudeGPUAccelerator* gpu_accel,
                                              const char* kernel_name,
                                              void** args,
                                              uint32_t grid_size,
                                              uint32_t block_size) {
    if (!gpu_accel || !gpu_accel->initialized || !kernel_name) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 커널 실행 시작 시간 기록
    uint64_t start_time = libetude_get_time_microseconds();

    LibEtudeErrorCode result = LIBETUDE_SUCCESS;

    // 백엔드별 커널 실행
    switch (gpu_accel->gpu_info.backend) {
        case LIBETUDE_GPU_CUDA:
            // CUDA 커널 실행 (실제 구현에서는 cuLaunchKernel 사용)
            // 임시 구현: 성공으로 처리
            break;

        case LIBETUDE_GPU_OPENCL:
            // OpenCL 커널 실행 (실제 구현에서는 clEnqueueNDRangeKernel 사용)
            // 임시 구현: 성공으로 처리
            break;

        case LIBETUDE_GPU_METAL:
            // Metal 커널 실행 (실제 구현에서는 MTLComputeCommandEncoder 사용)
            // 임시 구현: 성공으로 처리
            break;

        default:
            result = LIBETUDE_ERROR_NOT_IMPLEMENTED;
            break;
    }

    // 실행 시간 기록
    uint64_t end_time = libetude_get_time_microseconds();
    uint64_t duration = end_time - start_time;

    // 통계 업데이트
    gpu_accel->gpu_kernel_executions++;
    gpu_accel->avg_kernel_duration_us =
        (gpu_accel->avg_kernel_duration_us * (gpu_accel->gpu_kernel_executions - 1) + duration) /
        gpu_accel->gpu_kernel_executions;

    return result;
}// ====
========================================================================
// 오디오 백엔드 최적화 구현
// ============================================================================

LibEtudeErrorCode libetude_audio_backend_optimizer_init(LibEtudeAudioBackendOptimizer* audio_opt,
                                                        const LibEtudeHardwareInfo* hardware_info) {
    if (!audio_opt || !hardware_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(audio_opt, 0, sizeof(LibEtudeAudioBackendOptimizer));

    // 기본 설정 초기화
    audio_opt->config.buffer_size_frames = 256; // 기본 버퍼 크기
    audio_opt->config.num_buffers = 3;          // 트리플 버퍼링
    audio_opt->config.enable_exclusive_mode = false; // 기본적으로 비활성화
    audio_opt->config.enable_low_latency_mode = true;
    audio_opt->config.audio_thread_priority = 95; // 높은 우선순위
    audio_opt->config.enable_audio_thread_affinity = true;
    audio_opt->config.audio_cpu_affinity_mask = 1; // 첫 번째 코어 사용

    // 성능 등급에 따른 설정 조정
    if (hardware_info->performance_tier >= 4) {
        // 고성능 시스템: 더 작은 버퍼 크기로 저지연 달성
        audio_opt->config.buffer_size_frames = 128;
        audio_opt->config.enable_exclusive_mode = true;
    } else if (hardware_info->performance_tier <= 2) {
        // 저성능 시스템: 더 큰 버퍼 크기로 안정성 확보
        audio_opt->config.buffer_size_frames = 512;
        audio_opt->config.num_buffers = 4;
    }

    // 오디오 디바이스 초기화
    ETAudioFormat format = {
        .sample_rate = 48000,
        .bit_depth = 32,
        .num_channels = 2,
        .frame_size = 8, // 32-bit stereo
        .buffer_size = audio_opt->config.buffer_size_frames * 8
    };

    audio_opt->audio_device = et_audio_open_output_device(NULL, &format);
    if (!audio_opt->audio_device) {
        return LIBETUDE_ERROR_IO;
    }

    // 오디오 버퍼 초기화
    LibEtudeErrorCode result = _init_audio_buffers(audio_opt);
    if (result != LIBETUDE_SUCCESS) {
        libetude_audio_close_device(audio_opt->audio_device);
        return result;
    }

    // 오디오 콜백 설정
    et_audio_set_callback(audio_opt->audio_device, _audio_callback_wrapper, audio_opt);

    audio_opt->initialized = true;
    return LIBETUDE_SUCCESS;
}

void libetude_audio_backend_optimizer_destroy(LibEtudeAudioBackendOptimizer* audio_opt) {
    if (!audio_opt || !audio_opt->initialized) {
        return;
    }

    // 오디오 디바이스 정지 및 해제
    if (audio_opt->audio_device) {
        et_audio_stop(audio_opt->audio_device);
        et_audio_close_device(audio_opt->audio_device);
        audio_opt->audio_device = NULL;
    }

    // 오디오 버퍼 해제
    _destroy_audio_buffers(audio_opt);

    // 구조체 초기화
    memset(audio_opt, 0, sizeof(LibEtudeAudioBackendOptimizer));
}

LibEtudeErrorCode libetude_audio_set_low_latency_mode(LibEtudeAudioBackendOptimizer* audio_opt,
                                                      bool enable) {
    if (!audio_opt || !audio_opt->initialized) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    audio_opt->config.enable_low_latency_mode = enable;

    if (enable) {
        // 저지연 모드: 더 작은 버퍼 크기
        audio_opt->config.buffer_size_frames = 128;
        audio_opt->config.num_buffers = 2; // 더블 버퍼링
        audio_opt->config.audio_thread_priority = 99; // 최고 우선순위
    } else {
        // 일반 모드: 더 큰 버퍼 크기로 안정성 확보
        audio_opt->config.buffer_size_frames = 512;
        audio_opt->config.num_buffers = 3; // 트리플 버퍼링
        audio_opt->config.audio_thread_priority = 95;
    }

    return LIBETUDE_SUCCESS;
}

LibEtudeErrorCode libetude_audio_optimize_buffer_size(LibEtudeAudioBackendOptimizer* audio_opt,
                                                      uint32_t target_latency_ms) {
    if (!audio_opt || !audio_opt->initialized) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 목표 지연 시간에 따른 버퍼 크기 계산
    uint32_t sample_rate = 48000; // 기본 샘플링 레이트
    uint32_t target_frames = (sample_rate * target_latency_ms) / 1000;

    // 2의 거듭제곱으로 반올림 (효율성을 위해)
    uint32_t buffer_size = 64; // 최소 크기
    while (buffer_size < target_frames && buffer_size < 2048) {
        buffer_size *= 2;
    }

    audio_opt->config.buffer_size_frames = buffer_size;

    // 버퍼 수 조정
    if (target_latency_ms <= 10) {
        audio_opt->config.num_buffers = 2; // 매우 저지연
    } else if (target_latency_ms <= 20) {
        audio_opt->config.num_buffers = 3; // 저지연
    } else {
        audio_opt->config.num_buffers = 4; // 일반
    }

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 성능 모니터링 구현
// ============================================================================

void libetude_desktop_optimizer_update_stats(LibEtudeDesktopOptimizer* optimizer) {
    if (!optimizer || !optimizer->initialized) {
        return;
    }

    // CPU 사용률 계산 (간단한 구현)
    static uint64_t last_update_time = 0;
    uint64_t current_time = libetude_get_time_microseconds();

    if (last_update_time == 0) {
        last_update_time = current_time;
        return;
    }

    uint64_t time_delta = current_time - last_update_time;
    if (time_delta < 1000000) { // 1초 미만이면 업데이트하지 않음
        return;
    }

    // 멀티코어 통계 업데이트
    if (optimizer->multicore.scheduler) {
        // 작업 처리 통계 (실제 구현에서는 스케줄러에서 가져옴)
        optimizer->multicore.total_tasks_processed += 100; // 예시 값
        optimizer->multicore.avg_task_duration_us = 50;    // 예시 값
        optimizer->multicore.cpu_utilization = 0.75f;     // 예시 값
    }

    // GPU 통계 업데이트
    if (optimizer->gpu_accel.initialized) {
        optimizer->gpu_accel.gpu_utilization = 0.60f; // 예시 값
    }

    // 오디오 통계 업데이트
    if (optimizer->audio.initialized) {
        optimizer->audio.cpu_usage_audio_thread = 0.15f; // 예시 값
    }

    // 전체 통계 계산
    optimizer->overall_cpu_utilization =
        (optimizer->multicore.cpu_utilization + optimizer->audio.cpu_usage_audio_thread) / 2.0f;

    optimizer->overall_memory_utilization =
        (float)optimizer->hardware_info.memory.process_memory_usage /
        (float)optimizer->hardware_info.memory.total_physical;

    last_update_time = current_time;
}

void libetude_desktop_optimizer_print_stats(const LibEtudeDesktopOptimizer* optimizer) {
    if (!optimizer || !optimizer->initialized) {
        return;
    }

    printf("=== LibEtude Desktop Optimizer Statistics ===\n");
    printf("Hardware Performance Tier: %u/5\n", optimizer->hardware_info.performance_tier);
    printf("CPU: %s (%u cores)\n",
           optimizer->hardware_info.cpu.brand,
           optimizer->hardware_info.cpu.physical_cores);

    if (optimizer->hardware_info.gpu.available) {
        printf("GPU: %s (%s)\n",
               optimizer->hardware_info.gpu.name,
               optimizer->hardware_info.gpu.vendor);
    }

    printf("\n--- Multicore Optimization ---\n");
    printf("Worker Threads: %u\n", optimizer->multicore.config.worker_thread_count);
    printf("Tasks Processed: %llu\n", (unsigned long long)optimizer->multicore.total_tasks_processed);
    printf("Avg Task Duration: %llu μs\n", (unsigned long long)optimizer->multicore.avg_task_duration_us);
    printf("CPU Utilization: %.1f%%\n", optimizer->multicore.cpu_utilization * 100.0f);

    if (optimizer->gpu_accel.initialized) {
        printf("\n--- GPU Acceleration ---\n");
        printf("Backend: %s\n",
               (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_CUDA) ? "CUDA" :
               (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_OPENCL) ? "OpenCL" :
               (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_METAL) ? "Metal" : "Unknown");
        printf("Kernel Executions: %llu\n", (unsigned long long)optimizer->gpu_accel.gpu_kernel_executions);
        printf("Avg Kernel Duration: %llu μs\n", (unsigned long long)optimizer->gpu_accel.avg_kernel_duration_us);
        printf("GPU Utilization: %.1f%%\n", optimizer->gpu_accel.gpu_utilization * 100.0f);
        printf("Memory Usage: %.1f MB / %.1f MB\n",
               (float)optimizer->gpu_accel.allocated_memory / (1024 * 1024),
               (float)optimizer->gpu_accel.gpu_info.total_memory / (1024 * 1024));
    }

    if (optimizer->audio.initialized) {
        printf("\n--- Audio Backend Optimization ---\n");
        printf("Buffer Size: %u frames\n", optimizer->audio.config.buffer_size_frames);
        printf("Buffer Count: %u\n", optimizer->audio.config.num_buffers);
        printf("Low Latency Mode: %s\n", optimizer->audio.config.enable_low_latency_mode ? "Enabled" : "Disabled");
        printf("Audio Callbacks: %llu\n", (unsigned long long)optimizer->audio.audio_callbacks_processed);
        printf("Buffer Underruns: %llu\n", (unsigned long long)optimizer->audio.buffer_underruns);
        printf("Audio Thread CPU: %.1f%%\n", optimizer->audio.cpu_usage_audio_thread * 100.0f);
        printf("Total Latency: %llu μs\n", (unsigned long long)optimizer->audio.total_latency_us);
    }

    printf("\n--- Overall Performance ---\n");
    printf("Overall CPU Utilization: %.1f%%\n", optimizer->overall_cpu_utilization * 100.0f);
    printf("Overall Memory Utilization: %.1f%%\n", optimizer->overall_memory_utilization * 100.0f);
    printf("Total Inference Time: %llu μs\n", (unsigned long long)optimizer->total_inference_time_us);
    printf("Total Audio Processing Time: %llu μs\n", (unsigned long long)optimizer->total_audio_processing_time_us);
    printf("===============================================\n");
}// =====
=======================================================================
// 자동 최적화 구현
// ============================================================================

LibEtudeErrorCode libetude_desktop_optimizer_auto_optimize(LibEtudeDesktopOptimizer* optimizer) {
    if (!optimizer || !optimizer->initialized) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 성능 등급에 따른 자동 최적화
    uint32_t performance_tier = optimizer->hardware_info.performance_tier;

    if (performance_tier >= 4) {
        // 고성능 시스템: 최대 성능 모드
        optimizer->multicore.config.worker_thread_count = optimizer->hardware_info.cpu.physical_cores;
        optimizer->audio.config.buffer_size_frames = 128;
        optimizer->audio.config.enable_low_latency_mode = true;

        if (optimizer->gpu_accel.initialized) {
            optimizer->gpu_accel.config.gpu_utilization_target = 0.90f;
            optimizer->gpu_accel.config.enable_mixed_precision = true;
        }
    } else if (performance_tier >= 3) {
        // 중간 성능 시스템: 균형 모드
        optimizer->multicore.config.worker_thread_count = optimizer->hardware_info.cpu.physical_cores;
        optimizer->audio.config.buffer_size_frames = 256;
        optimizer->audio.config.enable_low_latency_mode = true;

        if (optimizer->gpu_accel.initialized) {
            optimizer->gpu_accel.config.gpu_utilization_target = 0.75f;
        }
    } else {
        // 저성능 시스템: 안정성 우선 모드
        optimizer->multicore.config.worker_thread_count =
            (optimizer->hardware_info.cpu.physical_cores > 2) ?
            optimizer->hardware_info.cpu.physical_cores - 1 :
            optimizer->hardware_info.cpu.physical_cores;
        optimizer->audio.config.buffer_size_frames = 512;
        optimizer->audio.config.enable_low_latency_mode = false;

        if (optimizer->gpu_accel.initialized) {
            optimizer->gpu_accel.config.gpu_utilization_target = 0.60f;
            optimizer->gpu_accel.config.enable_mixed_precision = false;
        }
    }

    return LIBETUDE_SUCCESS;
}

LibEtudeErrorCode libetude_desktop_optimizer_adaptive_tuning(LibEtudeDesktopOptimizer* optimizer,
                                                            float target_cpu_usage,
                                                            uint32_t target_latency_ms) {
    if (!optimizer || !optimizer->initialized) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 현재 성능 통계 업데이트
    libetude_desktop_optimizer_update_stats(optimizer);

    // CPU 사용률 조정
    if (optimizer->overall_cpu_utilization > target_cpu_usage + 0.1f) {
        // CPU 사용률이 너무 높음: 스레드 수 감소
        if (optimizer->multicore.config.worker_thread_count > 1) {
            optimizer->multicore.config.worker_thread_count--;
        }
    } else if (optimizer->overall_cpu_utilization < target_cpu_usage - 0.1f) {
        // CPU 사용률이 너무 낮음: 스레드 수 증가
        if (optimizer->multicore.config.worker_thread_count < optimizer->hardware_info.cpu.physical_cores) {
            optimizer->multicore.config.worker_thread_count++;
        }
    }

    // 지연 시간 조정
    if (optimizer->audio.total_latency_us > target_latency_ms * 1000) {
        // 지연 시간이 너무 높음: 버퍼 크기 감소
        if (optimizer->audio.config.buffer_size_frames > 64) {
            optimizer->audio.config.buffer_size_frames /= 2;
        }
    } else if (optimizer->audio.buffer_underruns > 0) {
        // 버퍼 언더런 발생: 버퍼 크기 증가
        if (optimizer->audio.config.buffer_size_frames < 1024) {
            optimizer->audio.config.buffer_size_frames *= 2;
        }
    }

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static LibEtudeErrorCode _init_multicore_threads(LibEtudeMulticoreOptimizer* multicore) {
    // 워커 스레드 배열 할당
    multicore->worker_threads = (pthread_t*)calloc(multicore->config.worker_thread_count, sizeof(pthread_t));
    if (!multicore->worker_threads) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    multicore->thread_active = (bool*)calloc(multicore->config.worker_thread_count, sizeof(bool));
    if (!multicore->thread_active) {
        free(multicore->worker_threads);
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 워커 스레드 생성
    for (uint32_t i = 0; i < multicore->config.worker_thread_count; i++) {
        if (pthread_create(&multicore->worker_threads[i], NULL, _worker_thread_function, multicore) != 0) {
            // 생성 실패 시 이미 생성된 스레드들 정리
            for (uint32_t j = 0; j < i; j++) {
                multicore->thread_active[j] = false;
                pthread_join(multicore->worker_threads[j], NULL);
            }
            free(multicore->worker_threads);
            free(multicore->thread_active);
            return LIBETUDE_ERROR_RUNTIME;
        }
        multicore->thread_active[i] = true;
        multicore->active_thread_count++;

        // 스레드 우선순위 설정
        _set_thread_priority(multicore->worker_threads[i], multicore->config.compute_thread_priority);

        // CPU 친화성 설정
        if (multicore->config.enable_cpu_affinity) {
            _set_thread_affinity(multicore->worker_threads[i], multicore->config.cpu_affinity_mask);
        }
    }

    return LIBETUDE_SUCCESS;
}

static void _destroy_multicore_threads(LibEtudeMulticoreOptimizer* multicore) {
    if (!multicore->worker_threads || !multicore->thread_active) {
        return;
    }

    // 모든 스레드를 비활성화하고 종료 대기
    for (uint32_t i = 0; i < multicore->config.worker_thread_count; i++) {
        if (multicore->thread_active[i]) {
            multicore->thread_active[i] = false;
            pthread_join(multicore->worker_threads[i], NULL);
        }
    }

    free(multicore->worker_threads);
    free(multicore->thread_active);
    multicore->worker_threads = NULL;
    multicore->thread_active = NULL;
    multicore->active_thread_count = 0;
}

static void* _worker_thread_function(void* arg) {
    LibEtudeMulticoreOptimizer* multicore = (LibEtudeMulticoreOptimizer*)arg;

    // 스레드 인덱스 찾기 (간단한 구현)
    pthread_t current_thread = pthread_self();
    uint32_t thread_index = 0;
    for (uint32_t i = 0; i < multicore->config.worker_thread_count; i++) {
        if (pthread_equal(multicore->worker_threads[i], current_thread)) {
            thread_index = i;
            break;
        }
    }

    // 워커 스레드 메인 루프
    while (multicore->thread_active[thread_index]) {
        // 작업 스케줄러에서 작업 가져와서 실행
        if (multicore->scheduler) {
            // 실제 구현에서는 스케줄러에서 작업을 가져와서 실행
            // 여기서는 간단한 대기 구현
            usleep(1000); // 1ms 대기
        } else {
            usleep(10000); // 10ms 대기
        }
    }

    return NULL;
}

static LibEtudeErrorCode _set_thread_priority(pthread_t thread, int priority) {
#ifdef _WIN32
    // Windows에서는 SetThreadPriority 사용
    HANDLE thread_handle = (HANDLE)thread;
    int win_priority = THREAD_PRIORITY_NORMAL;

    if (priority >= 90) {
        win_priority = THREAD_PRIORITY_TIME_CRITICAL;
    } else if (priority >= 70) {
        win_priority = THREAD_PRIORITY_HIGHEST;
    } else if (priority >= 50) {
        win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
    }

    if (!SetThreadPriority(thread_handle, win_priority)) {
        return LIBETUDE_ERROR_RUNTIME;
    }
#else
    // POSIX 시스템에서는 pthread_setschedparam 사용
    struct sched_param param;
    int policy = SCHED_OTHER;

    if (priority >= 90) {
        policy = SCHED_FIFO;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    } else if (priority >= 70) {
        policy = SCHED_RR;
        param.sched_priority = sched_get_priority_max(SCHED_RR) / 2;
    } else {
        param.sched_priority = 0;
    }

    if (pthread_setschedparam(thread, policy, &param) != 0) {
        return LIBETUDE_ERROR_RUNTIME;
    }
#endif

    return LIBETUDE_SUCCESS;
}

static LibEtudeErrorCode _set_thread_affinity(pthread_t thread, uint32_t affinity_mask) {
#ifdef _WIN32
    // Windows에서는 SetThreadAffinityMask 사용
    HANDLE thread_handle = (HANDLE)thread;
    if (!SetThreadAffinityMask(thread_handle, (DWORD_PTR)affinity_mask)) {
        return LIBETUDE_ERROR_RUNTIME;
    }
#elif defined(__linux__)
    // Linux에서는 pthread_setaffinity_np 사용
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    for (int i = 0; i < 32; i++) {
        if (affinity_mask & (1U << i)) {
            CPU_SET(i, &cpuset);
        }
    }

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return LIBETUDE_ERROR_RUNTIME;
    }
#elif defined(__APPLE__)
    // macOS에서는 thread_policy_set 사용
    thread_affinity_policy_data_t policy = { (integer_t)affinity_mask };
    if (thread_policy_set(pthread_mach_thread_np(thread),
                         THREAD_AFFINITY_POLICY,
                         (thread_policy_t)&policy,
                         THREAD_AFFINITY_POLICY_COUNT) != KERN_SUCCESS) {
        return LIBETUDE_ERROR_RUNTIME;
    }
#endif

    return LIBETUDE_SUCCESS;
}

static LibEtudeErrorCode _init_gpu_context(LibEtudeGPUAccelerator* gpu_accel) {
    // 백엔드별 GPU 컨텍스트 초기화
    switch (gpu_accel->gpu_info.backend) {
        case LIBETUDE_GPU_CUDA:
            // CUDA 컨텍스트 초기화 (실제 구현에서는 cuCtxCreate 사용)
            gpu_accel->gpu_context = (void*)0x1; // 임시 구현
            gpu_accel->command_queue = (void*)0x1; // 임시 구현
            break;

        case LIBETUDE_GPU_OPENCL:
            // OpenCL 컨텍스트 초기화 (실제 구현에서는 clCreateContext 사용)
            gpu_accel->gpu_context = (void*)0x1; // 임시 구현
            gpu_accel->command_queue = (void*)0x1; // 임시 구현
            break;

        case LIBETUDE_GPU_METAL:
            // Metal 컨텍스트 초기화 (실제 구현에서는 MTLCreateSystemDefaultDevice 사용)
            gpu_accel->gpu_context = (void*)0x1; // 임시 구현
            gpu_accel->command_queue = (void*)0x1; // 임시 구현
            break;

        default:
            return LIBETUDE_ERROR_NOT_IMPLEMENTED;
    }

    return LIBETUDE_SUCCESS;
}

static void _destroy_gpu_context(LibEtudeGPUAccelerator* gpu_accel) {
    // 백엔드별 GPU 컨텍스트 해제
    switch (gpu_accel->gpu_info.backend) {
        case LIBETUDE_GPU_CUDA:
            // CUDA 컨텍스트 해제 (실제 구현에서는 cuCtxDestroy 사용)
            break;

        case LIBETUDE_GPU_OPENCL:
            // OpenCL 컨텍스트 해제 (실제 구현에서는 clReleaseContext 사용)
            break;

        case LIBETUDE_GPU_METAL:
            // Metal 컨텍스트 해제 (자동 관리)
            break;

        default:
            break;
    }

    gpu_accel->gpu_context = NULL;
    gpu_accel->command_queue = NULL;
}

static LibEtudeErrorCode _create_gpu_memory_pool(LibEtudeGPUAccelerator* gpu_accel) {
    // GPU 메모리 풀 생성 (실제 구현에서는 백엔드별로 다름)
    size_t pool_size = (size_t)gpu_accel->config.gpu_memory_limit_mb * 1024 * 1024;

    // 임시 구현: 메모리 풀 포인터만 설정
    gpu_accel->gpu_memory_pool = (void*)0x1;

    return LIBETUDE_SUCCESS;
}

static LibEtudeErrorCode _init_audio_buffers(LibEtudeAudioBackendOptimizer* audio_opt) {
    // 오디오 버퍼 배열 할당
    audio_opt->audio_buffers = (float**)calloc(audio_opt->config.num_buffers, sizeof(float*));
    if (!audio_opt->audio_buffers) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 각 버퍼 할당
    size_t buffer_size = audio_opt->config.buffer_size_frames * 2; // 스테레오
    for (uint32_t i = 0; i < audio_opt->config.num_buffers; i++) {
        audio_opt->audio_buffers[i] = (float*)calloc(buffer_size, sizeof(float));
        if (!audio_opt->audio_buffers[i]) {
            // 할당 실패 시 이미 할당된 버퍼들 해제
            for (uint32_t j = 0; j < i; j++) {
                free(audio_opt->audio_buffers[j]);
            }
            free(audio_opt->audio_buffers);
            return LIBETUDE_ERROR_OUT_OF_MEMORY;
        }
    }

    audio_opt->frames_per_buffer = audio_opt->config.buffer_size_frames;
    audio_opt->current_buffer_index = 0;

    return LIBETUDE_SUCCESS;
}

static void _destroy_audio_buffers(LibEtudeAudioBackendOptimizer* audio_opt) {
    if (!audio_opt->audio_buffers) {
        return;
    }

    // 각 버퍼 해제
    for (uint32_t i = 0; i < audio_opt->config.num_buffers; i++) {
        if (audio_opt->audio_buffers[i]) {
            free(audio_opt->audio_buffers[i]);
        }
    }

    // 버퍼 배열 해제
    free(audio_opt->audio_buffers);
    audio_opt->audio_buffers = NULL;
}

static void _audio_callback_wrapper(float* buffer, int num_frames, void* user_data) {
    LibEtudeAudioBackendOptimizer* audio_opt = (LibEtudeAudioBackendOptimizer*)user_data;

    if (!audio_opt || !audio_opt->initialized) {
        // 오디오 버퍼를 0으로 채움 (무음)
        memset(buffer, 0, num_frames * 2 * sizeof(float));
        return;
    }

    uint64_t callback_start_time = libetude_get_time_microseconds();

    // 현재 버퍼에서 오디오 데이터 복사
    uint32_t buffer_index = audio_opt->current_buffer_index;
    if (audio_opt->audio_buffers && audio_opt->audio_buffers[buffer_index]) {
        size_t copy_size = num_frames * 2 * sizeof(float); // 스테레오
        memcpy(buffer, audio_opt->audio_buffers[buffer_index], copy_size);
    } else {
        // 버퍼가 없으면 무음 출력
        memset(buffer, 0, num_frames * 2 * sizeof(float));
        audio_opt->buffer_underruns++;
    }

    // 다음 버퍼로 이동
    audio_opt->current_buffer_index = (audio_opt->current_buffer_index + 1) % audio_opt->config.num_buffers;

    // 통계 업데이트
    audio_opt->audio_callbacks_processed++;

    uint64_t callback_end_time = libetude_get_time_microseconds();
    uint64_t callback_duration = callback_end_time - callback_start_time;

    // 지연 시간 계산
    audio_opt->processing_latency_us = callback_duration;
    audio_opt->buffer_latency_us = (uint64_t)(num_frames * 1000000.0 / 48000.0); // 48kHz 기준
    audio_opt->total_latency_us = audio_opt->processing_latency_us + audio_opt->buffer_latency_us;
}LibE
tudeErrorCode libetude_desktop_optimizer_stats_to_json(const LibEtudeDesktopOptimizer* optimizer,
                                                           char* buffer,
                                                           size_t buffer_size) {
    if (!optimizer || !optimizer->initialized || !buffer || buffer_size == 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"hardware\": {\n"
        "    \"performance_tier\": %u,\n"
        "    \"cpu_brand\": \"%s\",\n"
        "    \"cpu_cores\": %u,\n"
        "    \"gpu_available\": %s,\n"
        "    \"gpu_name\": \"%s\"\n"
        "  },\n"
        "  \"multicore\": {\n"
        "    \"worker_threads\": %u,\n"
        "    \"tasks_processed\": %llu,\n"
        "    \"avg_task_duration_us\": %llu,\n"
        "    \"cpu_utilization\": %.3f\n"
        "  },\n"
        "  \"gpu\": {\n"
        "    \"initialized\": %s,\n"
        "    \"kernel_executions\": %llu,\n"
        "    \"avg_kernel_duration_us\": %llu,\n"
        "    \"gpu_utilization\": %.3f,\n"
        "    \"memory_usage_mb\": %.1f\n"
        "  },\n"
        "  \"audio\": {\n"
        "    \"buffer_size_frames\": %u,\n"
        "    \"buffer_count\": %u,\n"
        "    \"low_latency_mode\": %s,\n"
        "    \"callbacks_processed\": %llu,\n"
        "    \"buffer_underruns\": %llu,\n"
        "    \"total_latency_us\": %llu\n"
        "  },\n"
        "  \"overall\": {\n"
        "    \"cpu_utilization\": %.3f,\n"
        "    \"memory_utilization\": %.3f,\n"
        "    \"total_inference_time_us\": %llu,\n"
        "    \"total_audio_processing_time_us\": %llu\n"
        "  }\n"
        "}",
        optimizer->hardware_info.performance_tier,
        optimizer->hardware_info.cpu.brand,
        optimizer->hardware_info.cpu.physical_cores,
        optimizer->hardware_info.gpu.available ? "true" : "false",
        optimizer->hardware_info.gpu.available ? optimizer->hardware_info.gpu.name : "N/A",
        optimizer->multicore.config.worker_thread_count,
        (unsigned long long)optimizer->multicore.total_tasks_processed,
        (unsigned long long)optimizer->multicore.avg_task_duration_us,
        optimizer->multicore.cpu_utilization,
        optimizer->gpu_accel.initialized ? "true" : "false",
        (unsigned long long)optimizer->gpu_accel.gpu_kernel_executions,
        (unsigned long long)optimizer->gpu_accel.avg_kernel_duration_us,
        optimizer->gpu_accel.gpu_utilization,
        (float)optimizer->gpu_accel.allocated_memory / (1024 * 1024),
        optimizer->audio.config.buffer_size_frames,
        optimizer->audio.config.num_buffers,
        optimizer->audio.config.enable_low_latency_mode ? "true" : "false",
        (unsigned long long)optimizer->audio.audio_callbacks_processed,
        (unsigned long long)optimizer->audio.buffer_underruns,
        (unsigned long long)optimizer->audio.total_latency_us,
        optimizer->overall_cpu_utilization,
        optimizer->overall_memory_utilization,
        (unsigned long long)optimizer->total_inference_time_us,
        (unsigned long long)optimizer->total_audio_processing_time_us
    );

    if (written >= (int)buffer_size) {
        return LIBETUDE_ERROR_BUFFER_TOO_SMALL;
    }

    return LIBETUDE_SUCCESS;
}

// 시간 측정 함수 (임시 구현)
uint64_t libetude_get_time_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}