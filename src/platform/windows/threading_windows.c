/**
 * @file threading_windows.c
 * @brief Windows 스레딩 추상화 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows Thread API를 추상화된 인터페이스로 래핑합니다.
 */

#ifdef _WIN32

#include "libetude/platform/threading.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Windows 특화 구조체 정의
// ============================================================================

/**
 * @brief Windows 스레드 구조체
 */
typedef struct ETThread {
    HANDLE handle;                 /**< Windows 스레드 핸들 */
    DWORD thread_id;              /**< Windows 스레드 ID */
    ETThreadFunc func;            /**< 스레드 함수 */
    void* arg;                    /**< 스레드 인자 */
    void* result;                 /**< 스레드 결과 */
    ETThreadAttributes attributes; /**< 스레드 속성 */
    bool detached;                /**< 분리 상태 */
    bool terminated;              /**< 종료 상태 */
} ETThread;

/**
 * @brief Windows 뮤텍스 구조체
 */
typedef struct ETMutex {
    CRITICAL_SECTION cs;          /**< Windows Critical Section */
    ETMutexType type;             /**< 뮤텍스 타입 */
    DWORD owner_thread_id;        /**< 소유 스레드 ID */
    int lock_count;               /**< 잠금 카운트 (재귀용) */
} ETMutex;

/**
 * @brief Windows 세마포어 구조체
 */
typedef struct ETSemaphore {
    HANDLE handle;                /**< Windows 세마포어 핸들 */
    int max_count;                /**< 최대 카운트 */
    char name[64];                /**< 세마포어 이름 */
} ETSemaphore;

/**
 * @brief Windows 조건변수 구조체
 */
typedef struct ETCondition {
    CONDITION_VARIABLE cv;        /**< Windows 조건변수 */
    bool initialized;             /**< 초기화 상태 */
} ETCondition;

// ============================================================================
// 내부 함수들
// ============================================================================

/**
 * @brief Windows 스레드 래퍼 함수
 */
static unsigned __stdcall windows_thread_wrapper(void* arg) {
    ETThread* thread = (ETThread*)arg;
    if (!thread || !thread->func) {
        return 1;
    }

    // 스레드 이름 설정 (Windows 10 이상)
    if (thread->attributes.name[0] != '\0') {
        // SetThreadDescription은 Windows 10 1607 이상에서만 사용 가능
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        if (kernel32) {
            typedef HRESULT (WINAPI *SetThreadDescriptionFunc)(HANDLE, PCWSTR);
            SetThreadDescriptionFunc SetThreadDescriptionPtr =
                (SetThreadDescriptionFunc)GetProcAddress(kernel32, "SetThreadDescription");

            if (SetThreadDescriptionPtr) {
                wchar_t wide_name[64];
                MultiByteToWideChar(CP_UTF8, 0, thread->attributes.name, -1, wide_name, 64);
                SetThreadDescriptionPtr(GetCurrentThread(), wide_name);
            }
        }
    }

    // 스레드 함수 실행
    thread->result = thread->func(thread->arg);
    thread->terminated = true;

    return 0;
}

/**
 * @brief Windows 오류를 공통 오류로 변환
 */
