/**
 * @file main.c
 * @brief 데스크톱 최적화 기능 시연 예제
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libetude/desktop_optimization.h"
#include "libetude/hardware.h"
#include "libetude/error.h"

// ============================================================================
// 헬퍼 함수
// ============================================================================

static void print_separator() {
    printf("================================================================\n");
}

static void print_header(const char* title) {
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

static void print_hardware_info(const LibEtudeHardwareInfo* info) {
    print_header("하드웨어 정보");

    printf("플랫폼: %s\n", info->platform_name);
    printf("OS 버전: %s\n", info->os_version);
    printf("성능 등급: %u/5\n", info->performance_tier);

    printf("\n--- CPU 정보 ---\n");
    printf("브랜드: %s\n", info->cpu.brand);
    printf("제조사: %s\n", info->cpu.vendor);
    printf("물리 코어: %u개\n", info->cpu.physical_cores);
    printf("논리 코어: %u개\n", info->cpu.logical_cores);
    printf("기본 주파수: %u MHz\n", info->cpu.base_frequency_mhz);
    printf("최대 주파수: %u MHz\n", info->cpu.max_frequency_mhz);
    printf("L1 캐시: %u KB\n", info->cpu.l1_cache_size);
    printf("L2 캐시: %u KB\n", info->cpu.l2_cache_size);
    printf("L3 캐시: %u KB\n", info->cpu.l3_cache_size);

    // SIMD 기능 출력
    printf("SIMD 지원: ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSE) printf("SSE ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSE2) printf("SSE2 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSE3) printf("SSE3 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSSE3) printf("SSSE3 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSE4_1) printf("SSE4.1 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_SSE4_2) printf("SSE4.2 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_AVX) printf("AVX ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_AVX2) printf("AVX2 ");
    if (info->cpu.simd_features & LIBETUDE_SIMD_NEON) printf("NEON ");
    printf("\n");

    printf("\n--- GPU 정보 ---\n");
    if (info->gpu.available) {
        printf("이름: %s\n", info->gpu.name);
        printf("제조사: %s\n", info->gpu.vendor);
        printf("백엔드: %s\n",
               (info->gpu.backend == LIBETUDE_GPU_CUDA) ? "CUDA" :
               (info->gpu.backend == LIBETUDE_GPU_OPENCL) ? "OpenCL" :
               (info->gpu.backend == LIBETUDE_GPU_METAL) ? "Metal" : "Unknown");
        printf("총 메모리: %.1f GB\n", (float)info->gpu.total_memory / (1024 * 1024 * 1024));
        printf("사용 가능 메모리: %.1f GB\n", (float)info->gpu.available_memory / (1024 * 1024 * 1024));
        printf("컴퓨트 유닛: %u개\n", info->gpu.compute_units);
        printf("코어 클럭: %u MHz\n", info->gpu.core_clock_mhz);
        printf("메모리 클럭: %u MHz\n", info->gpu.memory_clock_mhz);
    } else {
        printf("GPU 사용 불가\n");
    }

    printf("\n--- 메모리 정보 ---\n");
    printf("총 물리 메모리: %.1f GB\n", (float)info->memory.total_physical / (1024 * 1024 * 1024));
    printf("사용 가능 메모리: %.1f GB\n", (float)info->memory.available_physical / (1024 * 1024 * 1024));
    printf("메모리 대역폭: %u GB/s\n", info->memory.memory_bandwidth_gbps);
    printf("페이지 크기: %u bytes\n", info->memory.page_size);
    printf("메모리 제약 상태: %s\n", info->memory.memory_constrained ? "예" : "아니오");
    printf("권장 메모리 풀 크기: %.1f MB\n", (float)info->memory.recommended_pool_size / (1024 * 1024));
}

static void demonstrate_multicore_optimization(LibEtudeDesktopOptimizer* optimizer) {
    print_header("멀티코어 최적화 시연");

    printf("현재 설정:\n");
    printf("- 워커 스레드 수: %u\n", optimizer->multicore.config.worker_thread_count);
    printf("- 오디오 스레드 우선순위: %u\n", optimizer->multicore.config.audio_thread_priority);
    printf("- 연산 스레드 우선순위: %u\n", optimizer->multicore.config.compute_thread_priority);
    printf("- CPU 친화성 활성화: %s\n", optimizer->multicore.config.enable_cpu_affinity ? "예" : "아니오");
    printf("- CPU 친화성 마스크: 0x%08X\n", optimizer->multicore.config.cpu_affinity_mask);
    printf("- NUMA 최적화: %s\n", optimizer->multicore.config.enable_numa_optimization ? "예" : "아니오");

    printf("\n다양한 CPU 친화성 설정 테스트...\n");

    // 첫 4개 코어만 사용
    uint32_t affinity_4cores = 0x0F;
    LibEtudeErrorCode result = libetude_multicore_set_cpu_affinity(&optimizer->multicore, affinity_4cores);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 첫 4개 코어 친화성 설정 성공 (0x%08X)\n", affinity_4cores);
    } else {
        printf("✗ 첫 4개 코어 친화성 설정 실패\n");
    }

    // 짝수 코어만 사용
    uint32_t affinity_even = 0x55555555;
    result = libetude_multicore_set_cpu_affinity(&optimizer->multicore, affinity_even);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 짝수 코어 친화성 설정 성공 (0x%08X)\n", affinity_even);
    } else {
        printf("✗ 짝수 코어 친화성 설정 실패\n");
    }

    // 원래 설정으로 복원
    result = libetude_multicore_auto_configure(&optimizer->multicore, &optimizer->hardware_info.cpu);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 자동 설정으로 복원 완료\n");
    }
}

static void demonstrate_gpu_acceleration(LibEtudeDesktopOptimizer* optimizer) {
    print_header("GPU 가속 시연");

    if (!optimizer->gpu_accel.initialized) {
        printf("GPU 가속을 사용할 수 없습니다.\n");
        return;
    }

    printf("GPU 가속 설정:\n");
    printf("- 백엔드: %s\n",
           (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_CUDA) ? "CUDA" :
           (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_OPENCL) ? "OpenCL" :
           (optimizer->gpu_accel.gpu_info.backend == LIBETUDE_GPU_METAL) ? "Metal" : "Unknown");
    printf("- 혼합 정밀도: %s\n", optimizer->gpu_accel.config.enable_mixed_precision ? "활성화" : "비활성화");
    printf("- 텐서 코어: %s\n", optimizer->gpu_accel.config.enable_tensor_cores ? "활성화" : "비활성화");
    printf("- 메모리 제한: %u MB\n", optimizer->gpu_accel.config.gpu_memory_limit_mb);
    printf("- 목표 사용률: %.1f%%\n", optimizer->gpu_accel.config.gpu_utilization_target * 100.0f);

    printf("\nGPU 메모리 할당 테스트...\n");

    // 다양한 크기의 메모리 할당 테스트
    size_t test_sizes[] = { 1024, 1024*1024, 16*1024*1024, 64*1024*1024 };
    const char* size_names[] = { "1 KB", "1 MB", "16 MB", "64 MB" };
    size_t num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);

    void* gpu_ptrs[4] = { NULL };

    for (size_t i = 0; i < num_tests; i++) {
        gpu_ptrs[i] = libetude_gpu_allocate_memory(&optimizer->gpu_accel, test_sizes[i]);
        if (gpu_ptrs[i]) {
            printf("✓ %s GPU 메모리 할당 성공\n", size_names[i]);
        } else {
            printf("✗ %s GPU 메모리 할당 실패\n", size_names[i]);
        }
    }

    printf("현재 할당된 GPU 메모리: %.1f MB\n",
           (float)optimizer->gpu_accel.allocated_memory / (1024 * 1024));

    // GPU 커널 실행 테스트
    printf("\nGPU 커널 실행 테스트...\n");
    void* kernel_args[] = { gpu_ptrs[0], gpu_ptrs[1] };
    LibEtudeErrorCode result = libetude_gpu_execute_kernel(&optimizer->gpu_accel,
                                                          "vector_add",
                                                          kernel_args,
                                                          256, 64);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ GPU 커널 실행 성공\n");
    } else {
        printf("✗ GPU 커널 실행 실패: %d\n", result);
    }

    // 메모리 해제
    for (size_t i = 0; i < num_tests; i++) {
        if (gpu_ptrs[i]) {
            libetude_gpu_free_memory(&optimizer->gpu_accel, gpu_ptrs[i]);
        }
    }
    printf("GPU 메모리 해제 완료\n");
}

static void demonstrate_audio_optimization(LibEtudeDesktopOptimizer* optimizer) {
    print_header("오디오 백엔드 최적화 시연");

    if (!optimizer->audio.initialized) {
        printf("오디오 백엔드 최적화를 사용할 수 없습니다.\n");
        return;
    }

    printf("현재 오디오 설정:\n");
    printf("- 버퍼 크기: %u frames\n", optimizer->audio.config.buffer_size_frames);
    printf("- 버퍼 개수: %u\n", optimizer->audio.config.num_buffers);
    printf("- 저지연 모드: %s\n", optimizer->audio.config.enable_low_latency_mode ? "활성화" : "비활성화");
    printf("- 독점 모드: %s\n", optimizer->audio.config.enable_exclusive_mode ? "활성화" : "비활성화");
    printf("- 오디오 스레드 우선순위: %u\n", optimizer->audio.config.audio_thread_priority);
    printf("- 오디오 스레드 CPU 친화성: %s\n", optimizer->audio.config.enable_audio_thread_affinity ? "활성화" : "비활성화");

    // 현재 지연 시간 계산
    uint32_t sample_rate = 48000;
    float buffer_latency_ms = (float)optimizer->audio.config.buffer_size_frames * 1000.0f / sample_rate;
    float total_latency_ms = buffer_latency_ms * optimizer->audio.config.num_buffers;

    printf("- 예상 버퍼 지연 시간: %.2f ms\n", buffer_latency_ms);
    printf("- 예상 총 지연 시간: %.2f ms\n", total_latency_ms);

    printf("\n저지연 모드 테스트...\n");

    // 저지연 모드 활성화
    LibEtudeErrorCode result = libetude_audio_set_low_latency_mode(&optimizer->audio, true);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 저지연 모드 활성화\n");
        printf("  - 새 버퍼 크기: %u frames\n", optimizer->audio.config.buffer_size_frames);
        printf("  - 새 버퍼 개수: %u\n", optimizer->audio.config.num_buffers);

        float new_buffer_latency_ms = (float)optimizer->audio.config.buffer_size_frames * 1000.0f / sample_rate;
        float new_total_latency_ms = new_buffer_latency_ms * optimizer->audio.config.num_buffers;
        printf("  - 새 예상 지연 시간: %.2f ms\n", new_total_latency_ms);
    } else {
        printf("✗ 저지연 모드 활성화 실패\n");
    }

    printf("\n다양한 목표 지연 시간으로 버퍼 최적화 테스트...\n");

    uint32_t target_latencies[] = { 5, 10, 20, 50 };
    size_t num_targets = sizeof(target_latencies) / sizeof(target_latencies[0]);

    for (size_t i = 0; i < num_targets; i++) {
        result = libetude_audio_optimize_buffer_size(&optimizer->audio, target_latencies[i]);
        if (result == LIBETUDE_SUCCESS) {
            float actual_latency_ms = (float)optimizer->audio.config.buffer_size_frames *
                                     optimizer->audio.config.num_buffers * 1000.0f / sample_rate;
            printf("✓ 목표 %u ms → 실제 %.2f ms (버퍼: %u frames × %u)\n",
                   target_latencies[i], actual_latency_ms,
                   optimizer->audio.config.buffer_size_frames,
                   optimizer->audio.config.num_buffers);
        } else {
            printf("✗ 목표 %u ms 최적화 실패\n", target_latencies[i]);
        }
    }
}

static void demonstrate_adaptive_tuning(LibEtudeDesktopOptimizer* optimizer) {
    print_header("적응형 튜닝 시연");

    printf("현재 성능 통계 업데이트...\n");
    libetude_desktop_optimizer_update_stats(optimizer);

    printf("현재 상태:\n");
    printf("- 전체 CPU 사용률: %.1f%%\n", optimizer->overall_cpu_utilization * 100.0f);
    printf("- 전체 메모리 사용률: %.1f%%\n", optimizer->overall_memory_utilization * 100.0f);
    printf("- 워커 스레드 수: %u\n", optimizer->multicore.config.worker_thread_count);
    printf("- 오디오 버퍼 크기: %u frames\n", optimizer->audio.config.buffer_size_frames);

    printf("\n적응형 튜닝 시나리오 테스트...\n");

    // 시나리오 1: 고성능 모드 (높은 CPU 사용률, 낮은 지연 시간)
    printf("\n시나리오 1: 고성능 모드\n");
    LibEtudeErrorCode result = libetude_desktop_optimizer_adaptive_tuning(optimizer, 0.85f, 10);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 고성능 모드 튜닝 완료\n");
        printf("  - 워커 스레드: %u\n", optimizer->multicore.config.worker_thread_count);
        printf("  - 오디오 버퍼: %u frames\n", optimizer->audio.config.buffer_size_frames);
    }

    // 시나리오 2: 균형 모드 (중간 CPU 사용률, 중간 지연 시간)
    printf("\n시나리오 2: 균형 모드\n");
    result = libetude_desktop_optimizer_adaptive_tuning(optimizer, 0.70f, 20);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 균형 모드 튜닝 완료\n");
        printf("  - 워커 스레드: %u\n", optimizer->multicore.config.worker_thread_count);
        printf("  - 오디오 버퍼: %u frames\n", optimizer->audio.config.buffer_size_frames);
    }

    // 시나리오 3: 절약 모드 (낮은 CPU 사용률, 높은 지연 시간 허용)
    printf("\n시나리오 3: 절약 모드\n");
    result = libetude_desktop_optimizer_adaptive_tuning(optimizer, 0.50f, 50);
    if (result == LIBETUDE_SUCCESS) {
        printf("✓ 절약 모드 튜닝 완료\n");
        printf("  - 워커 스레드: %u\n", optimizer->multicore.config.worker_thread_count);
        printf("  - 오디오 버퍼: %u frames\n", optimizer->audio.config.buffer_size_frames);
    }
}

static void run_performance_monitoring(LibEtudeDesktopOptimizer* optimizer) {
    print_header("성능 모니터링 시연");

    printf("5초간 성능 모니터링을 실행합니다...\n");

    for (int i = 0; i < 5; i++) {
        printf("\n--- %d초 후 ---\n", i + 1);

        // 통계 업데이트
        libetude_desktop_optimizer_update_stats(optimizer);

        // 간단한 통계 출력
        printf("CPU 사용률: %.1f%% | 메모리 사용률: %.1f%%\n",
               optimizer->overall_cpu_utilization * 100.0f,
               optimizer->overall_memory_utilization * 100.0f);

        if (optimizer->multicore.scheduler) {
            printf("처리된 작업: %llu개 | 평균 작업 시간: %llu μs\n",
                   (unsigned long long)optimizer->multicore.total_tasks_processed,
                   (unsigned long long)optimizer->multicore.avg_task_duration_us);
        }

        if (optimizer->gpu_accel.initialized) {
            printf("GPU 사용률: %.1f%% | GPU 커널 실행: %llu회\n",
                   optimizer->gpu_accel.gpu_utilization * 100.0f,
                   (unsigned long long)optimizer->gpu_accel.gpu_kernel_executions);
        }

        if (optimizer->audio.initialized) {
            printf("오디오 콜백: %llu회 | 버퍼 언더런: %llu회\n",
                   (unsigned long long)optimizer->audio.audio_callbacks_processed,
                   (unsigned long long)optimizer->audio.buffer_underruns);
        }

        sleep(1);
    }

    printf("\n최종 상세 통계:\n");
    libetude_desktop_optimizer_print_stats(optimizer);
}

// ============================================================================
// 메인 함수
// ============================================================================

int main(int argc, char* argv[]) {
    printf("LibEtude 데스크톱 최적화 시연 프로그램\n");
    print_separator();

    // 명령행 인수 처리
    bool show_hardware_only = false;
    bool skip_gpu_demo = false;
    bool skip_audio_demo = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hardware-only") == 0) {
            show_hardware_only = true;
        } else if (strcmp(argv[i], "--skip-gpu") == 0) {
            skip_gpu_demo = true;
        } else if (strcmp(argv[i], "--skip-audio") == 0) {
            skip_audio_demo = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("사용법: %s [옵션]\n", argv[0]);
            printf("옵션:\n");
            printf("  --hardware-only  하드웨어 정보만 출력\n");
            printf("  --skip-gpu       GPU 시연 건너뛰기\n");
            printf("  --skip-audio     오디오 시연 건너뛰기\n");
            printf("  --help           이 도움말 출력\n");
            return 0;
        }
    }

    // 데스크톱 최적화 초기화
    LibEtudeDesktopOptimizer optimizer;
    LibEtudeErrorCode result = libetude_desktop_optimizer_init(&optimizer);
    if (result != LIBETUDE_SUCCESS) {
        printf("데스크톱 최적화 초기화 실패: %d\n", result);
        return 1;
    }

    // 하드웨어 정보 출력
    print_hardware_info(&optimizer.hardware_info);

    if (show_hardware_only) {
        libetude_desktop_optimizer_destroy(&optimizer);
        return 0;
    }

    // 멀티코어 최적화 시연
    demonstrate_multicore_optimization(&optimizer);

    // GPU 가속 시연 (선택적)
    if (!skip_gpu_demo) {
        demonstrate_gpu_acceleration(&optimizer);
    }

    // 오디오 백엔드 최적화 시연 (선택적)
    if (!skip_audio_demo) {
        demonstrate_audio_optimization(&optimizer);
    }

    // 적응형 튜닝 시연
    demonstrate_adaptive_tuning(&optimizer);

    // 성능 모니터링 시연
    run_performance_monitoring(&optimizer);

    // JSON 통계 출력
    print_header("JSON 형태 통계");
    char json_buffer[4096];
    result = libetude_desktop_optimizer_stats_to_json(&optimizer, json_buffer, sizeof(json_buffer));
    if (result == LIBETUDE_SUCCESS) {
        printf("%s\n", json_buffer);
    } else {
        printf("JSON 통계 생성 실패: %d\n", result);
    }

    // 정리
    libetude_desktop_optimizer_destroy(&optimizer);

    print_header("시연 완료");
    printf("LibEtude 데스크톱 최적화 기능 시연이 완료되었습니다.\n");

    return 0;
}