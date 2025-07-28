#ifndef LIBETUDE_EMBEDDED_OPTIMIZATION_H
#define LIBETUDE_EMBEDDED_OPTIMIZATION_H

#include "types.h"
#include "memory.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 임베디드 최적화 모드 열거형
typedef enum {
    ET_EMBEDDED_MODE_NORMAL = 0,     // 일반 모드
    ET_EMBEDDED_MODE_MINIMAL = 1,    // 최소 메모리 모드
    ET_EMBEDDED_MODE_ULTRA_LOW = 2   // 초저전력 모드
} ETEmbeddedMode;

// 임베디드 리소스 제약 구조체
typedef struct {
    size_t max_memory_bytes;         // 최대 메모리 사용량 (바이트)
    uint32_t max_cpu_freq_mhz;       // 최대 CPU 주파수 (MHz)
    uint32_t max_power_mw;           // 최대 전력 소비 (mW)
    bool has_fpu;                    // 부동소수점 유닛 존재 여부
    bool has_simd;                   // SIMD 지원 여부
    uint32_t cache_size_kb;          // 캐시 크기 (KB)
    uint32_t flash_size_kb;          // 플래시 메모리 크기 (KB)
    uint32_t ram_size_kb;            // RAM 크기 (KB)
} ETEmbeddedConstraints;

// 임베디드 최적화 설정 구조체
typedef struct {
    ETEmbeddedMode mode;             // 최적화 모드
    ETEmbeddedConstraints constraints; // 리소스 제약

    // 메모리 최적화 설정
    bool enable_memory_pooling;      // 메모리 풀링 활성화
    bool enable_in_place_ops;        // 인플레이스 연산 활성화
    bool enable_layer_streaming;     // 레이어 스트리밍 활성화
    size_t min_pool_size;            // 최소 메모리 풀 크기

    // 전력 최적화 설정
    bool enable_dynamic_freq;        // 동적 주파수 조절 활성화
    bool enable_sleep_mode;          // 슬립 모드 활성화
    uint32_t idle_timeout_ms;        // 유휴 타임아웃 (ms)

    // 연산 최적화 설정
    bool use_fixed_point;            // 고정소수점 연산 사용
    bool enable_quantization;        // 양자화 활성화
    uint8_t default_quantization;    // 기본 양자화 비트 수

    // 캐시 최적화 설정
    bool enable_cache_optimization;  // 캐시 최적화 활성화
    size_t cache_line_size;          // 캐시 라인 크기
} ETEmbeddedConfig;

// 임베디드 성능 통계 구조체
typedef struct {
    size_t current_memory_usage;     // 현재 메모리 사용량
    size_t peak_memory_usage;        // 최대 메모리 사용량
    uint32_t current_power_mw;       // 현재 전력 소비
    uint32_t average_power_mw;       // 평균 전력 소비
    uint32_t current_cpu_freq_mhz;   // 현재 CPU 주파수
    float cpu_utilization;           // CPU 사용률 (0.0-1.0)
    uint32_t cache_hit_rate;         // 캐시 히트율 (%)
    uint32_t inference_time_ms;      // 추론 시간 (ms)
} ETEmbeddedStats;

// 임베디드 최적화 컨텍스트
typedef struct ETEmbeddedContext ETEmbeddedContext;

// 임베디드 최적화 초기화 및 해제
ETEmbeddedContext* et_embedded_create_context(const ETEmbeddedConfig* config);
void et_embedded_destroy_context(ETEmbeddedContext* ctx);

// 임베디드 모드 설정
ETResult et_embedded_set_mode(ETEmbeddedContext* ctx, ETEmbeddedMode mode);
ETEmbeddedMode et_embedded_get_mode(const ETEmbeddedContext* ctx);

// 리소스 제약 설정
ETResult et_embedded_set_constraints(ETEmbeddedContext* ctx, const ETEmbeddedConstraints* constraints);
ETResult et_embedded_get_constraints(const ETEmbeddedContext* ctx, ETEmbeddedConstraints* constraints);

// 메모리 최적화 함수
ETResult et_embedded_optimize_memory(ETEmbeddedContext* ctx);
ETResult et_embedded_enable_minimal_memory_mode(ETEmbeddedContext* ctx, bool enable);
ETResult et_embedded_set_memory_limit(ETEmbeddedContext* ctx, size_t limit_bytes);

// 전력 최적화 함수
ETResult et_embedded_optimize_power(ETEmbeddedContext* ctx);
ETResult et_embedded_enable_low_power_mode(ETEmbeddedContext* ctx, bool enable);
ETResult et_embedded_set_cpu_frequency(ETEmbeddedContext* ctx, uint32_t freq_mhz);
ETResult et_embedded_enter_sleep_mode(ETEmbeddedContext* ctx);
ETResult et_embedded_exit_sleep_mode(ETEmbeddedContext* ctx);

// 연산 최적화 함수
ETResult et_embedded_enable_fixed_point(ETEmbeddedContext* ctx, bool enable);
ETResult et_embedded_set_quantization_level(ETEmbeddedContext* ctx, uint8_t bits);
ETResult et_embedded_optimize_for_cache(ETEmbeddedContext* ctx);

// 성능 모니터링 함수
ETResult et_embedded_get_stats(const ETEmbeddedContext* ctx, ETEmbeddedStats* stats);
ETResult et_embedded_reset_stats(ETEmbeddedContext* ctx);

// 리소스 체크 함수
bool et_embedded_check_memory_available(const ETEmbeddedContext* ctx, size_t required_bytes);
bool et_embedded_check_power_budget(const ETEmbeddedContext* ctx, uint32_t required_mw);
ETResult et_embedded_validate_constraints(const ETEmbeddedConstraints* constraints);

// 프리셋 설정 함수
ETResult et_embedded_apply_microcontroller_preset(ETEmbeddedContext* ctx);
ETResult et_embedded_apply_iot_device_preset(ETEmbeddedContext* ctx);
ETResult et_embedded_apply_edge_device_preset(ETEmbeddedContext* ctx);

// 디버그 및 진단 함수
void et_embedded_print_config(const ETEmbeddedContext* ctx);
void et_embedded_print_stats(const ETEmbeddedContext* ctx);
ETResult et_embedded_run_diagnostics(ETEmbeddedContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_EMBEDDED_OPTIMIZATION_H