static ETResult windows_error_to_et_result(DWORD error) {
    switch (error) {
        case ERROR_SUCCESS:
            return ET_SUCCESS;
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_HANDLE:
            return ET_ERROR_INVALID_PARAMETER;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return ET_ERROR_OUT_OF_MEMORY;
        case ERROR_ACCESS_DENIED:
            return ET_ERROR_ACCESS_DENIED;
        case ERROR_TIMEOUT:
        case WAIT_TIMEOUT:
            return ET_ERROR_TIMEOUT;
        case ERROR_ALREADY_EXISTS:
            return ET_ERROR_ALREADY_EXISTS;
        case ERROR_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
            return ET_ERROR_NOT_FOUND;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

/**
 * @brief ETThreadPriority를 Windows 우선순위로 변환
 */
static int et_priority_to_windows(ETThreadPriority priority) {
    switch (priority) {
        case ET_THREAD_PRIORITY_IDLE:
            return THREAD_PRIORITY_IDLE;
        case ET_THREAD_PRIORITY_LOW:
            return THREAD_PRIORITY_BELOW_NORMAL;
        case ET_THREAD_PRIORITY_NORMAL:
            return THREAD_PRIORITY_NORMAL;
        case ET_THREAD_PRIORITY_HIGH:
            return THREAD_PRIORITY_ABOVE_NORMAL;
        case ET_THREAD_PRIORITY_CRITICAL:
            return THREAD_PRIORITY_HIGHEST;
        default:
            return THREAD_PRIORITY_NORMAL;
    }
}

/**
 * @brief Windows 우선순위를 ETThreadPriority로 변환
 */
static ETThreadPriority windows_priority_to_et(int priority) {
    switch (priority) {
        case THREAD_PRIORITY_IDLE:
            return ET_THREAD_PRIORITY_IDLE;
        case THREAD_PRIORITY_BELOW_NORMAL:
            return ET_THREAD_PRIORITY_LOW;
        case THREAD_PRIORITY_NORMAL:
            return ET_THREAD_PRIORITY_NORMAL;
        case THREAD_PRIORITY_ABOVE_NORMAL:
            return ET_THREAD_PRIORITY_HIGH;
        case THREAD_PRIORITY_HIGHEST:
        case THREAD_PRIORITY_TIME_CRITICAL:
            return ET_THREAD_PRIORITY_CRITICAL;
        default:
            return ET_THREAD_PRIORITY_NORMAL;
    }
}

// ============================================================================
// 스레드 관리 함수들
// ============================================================================

static ETResult windows_create_thread(ETThread** thread, ETThreadFunc func, void* arg) {
    if (!thread || !func) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETThread* new_thread = (ETThread*)calloc(1, sizeof(ETThread));
    if (!new_thread) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 속성 설정
    et_thread_attributes_init(&new_thread->attributes);
    new_thread->func = func;
    new_thread->arg = arg;
    new_thread->detached = false;
    new_thread->terminated = false;

    // Windows 스레드 생성
    new_thread->handle = (HANDLE)_beginthreadex(
        NULL,                           // 보안 속성
        (unsigned)new_thread->attributes.stack_size,  // 스택 크기
        windows_thread_wrapper,         // 스레드 함수
        new_thread,                     // 인자
        0,                              // 생성 플래그
        (unsigned*)&new_thread->thread_id  // 스레드 ID
    );

    if (!new_thread->handle) {
        free(new_thread);
        return windows_error_to_et_result(GetLastError());
    }

    *thread = new_thread;
    return ET_SUCCESS;
}

static ETResult windows_create_thread_with_attributes(ETThread** thread, ETThreadFunc func,
                                                      void* arg, const ETThreadAttributes* attributes) {
    if (!thread || !func || !attributes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 속성 유효성 검증
    if (!et_thread_attributes_validate(attributes)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETThread* new_thread = (ETThread*)calloc(1, sizeof(ETThread));
    if (!new_thread) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 속성 복사
    new_thread->attributes = *attributes;
    new_thread->func = func;
    new_thread->arg = arg;
    new_thread->detached = attributes->detached;
    new_thread->terminated = false;

    // Windows 스레드 생성
    new_thread->handle = (HANDLE)_beginthreadex(
        NULL,                           // 보안 속성
        (unsigned)attributes->stack_size,  // 스택 크기
        windows_thread_wrapper,         // 스레드 함수
        new_thread,                     // 인자
        0,                              // 생성 플래그
        (unsigned*)&new_thread->thread_id  // 스레드 ID
    );

    if (!new_thread->handle) {
        free(new_thread);
        return windows_error_to_et_result(GetLastError());
    }

    // 우선순위 설정
    if (attributes->priority != ET_THREAD_PRIORITY_NORMAL) {
        SetThreadPriority(new_thread->handle, et_priority_to_windows(attributes->priority));
    }

    // CPU 친화성 설정
    if (attributes->cpu_affinity >= 0) {
        DWORD_PTR mask = 1ULL << attributes->cpu_affinity;
        SetThreadAffinityMask(new_thread->handle, mask);
    }

    *thread = new_thread;
    return ET_SUCCESS;
}

static ETResult windows_join_thread(ETThread* thread, void** result) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->detached) {
        return ET_ERROR_INVALID_OPERATION;
    }

    // 스레드 종료 대기
    DWORD wait_result = WaitForSingleObject(thread->handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return windows_error_to_et_result(GetLastError());
    }

    if (result) {
        *result = thread->result;
    }

    return ET_SUCCESS;
}

static ETResult windows_detach_thread(ETThread* thread) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->detached) {
        return ET_ERROR_INVALID_OPERATION;
    }

    thread->detached = true;
    return ET_SUCCESS;
}

