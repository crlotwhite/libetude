#include "libetude/embedded_optimization.h"
#include "libetude/memory.h"
#include "libetude/error.h"
#include "libetude/profiler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef __ARM_ARCH
#include <arm_neon.h>
#endif

// 임베디드 최적화 컨텍스트 구조체
struct ETEmbeddedContext {
    ETEmbeddedConfig config;         // 설정
    ETEmbeddedStats stats;           // 통계
    ETMemoryPool* memory_pool;       // 메모리 풀

    // 전력 관리
    bool is_sleeping;                // 슬립 모드 상태
    uint64_t last_activity_time;     // 마지막 활동 시간
    uint32_t original_cpu_freq;      // 원래 CPU 주파수

    // 메모리 관리
    void* minimal_buffer;            // 최소 메모리 버퍼
    size_t minimal_buffer_size;      // 최소 버퍼 크기
    bool memory_optimized;           // 메모리 최적화 상태

    // 캐시 최적화
    void* cache_aligned_buffer;      // 캐시 정렬 버퍼
    size_t cache_buffer_size;        // 캐시 버퍼 크기

    // 통계 추적
    uint64_t start_time;             // 시작 시간
    uint64_t total_inference_time;   // 총 추론 시간
    uint32_t inference_count;        // 추론 횟수
};

// 내부 함수 선언
static uint64_t get_current_time_ms(void);
static uint32_t get_current_cpu_frequency(void);
static uint32_t get_current_power_consumption(void);
static float get_cpu_utilization(void);
static ETResult optimize_memory_layout(ETEmbeddedContext* ctx);
static ETResult apply_power_optimizations(ETEmbeddedContext* ctx);
static ETResult configure_cache_optimization(ETEmbeddedContext* ctx);

// 현재 시간 가져오기 (밀리초)
static uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// 현재 CPU 주파수 가져오기 (플랫폼별 구현 필요)
static uint32_t get_current_cpu_frequency(void) {
#ifdef __linux__
    FILE* file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (file) {
        uint32_t freq;
        if (fscanf(file, "%u", &freq) == 1) {
            fclose(file);
            return freq / 1000; // kHz를 MHz로 변환
        }
        fclose(file);
    }
#endif
    return 0; // 알 수 없음
}

// 현재 전력 소비 가져오기 (추정값)
static uint32_t get_current_power_consumption(void) {
    // 실제 구현에서는 하드웨어별 전력 측정 API 사용
    // 여기서는 CPU 사용률 기반 추정값 반환
    float cpu_util = get_cpu_utilization();
    return (uint32_t)(100 + cpu_util * 400); // 100-500mW 범위 추정
}

// CPU 사용률 가져오기
static float get_cpu_utilization(void) {
#ifdef __linux__
    static long prev_idle = 0, prev_total = 0;
    FILE* file = fopen("/proc/stat", "r");
    if (file) {
        long user, nice, system, idle, iowait, irq, softirq;
        if (fscanf(file, "cpu %ld %ld %ld %ld %ld %ld %ld",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq) == 7) {
            long total = user + nice + system + idle + iowait + irq + softirq;
            long diff_idle = idle - prev_idle;
            long diff_total = total - prev_total;

            float utilization = 0.0f;
            if (diff_total > 0) {
                utilization = 1.0f - (float)diff_idle / diff_total;
            }

            prev_idle = idle;
            prev_total = total;
            fclose(file);
            return utilization;
        }
        fclose(file);
    }
#endif
    return 0.0f; // 알 수 없음
}

