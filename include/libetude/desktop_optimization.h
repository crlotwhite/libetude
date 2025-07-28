/**
 * @file desktop_optimization.h
 * @brief 데스크톱 환경 최적화 기능
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 데스크톱 환경에서의 멀티코어 최적화, GPU 가속 통합, 오디오 백엔드 최적화를 제공합니다.
 */

#ifndef LIBETUDE_DESKTOP_OPTIMIZATION_H
#define LIBETUDE_DESKTOP_OPTIMIZATION_H

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/hardware.h"
#include "libetude/task_scheduler.h"
#include "libetude/audio_io.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 멀티코어 최적화 구조체
// ============================================================================

/**
 * @brief 멀티코어 최적화 설정
 */
typedef struct {
    uint32_t worker_thread_count;       /**< 워커 스레드 수 */
    uint32_t audio_thread_priority;     /**< 오디오 스레드 우선순위 */
    uint32_t compute_thread_priority;   /**< 연산 스레드 우선순위 */
    bool enable_numa_optimization;      /**< NUMA 최적화 활성화 */
    bool enable_cpu_affinity;           /**< CPU 친화성 설정 활성화 */
    uint32_t cpu_affinity_mask;         /**< CPU 친화성 마스크 */
} LibEtudeMulticoreConfig;

/**
 * @brief 멀티코어 최적화 상태
 */
typedef struct {
    LibEtudeMulticoreConfig config;     /**< 설정 */
    ETTaskScheduler* scheduler;   /**< 작업 스케줄러 */
    pthread_t* worker_threads;          /**< 워커 스레드 배열 */
    bool* thread_active;                /**< 스레드 활성 상태 */
    uint32_t active_thread_count;       /**< 활성 스레드 수 */

    // 성능 통계
    uint64_t total_tasks_processed;     /**< 처리된 총 작업 수 */
    uint64_t avg_task_duration_us;      /**< 평균 작업 처리 시간 (마이크로초) */
    float cpu_utilization;              /**< CPU 사용률 */
} LibEtudeMulticoreOptimizer;

// ============================================================================
// GPU 가속 통합 구조체
// ============================================================================

/**
 * @brief GPU 가속 설정
 */
typedef struct {
    LibEtudeGPUBackend preferred_backend; /**< 선호하는 GPU 백엔드 */
    bool enable_mixed_precision;         /**< 혼합 정밀도 활성화 */
    bool enable_tensor_cores;            /**< 텐서 코어 활성화 (NVIDIA) */
    uint32_t gpu_memory_limit_mb;        /**< GPU 메모리 사용 제한 (MB) */
    float gpu_utilization_target;        /**< 목표 GPU 사용률 (0.0-1.0) */
} LibEtudeGPUAccelConfig;

/**
 * @brief GPU 가속 상태
 */
typedef struct {
    LibEtudeGPUAccelConfig config;       /**< 설정 */
    LibEtudeHardwareGPUInfo gpu_info;    /**< GPU 정보 */
    void* gpu_context;                   /**< GPU 컨텍스트 */
    void* command_queue;                 /**< 명령 큐 */

    // 메모리 관리
    void* gpu_memory_pool;               /**< GPU 메모리 풀 */
    size_t allocated_memory;             /**< 할당된 메모리 크기 */
    size_t peak_memory_usage;            /**< 최대 메모리 사용량 */

    // 성능 통계
    uint64_t gpu_kernel_executions;      /**< GPU 커널 실행 횟수 */
    uint64_t avg_kernel_duration_us;     /**< 평균 커널 실행 시간 */
    float gpu_utilization;               /**< GPU 사용률 */

    bool initialized;                    /**< 초기화 상태 */
} LibEtudeGPUAccelerator;

// ============================================================================
// 오디오 백엔드 최적화 구조체
// ============================================================================

/**
 * @brief 오디오 백엔드 최적화 설정
 */
typedef struct {
    uint32_t buffer_size_frames;         /**< 버퍼 크기 (프레임 단위) */
    uint32_t num_buffers;                /**< 버퍼 개수 */
    bool enable_exclusive_mode;          /**< 독점 모드 활성화 (Windows WASAPI) */
    bool enable_low_latency_mode;        /**< 저지연 모드 활성화 */
    uint32_t audio_thread_priority;      /**< 오디오 스레드 우선순위 */
    bool enable_audio_thread_affinity;   /**< 오디오 스레드 CPU 친화성 */
    uint32_t audio_cpu_affinity_mask;    /**< 오디오 CPU 친화성 마스크 */
} LibEtudeAudioBackendConfig;

