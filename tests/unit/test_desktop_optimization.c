/**
 * @file test_desktop_optimization.c
 * @brief 데스크톱 최적화 기능 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "libetude/desktop_optimization.h"
#include "libetude/hardware.h"
#include "libetude/error.h"

// ============================================================================
// 테스트 헬퍼 함수
// ============================================================================

static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// ============================================================================
// 멀티코어 최적화 테스트
// ============================================================================

static bool test_multicore_optimizer_init() {
    print_test_header("Multicore Optimizer Initialization Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to detect hardware: %d\n", result);
        return false;
    }

    LibEtudeMulticoreOptimizer multicore;
    result = libetude_multicore_optimizer_init(&multicore, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to initialize multicore optimizer: %d\n", result);
        return false;
    }

    // 기본 설정 확인
    assert(multicore.config.worker_thread_count > 0);
    assert(multicore.config.worker_thread_count <= hardware_info.cpu.physical_cores);
    assert(multicore.config.audio_thread_priority > multicore.config.compute_thread_priority);
    assert(multicore.scheduler != NULL);

    printf("Worker threads: %u\n", multicore.config.worker_thread_count);
    printf("Audio thread priority: %u\n", multicore.config.audio_thread_priority);
    printf("Compute thread priority: %u\n", multicore.config.compute_thread_priority);
    printf("CPU affinity enabled: %s\n", multicore.config.enable_cpu_affinity ? "Yes" : "No");

    libetude_multicore_optimizer_destroy(&multicore);
    return true;
}

static bool test_multicore_auto_configure() {
    print_test_header("Multicore Auto Configuration Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    LibEtudeMulticoreOptimizer multicore;
    result = libetude_multicore_optimizer_init(&multicore, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 자동 설정 테스트
    result = libetude_multicore_auto_configure(&multicore, &hardware_info.cpu);
    if (result != LIBETUDE_SUCCESS) {
        libetude_multicore_optimizer_destroy(&multicore);
        return false;
    }

    // 설정 검증
    assert(multicore.config.worker_thread_count > 0);
    assert(multicore.config.cpu_affinity_mask != 0);

    printf("Configured worker threads: %u\n", multicore.config.worker_thread_count);
    printf("CPU affinity mask: 0x%08X\n", multicore.config.cpu_affinity_mask);
    printf("NUMA optimization: %s\n", multicore.config.enable_numa_optimization ? "Enabled" : "Disabled");

    libetude_multicore_optimizer_destroy(&multicore);
    return true;
}

static bool test_cpu_affinity_setting() {
    print_test_header("CPU Affinity Setting Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    LibEtudeMulticoreOptimizer multicore;
    result = libetude_multicore_optimizer_init(&multicore, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // CPU 친화성 설정 테스트
    uint32_t test_affinity = 0x0F; // 첫 4개 코어
    result = libetude_multicore_set_cpu_affinity(&multicore, test_affinity);
    if (result != LIBETUDE_SUCCESS) {
        libetude_multicore_optimizer_destroy(&multicore);
        return false;
    }

    assert(multicore.config.cpu_affinity_mask == test_affinity);
    printf("CPU affinity set to: 0x%08X\n", multicore.config.cpu_affinity_mask);

    libetude_multicore_optimizer_destroy(&multicore);
    return true;
}

// ============================================================================
// GPU 가속 테스트
// ============================================================================

static bool test_gpu_accelerator_init() {
    print_test_header("GPU Accelerator Initialization Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    if (!hardware_info.gpu.available) {
        printf("GPU not available, skipping GPU tests\n");
        return true;
    }

    LibEtudeGPUAccelerator gpu_accel;
    result = libetude_gpu_accelerator_init(&gpu_accel, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to initialize GPU accelerator: %d\n", result);
        return false;
    }

    // GPU 설정 확인
    assert(gpu_accel.initialized);
    assert(gpu_accel.config.gpu_memory_limit_mb > 0);
    assert(gpu_accel.config.gpu_utilization_target > 0.0f);
    assert(gpu_accel.config.gpu_utilization_target <= 1.0f);

    printf("GPU Backend: %d\n", gpu_accel.gpu_info.backend);
    printf("GPU Name: %s\n", gpu_accel.gpu_info.name);
    printf("Memory Limit: %u MB\n", gpu_accel.config.gpu_memory_limit_mb);
    printf("Utilization Target: %.2f\n", gpu_accel.config.gpu_utilization_target);
    printf("Mixed Precision: %s\n", gpu_accel.config.enable_mixed_precision ? "Enabled" : "Disabled");

    libetude_gpu_accelerator_destroy(&gpu_accel);
    return true;
}

static bool test_gpu_memory_allocation() {
    print_test_header("GPU Memory Allocation Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS || !hardware_info.gpu.available) {
        printf("GPU not available, skipping test\n");
        return true;
    }

    LibEtudeGPUAccelerator gpu_accel;
    result = libetude_gpu_accelerator_init(&gpu_accel, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 메모리 할당 테스트
    size_t test_size = 1024 * 1024; // 1MB
    void* gpu_ptr = libetude_gpu_allocate_memory(&gpu_accel, test_size);
    if (!gpu_ptr) {
        printf("Failed to allocate GPU memory\n");
        libetude_gpu_accelerator_destroy(&gpu_accel);
        return false;
    }

    printf("Allocated %zu bytes of GPU memory\n", test_size);
    printf("Total allocated: %zu bytes\n", gpu_accel.allocated_memory);

    // 메모리 해제 테스트
    libetude_gpu_free_memory(&gpu_accel, gpu_ptr);
    printf("GPU memory freed\n");

    libetude_gpu_accelerator_destroy(&gpu_accel);
    return true;
}

static bool test_gpu_kernel_execution() {
    print_test_header("GPU Kernel Execution Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS || !hardware_info.gpu.available) {
        printf("GPU not available, skipping test\n");
        return true;
    }

    LibEtudeGPUAccelerator gpu_accel;
    result = libetude_gpu_accelerator_init(&gpu_accel, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 커널 실행 테스트
    void* args[] = { NULL, NULL };
    result = libetude_gpu_execute_kernel(&gpu_accel, "test_kernel", args, 64, 256);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to execute GPU kernel: %d\n", result);
        libetude_gpu_accelerator_destroy(&gpu_accel);
        return false;
    }

    printf("GPU kernel executed successfully\n");
    printf("Kernel executions: %llu\n", (unsigned long long)gpu_accel.gpu_kernel_executions);
    printf("Avg kernel duration: %llu μs\n", (unsigned long long)gpu_accel.avg_kernel_duration_us);

    libetude_gpu_accelerator_destroy(&gpu_accel);
    return true;
}

// ============================================================================
// 오디오 백엔드 최적화 테스트
// ============================================================================

static bool test_audio_backend_optimizer_init() {
    print_test_header("Audio Backend Optimizer Initialization Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    LibEtudeAudioBackendOptimizer audio_opt;
    result = libetude_audio_backend_optimizer_init(&audio_opt, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to initialize audio backend optimizer: %d\n", result);
        return false;
    }

    // 오디오 설정 확인
    assert(audio_opt.initialized);
    assert(audio_opt.config.buffer_size_frames > 0);
    assert(audio_opt.config.num_buffers > 0);
    assert(audio_opt.config.audio_thread_priority > 0);

    printf("Buffer size: %u frames\n", audio_opt.config.buffer_size_frames);
    printf("Number of buffers: %u\n", audio_opt.config.num_buffers);
    printf("Low latency mode: %s\n", audio_opt.config.enable_low_latency_mode ? "Enabled" : "Disabled");
    printf("Exclusive mode: %s\n", audio_opt.config.enable_exclusive_mode ? "Enabled" : "Disabled");
    printf("Audio thread priority: %u\n", audio_opt.config.audio_thread_priority);

    libetude_audio_backend_optimizer_destroy(&audio_opt);
    return true;
}

static bool test_audio_low_latency_mode() {
    print_test_header("Audio Low Latency Mode Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    LibEtudeAudioBackendOptimizer audio_opt;
    result = libetude_audio_backend_optimizer_init(&audio_opt, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 저지연 모드 활성화 테스트
    result = libetude_audio_set_low_latency_mode(&audio_opt, true);
    if (result != LIBETUDE_SUCCESS) {
        libetude_audio_backend_optimizer_destroy(&audio_opt);
        return false;
    }

    assert(audio_opt.config.enable_low_latency_mode);
    printf("Low latency mode enabled, buffer size: %u frames\n", audio_opt.config.buffer_size_frames);

    // 저지연 모드 비활성화 테스트
    result = libetude_audio_set_low_latency_mode(&audio_opt, false);
    if (result != LIBETUDE_SUCCESS) {
        libetude_audio_backend_optimizer_destroy(&audio_opt);
        return false;
    }

    assert(!audio_opt.config.enable_low_latency_mode);
    printf("Low latency mode disabled, buffer size: %u frames\n", audio_opt.config.buffer_size_frames);

    libetude_audio_backend_optimizer_destroy(&audio_opt);
    return true;
}

static bool test_audio_buffer_optimization() {
    print_test_header("Audio Buffer Optimization Test");

    LibEtudeHardwareInfo hardware_info;
    LibEtudeErrorCode result = libetude_hardware_detect(&hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    LibEtudeAudioBackendOptimizer audio_opt;
    result = libetude_audio_backend_optimizer_init(&audio_opt, &hardware_info);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 다양한 목표 지연 시간으로 버퍼 최적화 테스트
    uint32_t target_latencies[] = { 5, 10, 20, 50 };
    size_t num_targets = sizeof(target_latencies) / sizeof(target_latencies[0]);

    for (size_t i = 0; i < num_targets; i++) {
        result = libetude_audio_optimize_buffer_size(&audio_opt, target_latencies[i]);
        if (result != LIBETUDE_SUCCESS) {
            libetude_audio_backend_optimizer_destroy(&audio_opt);
            return false;
        }

        printf("Target latency: %u ms, Buffer size: %u frames, Buffers: %u\n",
               target_latencies[i],
               audio_opt.config.buffer_size_frames,
               audio_opt.config.num_buffers);
    }

    libetude_audio_backend_optimizer_destroy(&audio_opt);
    return true;
}

// ============================================================================
// 통합 데스크톱 최적화 테스트
// ============================================================================

static bool test_desktop_optimizer_init() {
    print_test_header("Desktop Optimizer Initialization Test");

    LibEtudeDesktopOptimizer optimizer;
    LibEtudeErrorCode result = libetude_desktop_optimizer_init(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        printf("Failed to initialize desktop optimizer: %d\n", result);
        return false;
    }

    // 초기화 상태 확인
    assert(optimizer.initialized);
    assert(optimizer.hardware_info.initialized);

    printf("Hardware performance tier: %u/5\n", optimizer.hardware_info.performance_tier);
    printf("CPU: %s (%u cores)\n",
           optimizer.hardware_info.cpu.brand,
           optimizer.hardware_info.cpu.physical_cores);

    if (optimizer.hardware_info.gpu.available) {
        printf("GPU: %s (%s)\n",
               optimizer.hardware_info.gpu.name,
               optimizer.hardware_info.gpu.vendor);
    } else {
        printf("GPU: Not available\n");
    }

    libetude_desktop_optimizer_destroy(&optimizer);
    return true;
}

static bool test_desktop_optimizer_auto_optimize() {
    print_test_header("Desktop Optimizer Auto Optimization Test");

    LibEtudeDesktopOptimizer optimizer;
    LibEtudeErrorCode result = libetude_desktop_optimizer_init(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 자동 최적화 테스트
    result = libetude_desktop_optimizer_auto_optimize(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        libetude_desktop_optimizer_destroy(&optimizer);
        return false;
    }

    // 성능 등급에 따른 설정 확인
    uint32_t performance_tier = optimizer.hardware_info.performance_tier;
    printf("Performance tier: %u\n", performance_tier);
    printf("Worker threads: %u\n", optimizer.multicore.config.worker_thread_count);
    printf("Audio buffer size: %u frames\n", optimizer.audio.config.buffer_size_frames);
    printf("Low latency mode: %s\n", optimizer.audio.config.enable_low_latency_mode ? "Enabled" : "Disabled");

    if (optimizer.gpu_accel.initialized) {
        printf("GPU utilization target: %.2f\n", optimizer.gpu_accel.config.gpu_utilization_target);
        printf("Mixed precision: %s\n", optimizer.gpu_accel.config.enable_mixed_precision ? "Enabled" : "Disabled");
    }

    libetude_desktop_optimizer_destroy(&optimizer);
    return true;
}

static bool test_desktop_optimizer_adaptive_tuning() {
    print_test_header("Desktop Optimizer Adaptive Tuning Test");

    LibEtudeDesktopOptimizer optimizer;
    LibEtudeErrorCode result = libetude_desktop_optimizer_init(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 적응형 튜닝 테스트
    float target_cpu_usage = 0.70f; // 70% CPU 사용률 목표
    uint32_t target_latency_ms = 15; // 15ms 지연 시간 목표

    result = libetude_desktop_optimizer_adaptive_tuning(&optimizer, target_cpu_usage, target_latency_ms);
    if (result != LIBETUDE_SUCCESS) {
        libetude_desktop_optimizer_destroy(&optimizer);
        return false;
    }

    printf("Adaptive tuning completed\n");
    printf("Target CPU usage: %.1f%%\n", target_cpu_usage * 100.0f);
    printf("Target latency: %u ms\n", target_latency_ms);
    printf("Current worker threads: %u\n", optimizer.multicore.config.worker_thread_count);
    printf("Current buffer size: %u frames\n", optimizer.audio.config.buffer_size_frames);

    libetude_desktop_optimizer_destroy(&optimizer);
    return true;
}

static bool test_desktop_optimizer_stats() {
    print_test_header("Desktop Optimizer Statistics Test");

    LibEtudeDesktopOptimizer optimizer;
    LibEtudeErrorCode result = libetude_desktop_optimizer_init(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        return false;
    }

    // 통계 업데이트 및 출력 테스트
    libetude_desktop_optimizer_update_stats(&optimizer);
    libetude_desktop_optimizer_print_stats(&optimizer);

    // JSON 형태 통계 출력 테스트
    char json_buffer[4096];
    result = libetude_desktop_optimizer_stats_to_json(&optimizer, json_buffer, sizeof(json_buffer));
    if (result != LIBETUDE_SUCCESS) {
        libetude_desktop_optimizer_destroy(&optimizer);
        return false;
    }

    printf("\nJSON Statistics:\n%s\n", json_buffer);

    libetude_desktop_optimizer_destroy(&optimizer);
    return true;
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main() {
    printf("LibEtude Desktop Optimization Tests\n");
    printf("====================================\n");

    bool all_passed = true;

    // 멀티코어 최적화 테스트
    all_passed &= test_multicore_optimizer_init();
    print_test_result("Multicore Optimizer Init", all_passed);

    all_passed &= test_multicore_auto_configure();
    print_test_result("Multicore Auto Configure", all_passed);

    all_passed &= test_cpu_affinity_setting();
    print_test_result("CPU Affinity Setting", all_passed);

    // GPU 가속 테스트
    all_passed &= test_gpu_accelerator_init();
    print_test_result("GPU Accelerator Init", all_passed);

    all_passed &= test_gpu_memory_allocation();
    print_test_result("GPU Memory Allocation", all_passed);

    all_passed &= test_gpu_kernel_execution();
    print_test_result("GPU Kernel Execution", all_passed);

    // 오디오 백엔드 최적화 테스트
    all_passed &= test_audio_backend_optimizer_init();
    print_test_result("Audio Backend Optimizer Init", all_passed);

    all_passed &= test_audio_low_latency_mode();
    print_test_result("Audio Low Latency Mode", all_passed);

    all_passed &= test_audio_buffer_optimization();
    print_test_result("Audio Buffer Optimization", all_passed);

    // 통합 데스크톱 최적화 테스트
    all_passed &= test_desktop_optimizer_init();
    print_test_result("Desktop Optimizer Init", all_passed);

    all_passed &= test_desktop_optimizer_auto_optimize();
    print_test_result("Desktop Optimizer Auto Optimize", all_passed);

    all_passed &= test_desktop_optimizer_adaptive_tuning();
    print_test_result("Desktop Optimizer Adaptive Tuning", all_passed);

    all_passed &= test_desktop_optimizer_stats();
    print_test_result("Desktop Optimizer Stats", all_passed);

    printf("\n====================================\n");
    printf("Overall Result: %s\n", all_passed ? "PASS" : "FAIL");

    return all_passed ? 0 : 1;
}