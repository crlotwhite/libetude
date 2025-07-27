/**
 * @file profiler.h
 * @brief LibEtude 성능 프로파일러 인터페이스
 *
 * 연산 시간 측정, 메모리 사용량 추적, CPU/GPU 사용률 모니터링을 제공합니다.
 */

#ifndef LIBETUDE_PROFILER_H
#define LIBETUDE_PROFILER_H

#include "libetude/types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 프로파일 항목 구조체
 *
 * 개별 연산의 성능 정보를 저장합니다.
 */
typedef struct {
    const char* op_name;        ///< 연산자 이름
    uint64_t start_time;        ///< 시작 시간 (나노초)
    uint64_t end_time;          ///< 종료 시간 (나노초)
    uint64_t cpu_cycles;        ///< CPU 사이클 수
    size_t memory_used;         ///< 사용된 메모리 (바이트)
    size_t memory_peak;         ///< 피크 메모리 (바이트)
    float cpu_usage;            ///< CPU 사용률 (0.0-1.0)
    float gpu_usage;            ///< GPU 사용률 (0.0-1.0)
} ProfileEntry;

/**
 * @brief 프로파일러 구조체
 *
 * 전체 프로파일링 세션의 정보를 관리합니다.
 */
typedef struct {
    ProfileEntry* entries;      ///< 프로파일 항목 배열
    int entry_count;            ///< 현재 항목 수
    int capacity;               ///< 최대 항목 수

    // 전체 통계
    uint64_t total_inference_time;  ///< 총 추론 시간 (나노초)
    uint64_t total_memory_peak;     ///< 총 피크 메모리 (바이트)
    float avg_cpu_usage;            ///< 평균 CPU 사용률
    float avg_gpu_usage;            ///< 평균 GPU 사용률

    // 프로파일링 상태
    bool is_profiling;              ///< 프로파일링 활성화 여부
    int active_profiles;            ///< 활성 프로파일 수

    // 시스템 정보
    uint64_t session_start_time;    ///< 세션 시작 시간
    uint64_t last_update_time;      ///< 마지막 업데이트 시간
} Profiler;

/**
 * @brief 프로파일러 생성
 *
 * @param capacity 최대 프로파일 항목 수
 * @return 생성된 프로파일러 포인터, 실패 시 NULL
 */
Profiler* rt_create_profiler(int capacity);

/**
 * @brief 프로파일러 해제
 *
 * @param profiler 해제할 프로파일러
 */
void rt_destroy_profiler(Profiler* profiler);

/**
 * @brief 프로파일링 시작
 *
 * @param profiler 프로파일러
 * @param name 연산 이름
 * @return 성공 시 ET_SUCCESS, 실패 시 에러 코드
 */
ETResult rt_start_profile(Profiler* profiler, const char* name);

/**
 * @brief 프로파일링 종료
 *
 * @param profiler 프로파일러
 * @param name 연산 이름 (시작할 때와 동일해야 함)
 * @return 성공 시 ET_SUCCESS, 실패 시 에러 코드
 */
ETResult rt_end_profile(Profiler* profiler, const char* name);

/**
 * @brief 메모리 사용량 업데이트
 *
 * @param profiler 프로파일러
 * @param name 연산 이름
 * @param memory_used 현재 메모리 사용량 (바이트)
 * @param memory_peak 피크 메모리 사용량 (바이트)
 */
void rt_update_memory_usage(Profiler* profiler, const char* name,
                           size_t memory_used, size_t memory_peak);

/**
 * @brief CPU/GPU 사용률 업데이트
 *
 * @param profiler 프로파일러
 * @param cpu_usage CPU 사용률 (0.0-1.0)
 * @param gpu_usage GPU 사용률 (0.0-1.0)
 */
void rt_update_resource_usage(Profiler* profiler, float cpu_usage, float gpu_usage);

/**
 * @brief 프로파일링 리포트 생성
 *
 * JSON 형태로 구조화된 성능 리포트를 생성합니다.
 *
 * @param profiler 프로파일러
 * @param output_path 출력 파일 경로
 * @return 성공 시 ET_SUCCESS, 실패 시 에러 코드
 */
ETResult rt_generate_report(Profiler* profiler, const char* output_path);

/**
 * @brief 프로파일링 통계 조회
 *
 * @param profiler 프로파일러
 * @param op_name 연산 이름 (NULL이면 전체 통계)
 * @return 프로파일 항목 포인터, 없으면 NULL
 */
const ProfileEntry* rt_get_profile_stats(Profiler* profiler, const char* op_name);

/**
 * @brief 프로파일링 데이터 초기화
 *
 * @param profiler 프로파일러
 */
void rt_reset_profiler(Profiler* profiler);

/**
 * @brief 프로파일링 활성화/비활성화
 *
 * @param profiler 프로파일러
 * @param enable true면 활성화, false면 비활성화
 */
void rt_enable_profiling(Profiler* profiler, bool enable);

/**
 * @brief 현재 시간 조회 (나노초)
 *
 * @return 현재 시간 (나노초)
 */
uint64_t rt_get_current_time_ns(void);

/**
 * @brief CPU 사이클 수 조회
 *
 * @return 현재 CPU 사이클 수
 */
uint64_t rt_get_cpu_cycles(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PROFILER_H