/**
 * @brief 오디오 백엔드 최적화 상태
 */
typedef struct {
    LibEtudeAudioBackendConfig config;   /**< 설정 */
    ETAudioDevice* audio_device;   /**< 오디오 디바이스 */

    // 버퍼 관리
    float** audio_buffers;               /**< 오디오 버퍼 배열 */
    uint32_t current_buffer_index;       /**< 현재 버퍼 인덱스 */
    uint32_t frames_per_buffer;          /**< 버퍼당 프레임 수 */

    // 지연 시간 측정
    uint64_t total_latency_us;           /**< 총 지연 시간 (마이크로초) */
    uint64_t buffer_latency_us;          /**< 버퍼 지연 시간 */
    uint64_t processing_latency_us;      /**< 처리 지연 시간 */

    // 성능 통계
    uint64_t audio_callbacks_processed;  /**< 처리된 오디오 콜백 수 */
    uint64_t buffer_underruns;           /**< 버퍼 언더런 횟수 */
    uint64_t buffer_overruns;            /**< 버퍼 오버런 횟수 */
    float cpu_usage_audio_thread;        /**< 오디오 스레드 CPU 사용률 */

    bool initialized;                    /**< 초기화 상태 */
} LibEtudeAudioBackendOptimizer;

// ============================================================================
// 통합 데스크톱 최적화 구조체
// ============================================================================

/**
 * @brief 데스크톱 최적화 통합 관리자
 */
typedef struct {
    LibEtudeMulticoreOptimizer multicore;    /**< 멀티코어 최적화 */
    LibEtudeGPUAccelerator gpu_accel;        /**< GPU 가속 */
    LibEtudeAudioBackendOptimizer audio;     /**< 오디오 백엔드 최적화 */

    LibEtudeHardwareInfo hardware_info;      /**< 하드웨어 정보 */

    // 전체 성능 통계
    uint64_t total_inference_time_us;        /**< 총 추론 시간 */
    uint64_t total_audio_processing_time_us; /**< 총 오디오 처리 시간 */
    float overall_cpu_utilization;           /**< 전체 CPU 사용률 */
    float overall_memory_utilization;        /**< 전체 메모리 사용률 */

    bool initialized;                        /**< 초기화 상태 */
} LibEtudeDesktopOptimizer;

// ============================================================================
// 초기화 및 해제 함수
// ============================================================================

/**
 * @brief 데스크톱 최적화를 초기화합니다
 * @param optimizer 최적화 관리자
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_desktop_optimizer_init(LibEtudeDesktopOptimizer* optimizer);

/**
 * @brief 데스크톱 최적화를 해제합니다
 * @param optimizer 최적화 관리자
 */
LIBETUDE_API void libetude_desktop_optimizer_destroy(LibEtudeDesktopOptimizer* optimizer);

// ============================================================================
// 멀티코어 최적화 함수
// ============================================================================

/**
 * @brief 멀티코어 최적화를 초기화합니다
 * @param multicore 멀티코어 최적화 구조체
 * @param hardware_info 하드웨어 정보
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_multicore_optimizer_init(LibEtudeMulticoreOptimizer* multicore,
                                                                const LibEtudeHardwareInfo* hardware_info);

/**
 * @brief 멀티코어 최적화를 해제합니다
 * @param multicore 멀티코어 최적화 구조체
 */
LIBETUDE_API void libetude_multicore_optimizer_destroy(LibEtudeMulticoreOptimizer* multicore);

/**
 * @brief 최적 스레드 수를 자동 설정합니다
 * @param multicore 멀티코어 최적화 구조체
 * @param cpu_info CPU 정보
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_multicore_auto_configure(LibEtudeMulticoreOptimizer* multicore,
                                                                 const LibEtudeHardwareCPUInfo* cpu_info);

/**
 * @brief CPU 친화성을 설정합니다
 * @param multicore 멀티코어 최적화 구조체
 * @param affinity_mask CPU 친화성 마스크
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_multicore_set_cpu_affinity(LibEtudeMulticoreOptimizer* multicore,
                                                                   uint32_t affinity_mask);

// ============================================================================
// GPU 가속 함수
// ============================================================================

/**
 * @brief GPU 가속을 초기화합니다
 * @param gpu_accel GPU 가속 구조체
 * @param hardware_info 하드웨어 정보
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_gpu_accelerator_init(LibEtudeGPUAccelerator* gpu_accel,
                                                            const LibEtudeHardwareInfo* hardware_info);

/**
 * @brief GPU 가속을 해제합니다
 * @param gpu_accel GPU 가속 구조체
 */