static void windows_destroy_thread(ETThread* thread) {
    if (!thread) {
        return;
    }

    if (thread->handle) {
        CloseHandle(thread->handle);
    }

    free(thread);
}

// ============================================================================
// 스레드 속성 관리 함수들
// ============================================================================

static ETResult windows_set_thread_priority(ETThread* thread, ETThreadPriority priority) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int windows_priority = et_priority_to_windows(priority);
    if (!SetThreadPriority(thread->handle, windows_priority)) {
        return windows_error_to_et_result(GetLastError());
    }

    thread->attributes.priority = priority;
    return ET_SUCCESS;
}

static ETResult windows_get_thread_priority(ETThread* thread, ETThreadPriority* priority) {
    if (!thread || !priority) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int windows_priority = GetThreadPriority(thread->handle);
    if (windows_priority == THREAD_PRIORITY_ERROR_RETURN) {
        return windows_error_to_et_result(GetLastError());
    }

    *priority = windows_priority_to_et(windows_priority);
    return ET_SUCCESS;
}

static ETResult windows_set_thread_affinity(ETThread* thread, int cpu_id) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    DWORD_PTR mask;
    if (cpu_id < 0) {
        // 모든 CPU에서 실행 가능
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        mask = (1ULL << sys_info.dwNumberOfProcessors) - 1;
    } else {
        mask = 1ULL << cpu_id;
    }

    if (!SetThreadAffinityMask(thread->handle, mask)) {
        return windows_error_to_et_result(GetLastError());
    }

    thread->attributes.cpu_affinity = cpu_id;
    return ET_SUCCESS;
}

static ETResult windows_get_current_thread_id(ETThreadID* id) {
    if (!id) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *id = (ETThreadID)GetCurrentThreadId();
    return ET_SUCCESS;
}

static ETResult windows_get_thread_state(ETThread* thread, ETThreadState* state) {
    if (!thread || !state) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->terminated) {
        *state = ET_THREAD_STATE_TERMINATED;
        return ET_SUCCESS;
    }

    DWORD wait_result = WaitForSingleObject(thread->handle, 0);
    switch (wait_result) {
        case WAIT_OBJECT_0:
            *state = ET_THREAD_STATE_TERMINATED;
            thread->terminated = true;
            break;
        case WAIT_TIMEOUT:
            *state = ET_THREAD_STATE_RUNNING;
            break;
        default:
            return windows_error_to_et_result(GetLastError());
    }

    return ET_SUCCESS;
}

static ETResult windows_sleep(uint32_t milliseconds) {
    Sleep(milliseconds);
    return ET_SUCCESS;
}

static ETResult windows_yield(void) {
    if (!SwitchToThread()) {
        Sleep(0);  // 대안으로 Sleep(0) 사용
    }
    return ET_SUCCESS;
}

// ============================================================================
// 뮤텍스 관리 함수들
// ============================================================================

