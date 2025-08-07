/**
 * @file threading_common.c
 * @brief 스레딩 추상화 레이어 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼에 관계없이 공통으로 사용되는 스레딩 관련 함수들을 구현합니다.
 */

#include "libetude/platform/threading.h"
#include "libetude/platform/common.h"
#include <string.h>

// ============================================================================
// 속성 초기화 함수들
// ============================================================================

void et_thread_attributes_init(ETThreadAttributes* attributes) {
    if (!attributes) {
        return;
    }

    memset(attributes, 0, sizeof(ETThreadAttributes));

    // 기본값 설정
    attributes->priority = ET_THREAD_PRIORITY_NORMAL;
    attributes->stack_size = 0;  // 시스템 기본값 사용
    attributes->cpu_affinity = -1;  // CPU 친화성 제한 없음
    attributes->detached = false;
    strncpy(attributes->name, "ETThread", sizeof(attributes->name) - 1);
    attributes->name[sizeof(attributes->name) - 1] = '\0';
}

void et_mutex_attributes_init(ETMutexAttributes* attributes) {
    if (!attributes) {
        return;
    }

    memset(attributes, 0, sizeof(ETMutexAttributes));

    // 기본값 설정
    attributes->type = ET_MUTEX_NORMAL;
    attributes->shared = false;  // 프로세스 내부에서만 사용
}

void et_semaphore_attributes_init(ETSemaphoreAttributes* attributes) {
    if (!attributes) {
        return;
    }

    memset(attributes, 0, sizeof(ETSemaphoreAttributes));

    // 기본값 설정
    attributes->max_count = INT32_MAX;  // 최대 카운트 제한 없음
    attributes->shared = false;  // 프로세스 내부에서만 사용
    strncpy(attributes->name, "", sizeof(attributes->name) - 1);
    attributes->name[sizeof(attributes->name) - 1] = '\0';
}