// 임베디드 컨텍스트 생성
ETEmbeddedContext* et_embedded_create_context(const ETEmbeddedConfig* config) {
    if (!config) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Config cannot be null");
        return NULL;
    }

    ETEmbeddedContext* ctx = (ETEmbeddedContext*)calloc(1, sizeof(ETEmbeddedContext));
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "Failed to allocate embedded context");
        return NULL;
    }

    // 설정 복사
    ctx->config = *config;
    ctx->start_time = get_current_time_ms();
    ctx->original_cpu_freq = get_current_cpu_frequency();

    // 메모리 풀 생성
    size_t pool_size = config->min_pool_size > 0 ? config->min_pool_size : 1024 * 1024; // 기본 1MB
    if (config->mode == ET_EMBEDDED_MODE_MINIMAL) {
        pool_size = pool_size / 4; // 최소 모드에서는 1/4로 축소
    } else if (config->mode == ET_EMBEDDED_MODE_ULTRA_LOW) {
        pool_size = pool_size / 8; // 초저전력 모드에서는 1/8로 축소
    }

    ctx->memory_pool = et_create_memory_pool(pool_size, 32); // 32바이트 정렬
    // 메모리 풀 생성 실패는 경고만 출력하고 계속 진행
    if (!ctx->memory_pool) {
        printf("Warning: Failed to create memory pool, continuing without it\n");
    }

    // 최소 메모리 버퍼 할당
    if (config->enable_memory_pooling) {
        ctx->minimal_buffer_size = 64 * 1024; // 64KB 기본값
        if (config->mode == ET_EMBEDDED_MODE_MINIMAL) {
            ctx->minimal_buffer_size = 32 * 1024; // 32KB
        } else if (config->mode == ET_EMBEDDED_MODE_ULTRA_LOW) {
            ctx->minimal_buffer_size = 16 * 1024; // 16KB
        }

        if (ctx->memory_pool) {
            ctx->minimal_buffer = et_alloc_from_pool(ctx->memory_pool, ctx->minimal_buffer_size);
            if (!ctx->minimal_buffer) {
                printf("Warning: Failed to allocate minimal buffer from pool\n");
            }
        }
    }

    // 캐시 최적화 설정
    if (config->enable_cache_optimization) {
        configure_cache_optimization(ctx);
    }

    // 초기 최적화 적용
    optimize_memory_layout(ctx);
    apply_power_optimizations(ctx);

    return ctx;
}

// 임베디드 컨텍스트 해제
void et_embedded_destroy_context(ETEmbeddedContext* ctx) {
    if (!ctx) return;

    // 슬립 모드에서 깨우기
    if (ctx->is_sleeping) {
        et_embedded_exit_sleep_mode(ctx);
    }

    // 메모리 해제
    if (ctx->memory_pool) {
        et_destroy_memory_pool(ctx->memory_pool);
    }

    if (ctx->cache_aligned_buffer) {
        free(ctx->cache_aligned_buffer);
    }

    free(ctx);
}

// 임베디드 모드 설정
ETResult et_embedded_set_mode(ETEmbeddedContext* ctx, ETEmbeddedMode mode) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;        return ET_ERROR_INVALID_ARGUMENT;
    }

    ctx->config.mode = mode;

    // 모드에 따른 설정 조정
    switch (mode) {
        case ET_EMBEDDED_MODE_MINIMAL:
            ctx->config.enable_memory_pooling = true;
            ctx->config.enable_in_place_ops = true;
            ctx->config.enable_layer_streaming = true;
            ctx->config.use_fixed_point = true;
            ctx->config.enable_quantization = true;
            ctx->config.default_quantization = 8; // INT8
            break;

        case ET_EMBEDDED_MODE_ULTRA_LOW:
            ctx->config.enable_memory_pooling = true;
            ctx->config.enable_in_place_ops = true;
            ctx->config.enable_layer_streaming = true;
            ctx->config.enable_dynamic_freq = true;
            ctx->config.enable_sleep_mode = true;
            ctx->config.use_fixed_point = true;
            ctx->config.enable_quantization = true;
            ctx->config.default_quantization = 4; // INT4
            break;

        default:
            break;
    }

    // 최적화 재적용
    optimize_memory_layout(ctx);
    apply_power_optimizations(ctx);

    return ET_SUCCESS;
}

// 임베디드 모드 가져오기
ETEmbeddedMode et_embedded_get_mode(const ETEmbeddedContext* ctx) {
    if (!ctx) return ET_EMBEDDED_MODE_NORMAL;
    return ctx->config.mode;
}

// 메모리 최적화
ETResult et_embedded_optimize_memory(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    return optimize_memory_layout(ctx);
}

// 최소 메모리 모드 활성화
ETResult et_embedded_enable_minimal_memory_mode(ETEmbeddedContext* ctx, bool enable) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    ctx->config.enable_memory_pooling = enable;
    ctx->config.enable_in_place_ops = enable;
    ctx->config.enable_layer_streaming = enable;

    if (enable) {
        return optimize_memory_layout(ctx);
    }

    return ET_SUCCESS;
}

