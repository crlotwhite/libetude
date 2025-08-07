/**
 * @file threading.h
 * @brief 스레딩 추상화 레이어 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼에 관계없이 일관된 스레딩 및 동기화 API를 제공합니다.
 * Windows Thread API와 POSIX pthread의 차이를 추상화합니다.
 */

#ifndef LIBETUDE_PLATFORM_THREADING_H
#define LIBETUDE_PLATFORM_THREADING_H

#include "libetude/platform/common.h"
#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 스레딩 관련 타입 정의
// ============================================================================

/**
 * @brief 스레드 핸들 (불투명 타입)
 */
typedef struct ETThread ETThread;

/**
 * @brief 뮤텍스 핸들 (불투명 타입)
 */
typedef struct ETMutex ETMutex;

/**
 * @brief 세마포어 핸들 (불투명 타입)
 */
typedef struct ETSemaphore ETSemaphore;

/**
 * @brief 조건변수 핸들 (불투명 타입)
 */
typedef struct ETCondition ETCondition;

/**
 * @brief 스레드 ID 타입
 */
typedef uint64_t ETThreadID;

/**
 * @brief 스레드 함수 타입
 * @param arg 스레드에 전달된 인자
 * @return 스레드 종료 값
 */
typedef void* (*ETThreadFunc)(void* arg);

/**
 * @brief 스레드 우선순위 열거형
 */
typedef enum {
    ET_THREAD_PRIORITY_IDLE = -2,      /**< 유휴 우선순위 */
    ET_THREAD_PRIORITY_LOW = -1,       /**< 낮은 우선순위 */
    ET_THREAD_PRIORITY_NORMAL = 0,     /**< 보통 우선순위 (기본값) */
    ET_THREAD_PRIORITY_HIGH = 1,       /**< 높은 우선순위 */
    ET_THREAD_PRIORITY_CRITICAL = 2    /**< 중요 우선순위 */
} ETThreadPriority;

/**
 * @brief 스레드 상태 열거형
 */
typedef enum {
    ET_THREAD_STATE_CREATED = 0,       /**< 생성됨 */
    ET_THREAD_STATE_RUNNING = 1,       /**< 실행 중 */
    ET_THREAD_STATE_SUSPENDED = 2,     /**< 일시정지 */
    ET_THREAD_STATE_TERMINATED = 3     /**< 종료됨 */
} ETThreadState;

/**
 * @brief 스레드 속성 구조체
 */
typedef struct {
    ETThreadPriority priority;         /**< 스레드 우선순위 */
    size_t stack_size;                 /**< 스택 크기 (0이면 기본값) */
    int cpu_affinity;                  /**< CPU 친화성 (-1이면 제한 없음) */
    bool detached;                     /**< 분리된 스레드 여부 */
    char name[64];                     /**< 스레드 이름 (디버깅용) */
} ETThreadAttributes;

/**
 * @brief 뮤텍스 타입 열거형
 */
typedef enum {
    ET_MUTEX_NORMAL = 0,               /**< 일반 뮤텍스 */
    ET_MUTEX_RECURSIVE = 1,            /**< 재귀 뮤텍스 */
    ET_MUTEX_TIMED = 2                 /**< 시간 제한 뮤텍스 */
} ETMutexType;

/**
 * @brief 뮤텍스 속성 구조체
 */
typedef struct {
    ETMutexType type;                  /**< 뮤텍스 타입 */
    bool shared;                       /**< 프로세스 간 공유 여부 */
} ETMutexAttributes;

/**
 * @brief 세마포어 속성 구조체
 */
typedef struct {
    int max_count;                     /**< 최대 카운트 */
    bool shared;                       /**< 프로세스 간 공유 여부 */
    char name[64];                     /**< 세마포어 이름 (명명된 세마포어용) */
} ETSemaphoreAttributes;

/**
 * @brief 조건변수 속성 구조체
 */
typedef struct {
    bool shared;                       /**< 프로세스 간 공유 여부 */
} ETConditionAttributes;

// ============================================================================
// 스레딩 인터페이스 구조체
// ============================================================================

/**
 * @brief 스레딩 추상화 인터페이스
 */