void et_condition_attributes_init(ETConditionAttributes* attributes) {
    if (!attributes) {
        return;
    }

    memset(attributes, 0, sizeof(ETConditionAttributes));

    // 기본값 설정
    attributes->shared = false;  // 프로세스 내부에서만 사용
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * @brief 스레드 우선순위를 문자열로 변환합니다
 * @param priority 스레드 우선순위
 * @return 우선순위 문자열
 */
const char* et_thread_priority_to_string(ETThreadPriority priority) {
    switch (priority) {
        case ET_THREAD_PRIORITY_IDLE:
            return "IDLE";
        case ET_THREAD_PRIORITY_LOW:
            return "LOW";
        case ET_THREAD_PRIORITY_NORMAL:
            return "NORMAL";
        case ET_THREAD_PRIORITY_HIGH:
            return "HIGH";
        case ET_THREAD_PRIORITY_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 스레드 상태를 문자열로 변환합니다
 * @param state 스레드 상태
 * @return 상태 문자열
 */
const char* et_thread_state_to_string(ETThreadState state) {
    switch (state) {
        case ET_THREAD_STATE_CREATED:
            return "CREATED";
        case ET_THREAD_STATE_RUNNING:
            return "RUNNING";
        case ET_THREAD_STATE_SUSPENDED:
            return "SUSPENDED";
        case ET_THREAD_STATE_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 뮤텍스 타입을 문자열로 변환합니다
 * @param type 뮤텍스 타입
 * @return 타입 문자열
 */
const char* et_mutex_type_to_string(ETMutexType type) {
    switch (type) {
        case ET_MUTEX_NORMAL:
            return "NORMAL";
        case ET_MUTEX_RECURSIVE:
            return "RECURSIVE";
        case ET_MUTEX_TIMED:
            return "TIMED";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// 오류 처리 함수들
// ============================================================================

/**
 * @brief 스레딩 관련 플랫폼 오류를 공통 오류로 매핑합니다
 * @param platform_error 플랫폼별 오류 코드
 * @param platform_type 플랫폼 타입
 * @return 공통 오류 코드
 */
ETResult et_threading_map_platform_error(int platform_error, ETPlatformType platform_type) {
    // 플랫폼별 오류 매핑은 각 플랫폼 구현에서 처리
    // 여기서는 기본적인 매핑만 제공

    if (platform_error == 0) {
        return ET_SUCCESS;
    }

    // 일반적인 오류 코드들
    switch (platform_error) {
        case 11:  // EAGAIN (리소스 일시적으로 사용 불가)
        case 16:  // EBUSY (리소스 사용 중)
            return ET_ERROR_BUSY;

        case 12:  // ENOMEM (메모리 부족)
            return ET_ERROR_OUT_OF_MEMORY;

        case 22:  // EINVAL (잘못된 인자)
            return ET_ERROR_INVALID_PARAMETER;

        case 110: // ETIMEDOUT (타임아웃)
            return ET_ERROR_TIMEOUT;

        case 1:   // EPERM (권한 없음)
        case 13:  // EACCES (접근 거부)
            return ET_ERROR_ACCESS_DENIED;

        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

// ============================================================================
// 디버깅 및 로깅 함수들
// ============================================================================

/**
 * @brief 스레드 정보를 로그로 출력합니다 (디버그용)
 * @param thread_id 스레드 ID
 * @param message 메시지
 */
void et_thread_log_debug(ETThreadID thread_id, const char* message) {
    #ifdef ET_DEBUG_THREADING
    printf("[THREAD DEBUG] Thread %llu: %s\n", (unsigned long long)thread_id, message);
    #endif
}

/**
 * @brief 뮤텍스 작업을 로그로 출력합니다 (디버그용)
 * @param mutex 뮤텍스 포인터
 * @param operation 작업 이름
 * @param result 작업 결과
 */
void et_mutex_log_debug(const ETMutex* mutex, const char* operation, ETResult result) {
    #ifdef ET_DEBUG_THREADING
    printf("[MUTEX DEBUG] Mutex %p: %s -> %s\n",
           (const void*)mutex, operation,
           (result == ET_SUCCESS) ? "SUCCESS" : "FAILED");
    #endif
}

// ============================================================================
// 성능 측정 함수들
// ============================================================================

/**
 * @brief 스레드 생성 시간을 측정합니다 (프로파일링용)
 * @param start_time 시작 시간 (나노초)
 * @param end_time 종료 시간 (나노초)
 * @return 경과 시간 (마이크로초)
 */
uint64_t et_thread_measure_creation_time(uint64_t start_time, uint64_t end_time) {
    return (end_time - start_time) / 1000;  // 나노초를 마이크로초로 변환
}

/**
 * @brief 뮤텍스 잠금 대기 시간을 측정합니다 (프로파일링용)
 * @param start_time 시작 시간 (나노초)
 * @param end_time 종료 시간 (나노초)
 * @return 경과 시간 (마이크로초)
 */
uint64_t et_mutex_measure_lock_time(uint64_t start_time, uint64_t end_time) {
    return (end_time - start_time) / 1000;  // 나노초를 마이크로초로 변환
}

// ============================================================================
// 검증 함수들
// ============================================================================

/**
 * @brief 스레드 속성의 유효성을 검증합니다
 * @param attributes 검증할 속성
 * @return 유효하면 true, 아니면 false
 */
bool et_thread_attributes_validate(const ETThreadAttributes* attributes) {
    if (!attributes) {
        return false;
    }

    // 우선순위 범위 검증
    if (attributes->priority < ET_THREAD_PRIORITY_IDLE ||
        attributes->priority > ET_THREAD_PRIORITY_CRITICAL) {
        return false;
    }

    // 스택 크기 검증 (0은 기본값으로 허용)
    if (attributes->stack_size > 0 && attributes->stack_size < 4096) {
        return false;  // 최소 4KB
    }

    // CPU 친화성 검증 (-1은 제한 없음으로 허용)
    if (attributes->cpu_affinity < -1) {
        return false;
    }

    return true;
}

/**
 * @brief 뮤텍스 속성의 유효성을 검증합니다
 * @param attributes 검증할 속성
 * @return 유효하면 true, 아니면 false
 */
bool et_mutex_attributes_validate(const ETMutexAttributes* attributes) {
    if (!attributes) {
        return false;
    }

    // 뮤텍스 타입 검증
    if (attributes->type < ET_MUTEX_NORMAL || attributes->type > ET_MUTEX_TIMED) {
        return false;
    }

    return true;
}

/**
 * @brief 세마포어 속성의 유효성을 검증합니다
 * @param attributes 검증할 속성
 * @return 유효하면 true, 아니면 false
 */
bool et_semaphore_attributes_validate(const ETSemaphoreAttributes* attributes) {
    if (!attributes) {
        return false;
    }

    // 최대 카운트 검증
    if (attributes->max_count <= 0) {
        return false;
    }

    return true;
}

/**
 * @brief 조건변수 속성의 유효성을 검증합니다
 * @param attributes 검증할 속성
 * @return 유효하면 true, 아니면 false
 */
bool et_condition_attributes_validate(const ETConditionAttributes* attributes) {
    if (!attributes) {
        return false;
    }

    // 현재는 shared 플래그만 있으므로 항상 유효
    return true;
}