// 메모리 제한 설정
ETResult et_embedded_set_memory_limit(ETEmbeddedContext* ctx, size_t limit_bytes) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    ctx->config.constraints.max_memory_bytes = limit_bytes;

    // 현재 사용량이 제한을 초과하는지 확인
    if (ctx->stats.current_memory_usage > limit_bytes) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "Current memory usage exceeds new limit");
        return ET_ERROR_OUT_OF_MEMORY;    }

    return ET_SUCCESS;
}

// 전력 최적화
ETResult et_embedded_optimize_power(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    return apply_power_optimizations(ctx);
}

// 저전력 모드 활성화
ETResult et_embedded_enable_low_power_mode(ETEmbeddedContext* ctx, bool enable) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    ctx->config.enable_dynamic_freq = enable;
    ctx->config.enable_sleep_mode = enable;

    if (enable) {
        return apply_power_optimizations(ctx);
    }

    return ET_SUCCESS;
}

// CPU 주파수 설정
ETResult et_embedded_set_cpu_frequency(ETEmbeddedContext* ctx, uint32_t freq_mhz) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (freq_mhz > ctx->config.constraints.max_cpu_freq_mhz) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Frequency exceeds maximum allowed");
        return ET_ERROR_INVALID_ARGUMENT;    }

    // 실제 구현에서는 플랫폼별 주파수 설정 API 사용
#ifdef __linux__
    char command[256];
    snprintf(command, sizeof(command),
             "echo %u > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed",
             freq_mhz * 1000);
    system(command);
#endif

    return ET_SUCCESS;
}

// 슬립 모드 진입
ETResult et_embedded_enter_sleep_mode(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (!ctx->config.enable_sleep_mode) {
        ET_SET_ERROR(ET_ERROR_RUNTIME, "Sleep mode is not enabled");
        return ET_ERROR_RUNTIME;    }

    ctx->is_sleeping = true;

    // CPU 주파수를 최소로 설정
    if (ctx->config.enable_dynamic_freq) {
        et_embedded_set_cpu_frequency(ctx, ctx->config.constraints.max_cpu_freq_mhz / 4);
    }

    return ET_SUCCESS;
}

// 슬립 모드 해제
ETResult et_embedded_exit_sleep_mode(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (!ctx->is_sleeping) {
        return ET_SUCCESS; // 이미 깨어있음
    }

    ctx->is_sleeping = false;
    ctx->last_activity_time = get_current_time_ms();

    // CPU 주파수를 원래대로 복원
    if (ctx->config.enable_dynamic_freq) {
        et_embedded_set_cpu_frequency(ctx, ctx->original_cpu_freq);
    }

    return ET_SUCCESS;
}

// 고정소수점 연산 활성화
ETResult et_embedded_enable_fixed_point(ETEmbeddedContext* ctx, bool enable) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    ctx->config.use_fixed_point = enable;
    return ET_SUCCESS;
}

// 양자화 레벨 설정
ETResult et_embedded_set_quantization_level(ETEmbeddedContext* ctx, uint8_t bits) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (bits != 4 && bits != 8 && bits != 16) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Supported quantization levels: 4, 8, 16 bits");
        return ET_ERROR_INVALID_ARGUMENT;    }

    ctx->config.enable_quantization = true;
    ctx->config.default_quantization = bits;

    return ET_SUCCESS;
}

// 캐시 최적화
ETResult et_embedded_optimize_for_cache(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    return configure_cache_optimization(ctx);
}

// 성능 통계 가져오기
ETResult et_embedded_get_stats(const ETEmbeddedContext* ctx, ETEmbeddedStats* stats) {
    if (!ctx || !stats) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context and stats cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    // 현재 통계 업데이트
    ETEmbeddedContext* mutable_ctx = (ETEmbeddedContext*)ctx;
    mutable_ctx->stats.current_power_mw = get_current_power_consumption();
    mutable_ctx->stats.current_cpu_freq_mhz = get_current_cpu_frequency();
    mutable_ctx->stats.cpu_utilization = get_cpu_utilization();

    // 평균 전력 계산
    uint64_t elapsed_time = get_current_time_ms() - ctx->start_time;
    if (elapsed_time > 0) {
        mutable_ctx->stats.average_power_mw =
            (mutable_ctx->stats.average_power_mw + mutable_ctx->stats.current_power_mw) / 2;
    }

    *stats = ctx->stats;
    return ET_SUCCESS;
}