typedef struct ETThreadInterface {
    // ========================================================================
    // 스레드 관리
    // ========================================================================

    /**
     * @brief 스레드를 생성합니다
     * @param thread 생성된 스레드 핸들 (출력)
     * @param func 스레드 함수
     * @param arg 스레드 함수에 전달할 인자
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_thread)(ETThread** thread, ETThreadFunc func, void* arg);

    /**
     * @brief 속성을 지정하여 스레드를 생성합니다
     * @param thread 생성된 스레드 핸들 (출력)
     * @param func 스레드 함수
     * @param arg 스레드 함수에 전달할 인자
     * @param attributes 스레드 속성
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_thread_with_attributes)(ETThread** thread, ETThreadFunc func,
                                              void* arg, const ETThreadAttributes* attributes);

    /**
     * @brief 스레드가 종료될 때까지 대기합니다
     * @param thread 대기할 스레드
     * @param result 스레드 종료 값 (출력, NULL 가능)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*join_thread)(ETThread* thread, void** result);

    /**
     * @brief 스레드를 분리합니다 (자동 정리)
     * @param thread 분리할 스레드
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*detach_thread)(ETThread* thread);

    /**
     * @brief 스레드 핸들을 해제합니다
     * @param thread 해제할 스레드 핸들
     */
    void (*destroy_thread)(ETThread* thread);

    // ========================================================================
    // 스레드 속성 관리
    // ========================================================================

    /**
     * @brief 스레드 우선순위를 설정합니다
     * @param thread 대상 스레드
     * @param priority 설정할 우선순위
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_thread_priority)(ETThread* thread, ETThreadPriority priority);

    /**
     * @brief 스레드 우선순위를 가져옵니다
     * @param thread 대상 스레드
     * @param priority 우선순위 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_thread_priority)(ETThread* thread, ETThreadPriority* priority);

    /**
     * @brief 스레드 CPU 친화성을 설정합니다
     * @param thread 대상 스레드
     * @param cpu_id CPU ID (-1이면 제한 해제)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_thread_affinity)(ETThread* thread, int cpu_id);

    /**
     * @brief 현재 스레드 ID를 가져옵니다
     * @param id 스레드 ID (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_current_thread_id)(ETThreadID* id);

    /**
     * @brief 스레드 상태를 가져옵니다
     * @param thread 대상 스레드
     * @param state 스레드 상태 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_thread_state)(ETThread* thread, ETThreadState* state);

    /**
     * @brief 현재 스레드를 지정된 시간만큼 대기시킵니다
     * @param milliseconds 대기 시간 (밀리초)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*sleep)(uint32_t milliseconds);

    /**
     * @brief 현재 스레드가 다른 스레드에게 실행 권한을 양보합니다
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*yield)(void);

    // ========================================================================
    // 뮤텍스 (Mutex) 관리
    // ========================================================================

    /**
     * @brief 뮤텍스를 생성합니다
     * @param mutex 생성된 뮤텍스 핸들 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_mutex)(ETMutex** mutex);

    /**
     * @brief 속성을 지정하여 뮤텍스를 생성합니다
     * @param mutex 생성된 뮤텍스 핸들 (출력)
     * @param attributes 뮤텍스 속성
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_mutex_with_attributes)(ETMutex** mutex, const ETMutexAttributes* attributes);

    /**
     * @brief 뮤텍스를 잠급니다 (블로킹)
     * @param mutex 잠글 뮤텍스
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*lock_mutex)(ETMutex* mutex);

    /**
     * @brief 뮤텍스 잠금을 시도합니다 (논블로킹)
     * @param mutex 잠글 뮤텍스
     * @return 성공시 ET_SUCCESS, 이미 잠겨있으면 ET_ERROR_BUSY, 실패시 오류 코드
     */
    ETResult (*try_lock_mutex)(ETMutex* mutex);

    /**
     * @brief 시간 제한으로 뮤텍스 잠금을 시도합니다
     * @param mutex 잠글 뮤텍스
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 성공시 ET_SUCCESS, 타임아웃시 ET_ERROR_TIMEOUT, 실패시 오류 코드
     */
    ETResult (*timed_lock_mutex)(ETMutex* mutex, uint32_t timeout_ms);

    /**
     * @brief 뮤텍스 잠금을 해제합니다
     * @param mutex 해제할 뮤텍스
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*unlock_mutex)(ETMutex* mutex);

    /**
     * @brief 뮤텍스를 해제합니다
     * @param mutex 해제할 뮤텍스
     */
    void (*destroy_mutex)(ETMutex* mutex);

    // ========================================================================
    // 세마포어 (Semaphore) 관리
    // ========================================================================

    /**
     * @brief 세마포어를 생성합니다
     * @param semaphore 생성된 세마포어 핸들 (출력)
     * @param initial_count 초기 카운트
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_semaphore)(ETSemaphore** semaphore, int initial_count);

    /**
     * @brief 속성을 지정하여 세마포어를 생성합니다
     * @param semaphore 생성된 세마포어 핸들 (출력)
     * @param initial_count 초기 카운트
     * @param attributes 세마포어 속성
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_semaphore_with_attributes)(ETSemaphore** semaphore, int initial_count,
                                                 const ETSemaphoreAttributes* attributes);

    /**
     * @brief 세마포어를 대기합니다 (블로킹)
     * @param semaphore 대기할 세마포어
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*wait_semaphore)(ETSemaphore* semaphore);

    /**
     * @brief 세마포어 대기를 시도합니다 (논블로킹)
     * @param semaphore 대기할 세마포어
     * @return 성공시 ET_SUCCESS, 사용 불가시 ET_ERROR_BUSY, 실패시 오류 코드
     */
    ETResult (*try_wait_semaphore)(ETSemaphore* semaphore);

    /**
     * @brief 시간 제한으로 세마포어 대기를 시도합니다
     * @param semaphore 대기할 세마포어
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 성공시 ET_SUCCESS, 타임아웃시 ET_ERROR_TIMEOUT, 실패시 오류 코드
     */
    ETResult (*timed_wait_semaphore)(ETSemaphore* semaphore, uint32_t timeout_ms);

    /**
     * @brief 세마포어를 신호합니다 (카운트 증가)
     * @param semaphore 신호할 세마포어
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*post_semaphore)(ETSemaphore* semaphore);

    /**
     * @brief 세마포어 카운트를 가져옵니다
     * @param semaphore 대상 세마포어
     * @param count 현재 카운트 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_semaphore_count)(ETSemaphore* semaphore, int* count);

    /**
     * @brief 세마포어를 해제합니다
     * @param semaphore 해제할 세마포어
     */
    void (*destroy_semaphore)(ETSemaphore* semaphore);

    // ========================================================================
    // 조건변수 (Condition Variable) 관리
    // ========================================================================

    /**
     * @brief 조건변수를 생성합니다
     * @param condition 생성된 조건변수 핸들 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_condition)(ETCondition** condition);

    /**
     * @brief 속성을 지정하여 조건변수를 생성합니다
     * @param condition 생성된 조건변수 핸들 (출력)
     * @param attributes 조건변수 속성
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_condition_with_attributes)(ETCondition** condition,
                                                 const ETConditionAttributes* attributes);

    /**
     * @brief 조건변수를 대기합니다 (뮤텍스와 함께)
     * @param condition 대기할 조건변수
     * @param mutex 연관된 뮤텍스
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*wait_condition)(ETCondition* condition, ETMutex* mutex);

    /**
     * @brief 시간 제한으로 조건변수를 대기합니다
     * @param condition 대기할 조건변수
     * @param mutex 연관된 뮤텍스
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 성공시 ET_SUCCESS, 타임아웃시 ET_ERROR_TIMEOUT, 실패시 오류 코드
     */
    ETResult (*timed_wait_condition)(ETCondition* condition, ETMutex* mutex, uint32_t timeout_ms);

    /**
     * @brief 조건변수를 신호합니다 (하나의 대기 스레드 깨움)
     * @param condition 신호할 조건변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*signal_condition)(ETCondition* condition);

    /**
     * @brief 조건변수를 브로드캐스트합니다 (모든 대기 스레드 깨움)
     * @param condition 브로드캐스트할 조건변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*broadcast_condition)(ETCondition* condition);

    /**
     * @brief 조건변수를 해제합니다
     * @param condition 해제할 조건변수
     */
    void (*destroy_condition)(ETCondition* condition);

    // ========================================================================
    // 플랫폼별 확장 데이터
    // ========================================================================

    /**
     * @brief 플랫폼별 확장 데이터
     */
    void* platform_data;

} ETThreadInterface;