static ETResult windows_create_mutex(ETMutex** mutex) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETMutex* new_mutex = (ETMutex*)calloc(1, sizeof(ETMutex));
    if (!new_mutex) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    InitializeCriticalSection(&new_mutex->cs);
    new_mutex->type = ET_MUTEX_NORMAL;
    new_mutex->owner_thread_id = 0;
    new_mutex->lock_count = 0;

    *mutex = new_mutex;
    return ET_SUCCESS;
}

static ETResult windows_create_mutex_with_attributes(ETMutex** mutex, const ETMutexAttributes* attributes) {
    if (!mutex || !attributes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!et_mutex_attributes_validate(attributes)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETMutex* new_mutex = (ETMutex*)calloc(1, sizeof(ETMutex));
    if (!new_mutex) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 재귀 뮤텍스는 기본적으로 지원됨 (Critical Section)
    InitializeCriticalSection(&new_mutex->cs);
    new_mutex->type = attributes->type;
    new_mutex->owner_thread_id = 0;
    new_mutex->lock_count = 0;

    *mutex = new_mutex;
    return ET_SUCCESS;
}

static ETResult windows_lock_mutex(ETMutex* mutex) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    EnterCriticalSection(&mutex->cs);

    mutex->owner_thread_id = GetCurrentThreadId();
    mutex->lock_count++;

    return ET_SUCCESS;
}

static ETResult windows_try_lock_mutex(ETMutex* mutex) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!TryEnterCriticalSection(&mutex->cs)) {
        return ET_ERROR_BUSY;
    }

    mutex->owner_thread_id = GetCurrentThreadId();
    mutex->lock_count++;

    return ET_SUCCESS;
}

static ETResult windows_timed_lock_mutex(ETMutex* mutex, uint32_t timeout_ms) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // Windows Critical Section은 시간 제한 잠금을 직접 지원하지 않음
    // 폴링 방식으로 구현
    DWORD start_time = GetTickCount();
    DWORD end_time = start_time + timeout_ms;

    while (GetTickCount() < end_time) {
        if (TryEnterCriticalSection(&mutex->cs)) {
            mutex->owner_thread_id = GetCurrentThreadId();
            mutex->lock_count++;
            return ET_SUCCESS;
        }
        Sleep(1);  // 1ms 대기
    }

    return ET_ERROR_TIMEOUT;
}

static ETResult windows_unlock_mutex(ETMutex* mutex) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (mutex->owner_thread_id != GetCurrentThreadId()) {
        return ET_ERROR_INVALID_OPERATION;
    }

    mutex->lock_count--;
    if (mutex->lock_count == 0) {
        mutex->owner_thread_id = 0;
    }

    LeaveCriticalSection(&mutex->cs);
    return ET_SUCCESS;
}

static void windows_destroy_mutex(ETMutex* mutex) {
    if (!mutex) {
        return;
    }

    DeleteCriticalSection(&mutex->cs);
    free(mutex);
}

// ============================================================================
// 세마포어 관리 함수들
// ============================================================================