// 통계 리셋
ETResult et_embedded_reset_stats(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    memset(&ctx->stats, 0, sizeof(ETEmbeddedStats));
    ctx->start_time = get_current_time_ms();
    ctx->inference_count = 0;
    ctx->total_inference_time = 0;

    return ET_SUCCESS;
}

// 메모리 가용성 체크
bool et_embedded_check_memory_available(const ETEmbeddedContext* ctx, size_t required_bytes) {
    if (!ctx) return false;

    size_t available = ctx->config.constraints.max_memory_bytes - ctx->stats.current_memory_usage;
    return available >= required_bytes;
}

// 전력 예산 체크
bool et_embedded_check_power_budget(const ETEmbeddedContext* ctx, uint32_t required_mw) {
    if (!ctx) return false;

    uint32_t available = ctx->config.constraints.max_power_mw - ctx->stats.current_power_mw;
    return available >= required_mw;
}

// 제약 조건 검증
ETResult et_embedded_validate_constraints(const ETEmbeddedConstraints* constraints) {
    if (!constraints) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Constraints cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (constraints->max_memory_bytes == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Max memory must be greater than 0");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (constraints->max_cpu_freq_mhz == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Max CPU frequency must be greater than 0");
        return ET_ERROR_INVALID_ARGUMENT;    }

    if (constraints->max_power_mw == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Max power must be greater than 0");
        return ET_ERROR_INVALID_ARGUMENT;    }

    return ET_SUCCESS;
}

// 마이크로컨트롤러 프리셋 적용
ETResult et_embedded_apply_microcontroller_preset(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    // 마이크로컨트롤러용 극한 최적화 설정
    ctx->config.mode = ET_EMBEDDED_MODE_ULTRA_LOW;
    ctx->config.constraints.max_memory_bytes = 64 * 1024;    // 64KB
    ctx->config.constraints.max_cpu_freq_mhz = 48;           // 48MHz
    ctx->config.constraints.max_power_mw = 50;               // 50mW
    ctx->config.constraints.has_fpu = false;
    ctx->config.constraints.has_simd = false;
    ctx->config.constraints.cache_size_kb = 0;
    ctx->config.constraints.flash_size_kb = 256;
    ctx->config.constraints.ram_size_kb = 64;

    ctx->config.enable_memory_pooling = true;
    ctx->config.enable_in_place_ops = true;
    ctx->config.enable_layer_streaming = true;
    ctx->config.enable_dynamic_freq = true;
    ctx->config.enable_sleep_mode = true;
    ctx->config.use_fixed_point = true;
    ctx->config.enable_quantization = true;
    ctx->config.default_quantization = 4;
    ctx->config.idle_timeout_ms = 100;

    return et_embedded_set_mode(ctx, ET_EMBEDDED_MODE_ULTRA_LOW);
}

// IoT 디바이스 프리셋 적용
ETResult et_embedded_apply_iot_device_preset(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    // IoT 디바이스용 최적화 설정
    ctx->config.mode = ET_EMBEDDED_MODE_MINIMAL;
    ctx->config.constraints.max_memory_bytes = 512 * 1024;   // 512KB
    ctx->config.constraints.max_cpu_freq_mhz = 168;          // 168MHz
    ctx->config.constraints.max_power_mw = 200;              // 200mW
    ctx->config.constraints.has_fpu = true;
    ctx->config.constraints.has_simd = false;
    ctx->config.constraints.cache_size_kb = 16;
    ctx->config.constraints.flash_size_kb = 1024;
    ctx->config.constraints.ram_size_kb = 512;

    ctx->config.enable_memory_pooling = true;
    ctx->config.enable_in_place_ops = true;
    ctx->config.enable_layer_streaming = true;
    ctx->config.enable_dynamic_freq = true;
    ctx->config.enable_sleep_mode = true;
    ctx->config.use_fixed_point = false;
    ctx->config.enable_quantization = true;
    ctx->config.default_quantization = 8;
    ctx->config.idle_timeout_ms = 500;

    return et_embedded_set_mode(ctx, ET_EMBEDDED_MODE_MINIMAL);
}