// ============================================================================
// 편의 함수 및 매크로
// ============================================================================

/**
 * @brief 기본 스레드 속성을 초기화합니다
 * @param attributes 초기화할 속성 구조체
 */
void et_thread_attributes_init(ETThreadAttributes* attributes);

/**
 * @brief 기본 뮤텍스 속성을 초기화합니다
 * @param attributes 초기화할 속성 구조체
 */
void et_mutex_attributes_init(ETMutexAttributes* attributes);

/**
 * @brief 기본 세마포어 속성을 초기화합니다
 * @param attributes 초기화할 속성 구조체
 */
void et_semaphore_attributes_init(ETSemaphoreAttributes* attributes);

/**
 * @brief 기본 조건변수 속성을 초기화합니다
 * @param attributes 초기화할 속성 구조체
 */
void et_condition_attributes_init(ETConditionAttributes* attributes);

// ============================================================================
// 스레드 안전 매크로
// ============================================================================

/**
 * @brief 뮤텍스를 사용한 임계 영역 매크로
 * @param mutex_ptr 뮤텍스 포인터
 * @param code 실행할 코드 블록
 */
#define ET_MUTEX_SCOPE(mutex_ptr, code) \
    do { \
        ETResult _lock_result = et_thread_interface->lock_mutex(mutex_ptr); \
        if (_lock_result == ET_SUCCESS) { \
            code \
            et_thread_interface->unlock_mutex(mutex_ptr); \
        } \
    } while(0)

/**
 * @brief 현재 스레드 ID를 가져오는 편의 매크로
 */
#define ET_CURRENT_THREAD_ID() ({ \
    ETThreadID _id = 0; \
    et_thread_interface->get_current_thread_id(&_id); \
    _id; \
})

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_THREADING_H