static ETResult windows_create_semaphore(ETSemaphore** semaphore, int initial_count) {
    if (!semaphore || initial_count < 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETSemaphore* new_sem = (ETSemaphore*)calloc(1, sizeof(ETSemaphore));
    if (!new_sem) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    new_sem->max_count = INT32_MAX;
    new_sem->name[0] = '\0';

    new_sem->handle = CreateSemaphoreA(
        NULL,           // 보안 속성
        initial_count,  // 초기 카운트
        new_sem->max_count,  // 최대 카운트
        NULL            // 이름 (익명)
    );

    if (!new_sem->handle) {
        free(new_sem);
        return windows_error_to_et_result(GetLastError());
    }

    *semaphore = new_sem;
    return ET_SUCCESS;
}

static ETResult windows_create_semaphore_with_attributes(ETSemaphore** semaphore, int initial_count,
                                                         const ETSemaphoreAttributes* attributes) {
    if (!semaphore || initial_count < 0 || !attributes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!et_semaphore_attributes_validate(attributes)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETSemaphore* new_sem = (ETSemaphore*)calloc(1, sizeof(ETSemaphore));
    if (!new_sem) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    new_sem->max_count = attributes->max_count;
    strncpy(new_sem->name, attributes->name, sizeof(new_sem->name) - 1);
    new_sem->name[sizeof(new_sem->name) - 1] = '\0';

    const char* name_ptr = (new_sem->name[0] != '\0') ? new_sem->name : NULL;

    new_sem->handle = CreateSemaphoreA(
        NULL,           // 보안 속성
        initial_count,  // 초기 카운트
        new_sem->max_count,  // 최대 카운트
        name_ptr        // 이름
    );

    if (!new_sem->handle) {
        free(new_sem);
        return windows_error_to_et_result(GetLastError());
    }

    *semaphore = new_sem;
    return ET_SUCCESS;
}

static ETResult windows_wait_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    DWORD wait_result = WaitForSingleObject(semaphore->handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return windows_error_to_et_result(GetLastError());
    }

    return ET_SUCCESS;
}

static ETResult windows_try_wait_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    DWORD wait_result = WaitForSingleObject(semaphore->handle, 0);
    switch (wait_result) {
        case WAIT_OBJECT_0:
            return ET_SUCCESS;
        case WAIT_TIMEOUT:
            return ET_ERROR_BUSY;
        default:
            return windows_error_to_et_result(GetLastError());
    }
}

static ETResult windows_timed_wait_semaphore(ETSemaphore* semaphore, uint32_t timeout_ms) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    DWORD wait_result = WaitForSingleObject(semaphore->handle, timeout_ms);
    switch (wait_result) {
        case WAIT_OBJECT_0:
            return ET_SUCCESS;
        case WAIT_TIMEOUT:
            return ET_ERROR_TIMEOUT;
        default:
            return windows_error_to_et_result(GetLastError());
    }
}

static ETResult windows_post_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!ReleaseSemaphore(semaphore->handle, 1, NULL)) {
        return windows_error_to_et_result(GetLastError());
    }

    return ET_SUCCESS;
}

static ETResult windows_get_semaphore_count(ETSemaphore* semaphore, int* count) {
    if (!semaphore || !count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // Windows에서는 세마포어 카운트를 직접 조회하는 API가 없음
    // ReleaseSemaphore로 이전 카운트를 얻을 수 있지만, 이는 카운트를 변경함
    // 따라서 정확한 구현이 어려움 - 근사치 반환
    *count = -1;  // 알 수 없음을 나타냄
    return ET_ERROR_NOT_IMPLEMENTED;
}

static void windows_destroy_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return;
    }

    if (semaphore->handle) {
        CloseHandle(semaphore->handle);
    }

    free(semaphore);
}

// ============================================================================
// 조건변수 관리 함수들
// ============================================================================

static ETResult windows_create_condition(ETCondition** condition) {
    if (!condition) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETCondition* new_cond = (ETCondition*)calloc(1, sizeof(ETCondition));
    if (!new_cond) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    InitializeConditionVariable(&new_cond->cv);
    new_cond->initialized = true;

    *condition = new_cond;
    return ET_SUCCESS;
}