// 엣지 디바이스 프리셋 적용
ETResult et_embedded_apply_edge_device_preset(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    // 엣지 디바이스용 균형 최적화 설정
    ctx->config.mode = ET_EMBEDDED_MODE_NORMAL;
    ctx->config.constraints.max_memory_bytes = 4 * 1024 * 1024; // 4MB
    ctx->config.constraints.max_cpu_freq_mhz = 800;             // 800MHz
    ctx->config.constraints.max_power_mw = 1000;                // 1W
    ctx->config.constraints.has_fpu = true;
    ctx->config.constraints.has_simd = true;
    ctx->config.constraints.cache_size_kb = 256;
    ctx->config.constraints.flash_size_kb = 8192;
    ctx->config.constraints.ram_size_kb = 4096;

    ctx->config.enable_memory_pooling = true;
    ctx->config.enable_in_place_ops = false;
    ctx->config.enable_layer_streaming = false;
    ctx->config.enable_dynamic_freq = true;
    ctx->config.enable_sleep_mode = false;
    ctx->config.use_fixed_point = false;
    ctx->config.enable_quantization = true;
    ctx->config.default_quantization = 16;
    ctx->config.idle_timeout_ms = 1000;

    return et_embedded_set_mode(ctx, ET_EMBEDDED_MODE_NORMAL);
}

// 설정 출력
void et_embedded_print_config(const ETEmbeddedContext* ctx) {
    if (!ctx) return;

    printf("=== Embedded Optimization Configuration ===\n");
    printf("Mode: %s\n",
           ctx->config.mode == ET_EMBEDDED_MODE_NORMAL ? "Normal" :
           ctx->config.mode == ET_EMBEDDED_MODE_MINIMAL ? "Minimal" : "Ultra Low");

    printf("Constraints:\n");
    printf("  Max Memory: %zu bytes\n", ctx->config.constraints.max_memory_bytes);
    printf("  Max CPU Freq: %u MHz\n", ctx->config.constraints.max_cpu_freq_mhz);
    printf("  Max Power: %u mW\n", ctx->config.constraints.max_power_mw);
    printf("  Has FPU: %s\n", ctx->config.constraints.has_fpu ? "Yes" : "No");
    printf("  Has SIMD: %s\n", ctx->config.constraints.has_simd ? "Yes" : "No");

    printf("Optimizations:\n");
    printf("  Memory Pooling: %s\n", ctx->config.enable_memory_pooling ? "Enabled" : "Disabled");
    printf("  In-place Ops: %s\n", ctx->config.enable_in_place_ops ? "Enabled" : "Disabled");
    printf("  Layer Streaming: %s\n", ctx->config.enable_layer_streaming ? "Enabled" : "Disabled");
    printf("  Dynamic Frequency: %s\n", ctx->config.enable_dynamic_freq ? "Enabled" : "Disabled");
    printf("  Sleep Mode: %s\n", ctx->config.enable_sleep_mode ? "Enabled" : "Disabled");
    printf("  Fixed Point: %s\n", ctx->config.use_fixed_point ? "Enabled" : "Disabled");
    printf("  Quantization: %s (%u bits)\n",
           ctx->config.enable_quantization ? "Enabled" : "Disabled",
           ctx->config.default_quantization);
}

// 통계 출력
void et_embedded_print_stats(const ETEmbeddedContext* ctx) {
    if (!ctx) return;

    printf("=== Embedded Performance Statistics ===\n");
    printf("Memory Usage:\n");
    printf("  Current: %zu bytes\n", ctx->stats.current_memory_usage);
    printf("  Peak: %zu bytes\n", ctx->stats.peak_memory_usage);

    printf("Power Consumption:\n");
    printf("  Current: %u mW\n", ctx->stats.current_power_mw);
    printf("  Average: %u mW\n", ctx->stats.average_power_mw);

    printf("CPU:\n");
    printf("  Current Frequency: %u MHz\n", ctx->stats.current_cpu_freq_mhz);
    printf("  Utilization: %.1f%%\n", ctx->stats.cpu_utilization * 100.0f);

    printf("Performance:\n");
    printf("  Cache Hit Rate: %u%%\n", ctx->stats.cache_hit_rate);
    printf("  Inference Time: %u ms\n", ctx->stats.inference_time_ms);

    if (ctx->inference_count > 0) {
        printf("  Average Inference Time: %.1f ms\n",
               (float)ctx->total_inference_time / ctx->inference_count);
    }
}

