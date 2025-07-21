/**
 * @file api.h
 * @brief LibEtude 핵심 C API
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude는 음성 합성에 특화된 AI 추론 엔진입니다.
 * 이 헤더는 외부 애플리케이션에서 사용할 수 있는 핵심 C API를 정의합니다.
 */

#ifndef LIBETUDE_API_H
#define LIBETUDE_API_H

#include "types.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LibEtude 엔진 핸들
 *
 * 불투명한 포인터로 내부 구현을 숨깁니다.
 */
typedef struct LibEtudeEngine LibEtudeEngine;

/**
 * @brief 오디오 스트림 콜백 함수 타입
 *
 * @param audio 생성된 오디오 데이터 (float 배열)
 * @param length 오디오 샘플 수
 * @param user_data 사용자 정의 데이터
 */
typedef void (*AudioStreamCallback)(const float* audio, int length, void* user_data);

/**
 * @brief 성능 통계 구조체
 */
typedef struct {
    double inference_time_ms;      /**< 추론 시간 (밀리초) */
    double memory_usage_mb;        /**< 메모리 사용량 (MB) */
    double cpu_usage_percent;      /**< CPU 사용률 (%) */
    double gpu_usage_percent;      /**< GPU 사용률 (%) */
    int active_threads;            /**< 활성 스레드 수 */
} PerformanceStats;

/**
 * @brief 품질 모드 열거형
 */
typedef enum {
    LIBETUDE_QUALITY_FAST = 0,     /**< 빠른 처리 (낮은 품질) */
    LIBETUDE_QUALITY_BALANCED = 1, /**< 균형 모드 */
    LIBETUDE_QUALITY_HIGH = 2      /**< 고품질 (느린 처리) */
} QualityMode;

// ============================================================================
// 엔진 생성 및 관리
// ============================================================================

/**
 * @brief LibEtude 엔진을 생성합니다
 *
 * @param model_path 모델 파일 경로 (.lef 또는 .lefx)
 * @return 성공 시 엔진 핸들, 실패 시 NULL
 */
LIBETUDE_API LibEtudeEngine* libetude_create_engine(const char* model_path);

/**
 * @brief LibEtude 엔진을 해제합니다
 *
 * @param engine 엔진 핸들
 */
LIBETUDE_API void libetude_destroy_engine(LibEtudeEngine* engine);

// ============================================================================
// 음성 합성 (동기 처리)
// ============================================================================

/**
 * @brief 텍스트를 음성으로 합성합니다 (동기)
 *
 * @param engine 엔진 핸들
 * @param text 합성할 텍스트 (UTF-8)
 * @param output_audio 출력 오디오 버퍼 (호출자가 할당)
 * @param output_length 입력: 버퍼 크기, 출력: 실제 오디오 길이
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_synthesize_text(LibEtudeEngine* engine,
                                          const char* text,
                                          float* output_audio,
                                          int* output_length);

/**
 * @brief 가사와 음표를 노래로 합성합니다 (동기)
 *
 * @param engine 엔진 핸들
 * @param lyrics 가사 텍스트 (UTF-8)
 * @param notes 음표 배열 (MIDI 노트 번호)
 * @param note_count 음표 개수
 * @param output_audio 출력 오디오 버퍼
 * @param output_length 입력: 버퍼 크기, 출력: 실제 오디오 길이
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_synthesize_singing(LibEtudeEngine* engine,
                                             const char* lyrics,
                                             const float* notes,
                                             int note_count,
                                             float* output_audio,
                                             int* output_length);

// ============================================================================
// 실시간 스트리밍 (비동기 처리)
// ============================================================================

/**
 * @brief 실시간 스트리밍을 시작합니다
 *
 * @param engine 엔진 핸들
 * @param callback 오디오 데이터 콜백 함수
 * @param user_data 콜백에 전달할 사용자 데이터
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_start_streaming(LibEtudeEngine* engine,
                                          AudioStreamCallback callback,
                                          void* user_data);

/**
 * @brief 스트리밍 중에 텍스트를 추가합니다
 *
 * @param engine 엔진 핸들
 * @param text 합성할 텍스트
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_stream_text(LibEtudeEngine* engine, const char* text);

/**
 * @brief 실시간 스트리밍을 중지합니다
 *
 * @param engine 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_stop_streaming(LibEtudeEngine* engine);

// ============================================================================
// 성능 제어 및 모니터링
// ============================================================================

/**
 * @brief 품질 모드를 설정합니다
 *
 * @param engine 엔진 핸들
 * @param quality_mode 품질 모드
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_set_quality_mode(LibEtudeEngine* engine, QualityMode quality_mode);

/**
 * @brief GPU 가속을 활성화합니다
 *
 * @param engine 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_enable_gpu_acceleration(LibEtudeEngine* engine);

/**
 * @brief 성능 통계를 가져옵니다
 *
 * @param engine 엔진 핸들
 * @param stats 성능 통계를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_get_performance_stats(LibEtudeEngine* engine, PerformanceStats* stats);

// ============================================================================
// 확장 모델 관리
// ============================================================================

/**
 * @brief 확장 모델을 로드합니다
 *
 * @param engine 엔진 핸들
 * @param extension_path 확장 모델 파일 경로 (.lefx)
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_load_extension(LibEtudeEngine* engine, const char* extension_path);

/**
 * @brief 확장 모델을 언로드합니다
 *
 * @param engine 엔진 핸들
 * @param extension_id 확장 모델 ID
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_unload_extension(LibEtudeEngine* engine, int extension_id);

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief LibEtude 버전 문자열을 반환합니다
 *
 * @return 버전 문자열 (예: "1.0.0")
 */
LIBETUDE_API const char* libetude_get_version();

/**
 * @brief 지원되는 하드웨어 기능을 반환합니다
 *
 * @return 하드웨어 기능 플래그 (SIMD_* 플래그의 조합)
 */
LIBETUDE_API uint32_t libetude_get_hardware_features();

/**
 * @brief 마지막 오류 메시지를 반환합니다
 *
 * @return 오류 메시지 문자열
 */
LIBETUDE_API const char* libetude_get_last_error();

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_API_H