#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "libetude/embedded_optimization.h"
#include "libetude/api.h"
#include "libetude/error.h"

// 전역 변수
static ETEmbeddedContext* g_embedded_ctx = NULL;
static bool g_running = true;

// 시그널 핸들러
void signal_handler(int sig) {
    printf("\nShutting down embedded optimization example...\n");
    g_running = false;
}

// 사용법 출력
void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -m <mode>     Embedded mode (normal|minimal|ultra_low)\n");
    printf("  -p <preset>   Apply preset (microcontroller|iot|edge)\n");
    printf("  -t <seconds>  Run time in seconds (default: 10)\n");
    printf("  -v            Verbose output\n");
    printf("  -h            Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -m minimal -t 30\n", program_name);
    printf("  %s -p microcontroller -v\n", program_name);
    printf("  %s -p iot -t 60\n", program_name);
}

// 모드 문자열을 열거형으로 변환
ETEmbeddedMode parse_mode(const char* mode_str) {
    if (strcmp(mode_str, "normal") == 0) {
        return ET_EMBEDDED_MODE_NORMAL;
    } else if (strcmp(mode_str, "minimal") == 0) {
        return ET_EMBEDDED_MODE_MINIMAL;
    } else if (strcmp(mode_str, "ultra_low") == 0) {
        return ET_EMBEDDED_MODE_ULTRA_LOW;
    }
    return ET_EMBEDDED_MODE_NORMAL;
}

// 기본 설정 생성
ETEmbeddedConfig create_default_config() {
    ETEmbeddedConfig config = {0};

    // 기본 모드 설정
    config.mode = ET_EMBEDDED_MODE_NORMAL;

    // 기본 제약 조건 (일반적인 임베디드 시스템)
    config.constraints.max_memory_bytes = 2 * 1024 * 1024;  // 2MB
    config.constraints.max_cpu_freq_mhz = 800;              // 800MHz
    config.constraints.max_power_mw = 1000;                 // 1W
    config.constraints.has_fpu = true;
    config.constraints.has_simd = true;
    config.constraints.cache_size_kb = 256;
    config.constraints.flash_size_kb = 8192;               // 8MB
    config.constraints.ram_size_kb = 2048;                 // 2MB

    // 기본 최적화 설정
    config.enable_memory_pooling = true;
    config.enable_in_place_ops = false;
    config.enable_layer_streaming = false;
    config.enable_dynamic_freq = true;
    config.enable_sleep_mode = false;
    config.use_fixed_point = false;
    config.enable_quantization = true;
    config.default_quantization = 16;
    config.enable_cache_optimization = true;
    config.cache_line_size = 64;
    config.idle_timeout_ms = 1000;
    config.min_pool_size = 128 * 1024;                     // 128KB

    return config;
}

// 프리셋 적용
ETResult apply_preset(ETEmbeddedContext* ctx, const char* preset_name) {
    if (strcmp(preset_name, "microcontroller") == 0) {
        return et_embedded_apply_microcontroller_preset(ctx);
    } else if (strcmp(preset_name, "iot") == 0) {
        return et_embedded_apply_iot_device_preset(ctx);
    } else if (strcmp(preset_name, "edge") == 0) {
        return et_embedded_apply_edge_device_preset(ctx);
    }

    printf("Unknown preset: %s\n", preset_name);
    return ET_ERROR_INVALID_ARGUMENT;
}

// 성능 모니터링 스레드 시뮬레이션
void monitor_performance(ETEmbeddedContext* ctx, bool verbose) {
    ETEmbeddedStats stats;
    ETResult result = et_embedded_get_stats(ctx, &stats);

    if (result == ET_SUCCESS) {
        if (verbose) {
            printf("\n--- Performance Monitor ---\n");
            printf("Memory Usage: %zu / %zu bytes (%.1f%%)\n",
                   stats.current_memory_usage,
                   stats.peak_memory_usage,
                   stats.peak_memory_usage > 0 ?
                   (float)stats.current_memory_usage / stats.peak_memory_usage * 100.0f : 0.0f);
            printf("Power: %u mW (avg: %u mW)\n",
                   stats.current_power_mw, stats.average_power_mw);
            printf("CPU: %u MHz (%.1f%% util)\n",
                   stats.current_cpu_freq_mhz, stats.cpu_utilization * 100.0f);
            printf("Cache Hit Rate: %u%%\n", stats.cache_hit_rate);
            printf("Inference Time: %u ms\n", stats.inference_time_ms);
        } else {
            printf("Mem: %zuB, Power: %umW, CPU: %uMHz (%.1f%%), Cache: %u%%, Inf: %ums\n",
                   stats.current_memory_usage, stats.current_power_mw,
                   stats.current_cpu_freq_mhz, stats.cpu_utilization * 100.0f,
                   stats.cache_hit_rate, stats.inference_time_ms);
        }
    }
}

