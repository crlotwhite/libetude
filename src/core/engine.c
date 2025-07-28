/**
 * @file engine.c
 * @brief LibEtude 핵심 엔진 내부 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/api.h"
#include "libetude/types.h"
#include "libetude/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 내부 유틸리티 함수
// ============================================================================

/**
 * @brief 모델 파일 존재 여부를 확인합니다
 */
bool et_check_model_file(const char* path) {
    if (!path) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/**
 * @brief 엔진 초기화 검증 (불투명한 포인터로 처리)
 */
bool et_validate_engine(const LibEtudeEngine* engine) {
    // 구조체 내부에 접근할 수 없으므로 NULL 체크만 수행
    return engine != NULL;
}

/**
 * @brief 텍스트 유효성 검증
 */
bool et_validate_text(const char* text) {
    if (!text) {
        return false;
    }

    size_t len = strlen(text);
    return len > 0 && len <= LIBETUDE_MAX_TEXT_LENGTH;
}

/**
 * @brief 오디오 버퍼 유효성 검증
 */
bool et_validate_audio_buffer(const float* buffer, int length) {
    return buffer != NULL && length > 0;
}

/**
 * @brief 품질 모드 유효성 검증
 */
bool et_validate_quality_mode(QualityMode mode) {
    return mode >= LIBETUDE_QUALITY_FAST && mode <= LIBETUDE_QUALITY_HIGH;
}

/**
 * @brief 성능 통계 초기화
 */
void et_init_performance_stats(PerformanceStats* stats) {
    if (!stats) {
        return;
    }

    stats->inference_time_ms = 0.0;
    stats->memory_usage_mb = 0.0;
    stats->cpu_usage_percent = 0.0;
    stats->gpu_usage_percent = 0.0;
    stats->active_threads = 1;
}

/**
 * @brief 더미 오디오 생성 (테스트용)
 */
void et_generate_dummy_audio(float* buffer, int length, float frequency) {
    if (!buffer || length <= 0) {
        return;
    }

    const float sample_rate = LIBETUDE_DEFAULT_SAMPLE_RATE;
    const float amplitude = 0.1f;

    for (int i = 0; i < length; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
}

/**
 * @brief 엔진 상태 문자열 반환
 */
const char* et_get_engine_state_string(const LibEtudeEngine* engine) {
    if (!engine) {
        return "NULL";
    }

    // 구조체 내부에 접근할 수 없으므로 기본 상태 반환
    return "UNKNOWN";
}