static ETResult windows_create_condition_with_attributes(ETCondition** condition,
                                                         const ETConditionAttributes* attributes) {
    if (!condition || !attributes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!et_condition_attributes_validate(attributes)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // Windows 조건변수는 속성을 지원하지 않으므로 기본 생성과 동일
    return windows_create_condition(condition);
}

static ETResult windows_wait_condition(ETCondition* condition, ETMutex* mutex) {
    if (!condition || !mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!SleepConditionVariableCS(&condition->cv, &mutex->cs, INFINITE)) {
        return windows_error_to_et_result(GetLastError());
    }

    return ET_SUCCESS;
}

static ETResult windows_timed_wait_condition(ETCondition* condition, ETMutex* mutex, uint32_t timeout_ms) {
    if (!condition || !mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!SleepConditionVariableCS(&condition->cv, &mutex->cs, timeout_ms)) {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            return ET_ERROR_TIMEOUT;
        }
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_signal_condition(ETCondition* condition) {
    if (!condition) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    WakeConditionVariable(&condition->cv);
    return ET_SUCCESS;
}

static ETResult windows_broadcast_condition(ETCondition* condition) {
    if (!condition) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    WakeAllConditionVariable(&condition->cv);
    return ET_SUCCESS;
}

static void windows_destroy_condition(ETCondition* condition) {
    if (!condition) {
        return;
    }

    // Windows 조건변수는 명시적 정리가 필요 없음
    free(condition);
}

// ============================================================================
// Windows 스레딩 인터페이스 구조체
// ============================================================================

static ETThreadInterface g_windows_thread_interface = {
    // 스레드 관리
    .create_thread = windows_create_thread,
    .create_thread_with_attributes = windows_create_thread_with_attributes,
    .join_thread = windows_join_thread,
    .detach_thread = windows_detach_thread,
    .destroy_thread = windows_destroy_thread,

    // 스레드 속성 관리
    .set_thread_priority = windows_set_thread_priority,
    .get_thread_priority = windows_get_thread_priority,
    .set_thread_affinity = windows_set_thread_affinity,
    .get_current_thread_id = windows_get_current_thread_id,
    .get_thread_state = windows_get_thread_state,
    .sleep = windows_sleep,
    .yield = windows_yield,

    // 뮤텍스 관리
    .create_mutex = windows_create_mutex,
    .create_mutex_with_attributes = windows_create_mutex_with_attributes,
    .lock_mutex = windows_lock_mutex,
    .try_lock_mutex = windows_try_lock_mutex,
    .timed_lock_mutex = windows_timed_lock_mutex,
    .unlock_mutex = windows_unlock_mutex,
    .destroy_mutex = windows_destroy_mutex,

    // 세마포어 관리
    .create_semaphore = windows_create_semaphore,
    .create_semaphore_with_attributes = windows_create_semaphore_with_attributes,
    .wait_semaphore = windows_wait_semaphore,
    .try_wait_semaphore = windows_try_wait_semaphore,
    .timed_wait_semaphore = windows_timed_wait_semaphore,
    .post_semaphore = windows_post_semaphore,
    .get_semaphore_count = windows_get_semaphore_count,
    .destroy_semaphore = windows_destroy_semaphore,

    // 조건변수 관리
    .create_condition = windows_create_condition,
    .create_condition_with_attributes = windows_create_condition_with_attributes,
    .wait_condition = windows_wait_condition,
    .timed_wait_condition = windows_timed_wait_condition,
    .signal_condition = windows_signal_condition,
    .broadcast_condition = windows_broadcast_condition,
    .destroy_condition = windows_destroy_condition,

    // 플랫폼별 확장 데이터
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief Windows 스레딩 인터페이스를 가져옵니다
 * @return Windows 스레딩 인터페이스 포인터
 */
ETThreadInterface* et_get_windows_thread_interface(void) {
    return &g_windows_thread_interface;
}

/**
 * @brief Windows 스레딩 인터페이스를 생성합니다
 * @param interface 생성된 인터페이스 포인터 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_windows_thread_interface(ETThreadInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *interface = &g_windows_thread_interface;
    return ET_SUCCESS;
}

/**
 * @brief Windows 스레딩 인터페이스를 해제합니다
 * @param interface 해제할 인터페이스
 */
void et_destroy_windows_thread_interface(ETThreadInterface* interface) {
    // 정적 인터페이스이므로 별도 정리 작업 없음
    (void)interface;
}

#endif // _WIN32