// 워크로드 시뮬레이션
void simulate_workload(ETEmbeddedContext* ctx) {
    // 메모리 할당/해제 시뮬레이션
    static int allocation_count = 0;
    allocation_count++;

    // 주기적으로 메모리 사용량 변화 시뮬레이션
    if (allocation_count % 5 == 0) {
        // 메모리 최적화 실행
        et_embedded_optimize_memory(ctx);
    }

    // 전력 최적화 시뮬레이션
    if (allocation_count % 10 == 0) {
        et_embedded_optimize_power(ctx);
    }

    // 캐시 최적화 시뮬레이션
    if (allocation_count % 15 == 0) {
        et_embedded_optimize_for_cache(ctx);
    }

    // 슬립 모드 테스트 (ultra_low 모드에서만)
    ETEmbeddedMode mode = et_embedded_get_mode(ctx);
    if (mode == ET_EMBEDDED_MODE_ULTRA_LOW && allocation_count % 20 == 0) {
        printf("Entering sleep mode...\n");
        et_embedded_enter_sleep_mode(ctx);
        usleep(100000); // 100ms 슬립
        et_embedded_exit_sleep_mode(ctx);
        printf("Exiting sleep mode...\n");
    }
}

// 리소스 체크 및 경고
void check_resources(ETEmbeddedContext* ctx) {
    // 메모리 체크
    if (!et_embedded_check_memory_available(ctx, 64 * 1024)) { // 64KB 체크
        printf("WARNING: Low memory available!\n");
    }

    // 전력 체크
    if (!et_embedded_check_power_budget(ctx, 100)) { // 100mW 체크
        printf("WARNING: Power budget exceeded!\n");
    }
}

int main(int argc, char* argv[]) {
    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 명령행 인수 파싱
    ETEmbeddedMode mode = ET_EMBEDDED_MODE_NORMAL;
    const char* preset = NULL;
    int run_time = 10; // 기본 10초
    bool verbose = false;

    int opt;
    while ((opt = getopt(argc, argv, "m:p:t:vh")) != -1) {
        switch (opt) {
            case 'm':
                mode = parse_mode(optarg);
                break;
            case 'p':
                preset = optarg;
                break;
            case 't':
                run_time = atoi(optarg);
                if (run_time <= 0) run_time = 10;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("LibEtude Embedded Optimization Example\n");
    printf("======================================\n");

    // 기본 설정 생성
    ETEmbeddedConfig config = create_default_config();
    config.mode = mode;

    // 임베디드 컨텍스트 생성
    g_embedded_ctx = et_embedded_create_context(&config);
    if (!g_embedded_ctx) {
        const ETError* error = et_get_last_error();
        printf("Failed to create embedded context: %s\n", error ? error->message : "Unknown error");
        return 1;
    }

    printf("Embedded context created successfully\n");

    // 프리셋 적용
    if (preset) {
        ETResult result = apply_preset(g_embedded_ctx, preset);
        if (result == ET_SUCCESS) {
            printf("Applied preset: %s\n", preset);
        } else {
            printf("Failed to apply preset %s: %s\n", preset, et_error_string(result));
        }
    }

    // 초기 설정 출력
    if (verbose) {
        et_embedded_print_config(g_embedded_ctx);
    }

    // 초기 진단 실행
    printf("\nRunning initial diagnostics...\n");
    et_embedded_run_diagnostics(g_embedded_ctx);

    // 메인 루프
    printf("\nStarting performance monitoring (running for %d seconds)...\n", run_time);
    printf("Press Ctrl+C to stop early\n\n");

    time_t start_time = time(NULL);
    int iteration = 0;

    while (g_running && (time(NULL) - start_time) < run_time) {
        iteration++;

        // 워크로드 시뮬레이션
        simulate_workload(g_embedded_ctx);

        // 성능 모니터링
        if (iteration % 10 == 0) { // 1초마다 (100ms * 10)
            monitor_performance(g_embedded_ctx, verbose);
        }

        // 리소스 체크
        if (iteration % 50 == 0) { // 5초마다
            check_resources(g_embedded_ctx);
        }

        // 100ms 대기
        usleep(100000);
    }

    // 최종 통계 출력
    printf("\n\nFinal Performance Statistics:\n");
    printf("============================\n");
    et_embedded_print_stats(g_embedded_ctx);

    // 최종 진단
    printf("\nFinal Diagnostics:\n");
    printf("==================\n");
    et_embedded_run_diagnostics(g_embedded_ctx);

    // 정리
    et_embedded_destroy_context(g_embedded_ctx);
    printf("\nEmbedded optimization example completed successfully\n");

    return 0;
}