// 진단 실행
ETResult et_embedded_run_diagnostics(ETEmbeddedContext* ctx) {
    if (!ctx) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "Context cannot be null");
        return ET_ERROR_INVALID_ARGUMENT;    }

    printf("=== Embedded System Diagnostics ===\n");

    // 메모리 진단
    printf("Memory Diagnostics:\n");
    if (ctx->memory_pool) {
        printf("  Memory pool status: OK\n");
        printf("  Pool utilization: %.1f%%\n",
               (float)ctx->stats.current_memory_usage / ctx->config.constraints.max_memory_bytes * 100.0f);
    } else {
        printf("  Memory pool status: ERROR - Not initialized\n");
    }

    // 전력 진단
    printf("Power Diagnostics:\n");
    uint32_t current_power = get_current_power_consumption();
    printf("  Current power consumption: %u mW\n", current_power);
    if (current_power > ctx->config.constraints.max_power_mw) {
        printf("  WARNING: Power consumption exceeds limit!\n");
    } else {
        printf("  Power consumption: OK\n");
    }

    // CPU 진단
    printf("CPU Diagnostics:\n");
    uint32_t current_freq = get_current_cpu_frequency();
    printf("  Current CPU frequency: %u MHz\n", current_freq);
    // current_freq가 0이 아니고 최대 주파수를 초과하는 경우에만 경고
    if (current_freq != 0 && current_freq > ctx->config.constraints.max_cpu_freq_mhz) {
        printf("  WARNING: CPU frequency exceeds limit!\n");
    } else if (current_freq == 0) {
        printf("  WARNING: Could not read CPU frequency\n");
    } else {
        printf("  CPU frequency: OK\n");
    }

    // 캐시 진단
    if (ctx->config.enable_cache_optimization) {
        printf("Cache Diagnostics:\n");
        printf("  Cache optimization: Enabled\n");
        printf("  Cache hit rate: %u%%\n", ctx->stats.cache_hit_rate);
    }

    return ET_SUCCESS;
}

// 내부 함수 구현

// 메모리 레이아웃 최적화
static ETResult optimize_memory_layout(ETEmbeddedContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    // 메모리 풀 재구성
    if (ctx->config.enable_memory_pooling && ctx->memory_pool) {
        et_reset_pool(ctx->memory_pool);

        // 최소 메모리 모드에서는 더 작은 청크 사용
        if (ctx->config.mode == ET_EMBEDDED_MODE_MINIMAL ||
            ctx->config.mode == ET_EMBEDDED_MODE_ULTRA_LOW) {
            // 메모리 풀을 더 작은 단위로 분할
            ctx->memory_optimized = true;
        }
    }

    return ET_SUCCESS;
}

// 전력 최적화 적용
static ETResult apply_power_optimizations(ETEmbeddedContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    // 동적 주파수 조절
    if (ctx->config.enable_dynamic_freq) {
        uint32_t target_freq = ctx->config.constraints.max_cpu_freq_mhz;

        // 모드에 따른 주파수 조절
        if (ctx->config.mode == ET_EMBEDDED_MODE_MINIMAL) {
            target_freq = target_freq * 3 / 4; // 75%
        } else if (ctx->config.mode == ET_EMBEDDED_MODE_ULTRA_LOW) {
            target_freq = target_freq / 2; // 50%
        }

        et_embedded_set_cpu_frequency(ctx, target_freq);
    }

    // 슬립 모드 설정
    if (ctx->config.enable_sleep_mode) {
        ctx->last_activity_time = get_current_time_ms();
    }

    return ET_SUCCESS;
}

// 캐시 최적화 설정
static ETResult configure_cache_optimization(ETEmbeddedContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    if (ctx->config.constraints.cache_size_kb > 0) {
        // 캐시 라인 크기 설정 (일반적으로 64바이트)
        ctx->config.cache_line_size = 64;

        // 캐시 정렬 버퍼 할당
        ctx->cache_buffer_size = ctx->config.constraints.cache_size_kb * 1024 / 4; // 캐시의 1/4 사용
#ifdef _WIN32
        ctx->cache_aligned_buffer = _aligned_malloc(ctx->cache_buffer_size, ctx->config.cache_line_size);
#else
        ctx->cache_aligned_buffer = aligned_alloc(ctx->config.cache_line_size, ctx->cache_buffer_size);
#endif

        if (!ctx->cache_aligned_buffer) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
    }

    return ET_SUCCESS;
}