LIBETUDE_API void libetude_gpu_accelerator_destroy(LibEtudeGPUAccelerator* gpu_accel);

/**
 * @brief GPU 메모리를 할당합니다
 * @param gpu_accel GPU 가속 구조체
 * @param size 할당할 크기 (바이트)
 * @return 할당된 GPU 메모리 포인터, 실패 시 NULL
 */
LIBETUDE_API void* libetude_gpu_allocate_memory(LibEtudeGPUAccelerator* gpu_accel, size_t size);

/**
 * @brief GPU 메모리를 해제합니다
 * @param gpu_accel GPU 가속 구조체
 * @param ptr 해제할 메모리 포인터
 */
LIBETUDE_API void libetude_gpu_free_memory(LibEtudeGPUAccelerator* gpu_accel, void* ptr);

/**
 * @brief GPU 커널을 실행합니다
 * @param gpu_accel GPU 가속 구조체
 * @param kernel_name 커널 이름
 * @param args 커널 인수 배열
 * @param grid_size 그리드 크기
 * @param block_size 블록 크기
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_gpu_execute_kernel(LibEtudeGPUAccelerator* gpu_accel,
                                                          const char* kernel_name,
                                                          void** args,
                                                          uint32_t grid_size,
                                                          uint32_t block_size);

// ============================================================================
// 오디오 백엔드 최적화 함수
// ============================================================================

/**
 * @brief 오디오 백엔드 최적화를 초기화합니다
 * @param audio_opt 오디오 백엔드 최적화 구조체
 * @param hardware_info 하드웨어 정보
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_audio_backend_optimizer_init(LibEtudeAudioBackendOptimizer* audio_opt,
                                                                     const LibEtudeHardwareInfo* hardware_info);

/**
 * @brief 오디오 백엔드 최적화를 해제합니다
 * @param audio_opt 오디오 백엔드 최적화 구조체
 */
LIBETUDE_API void libetude_audio_backend_optimizer_destroy(LibEtudeAudioBackendOptimizer* audio_opt);

/**
 * @brief 저지연 오디오 모드를 설정합니다
 * @param audio_opt 오디오 백엔드 최적화 구조체
 * @param enable 저지연 모드 활성화 여부
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_audio_set_low_latency_mode(LibEtudeAudioBackendOptimizer* audio_opt,
                                                                  bool enable);

/**
 * @brief 오디오 버퍼 크기를 최적화합니다
 * @param audio_opt 오디오 백엔드 최적화 구조체
 * @param target_latency_ms 목표 지연 시간 (밀리초)
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_audio_optimize_buffer_size(LibEtudeAudioBackendOptimizer* audio_opt,
                                                                   uint32_t target_latency_ms);

// ============================================================================
// 성능 모니터링 함수
// ============================================================================

/**
 * @brief 성능 통계를 업데이트합니다
 * @param optimizer 데스크톱 최적화 관리자
 */
LIBETUDE_API void libetude_desktop_optimizer_update_stats(LibEtudeDesktopOptimizer* optimizer);

/**
 * @brief 성능 통계를 출력합니다
 * @param optimizer 데스크톱 최적화 관리자
 */
LIBETUDE_API void libetude_desktop_optimizer_print_stats(const LibEtudeDesktopOptimizer* optimizer);

/**
 * @brief 성능 통계를 JSON 형태로 출력합니다
 * @param optimizer 데스크톱 최적화 관리자
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_desktop_optimizer_stats_to_json(const LibEtudeDesktopOptimizer* optimizer,
                                                                        char* buffer,
                                                                        size_t buffer_size);

// ============================================================================
// 자동 최적화 함수
// ============================================================================

/**
 * @brief 하드웨어에 따른 자동 최적화를 수행합니다
 * @param optimizer 데스크톱 최적화 관리자
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_desktop_optimizer_auto_optimize(LibEtudeDesktopOptimizer* optimizer);

/**
 * @brief 실시간 성능 조정을 수행합니다
 * @param optimizer 데스크톱 최적화 관리자
 * @param target_cpu_usage 목표 CPU 사용률 (0.0-1.0)
 * @param target_latency_ms 목표 지연 시간 (밀리초)
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_desktop_optimizer_adaptive_tuning(LibEtudeDesktopOptimizer* optimizer,
                                                                          float target_cpu_usage,
                                                                          uint32_t target_latency_ms);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_DESKTOP_OPTIMIZATION_H