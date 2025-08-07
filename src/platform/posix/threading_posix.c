/**
 * @file threading_posix.c
 * @brief POSIX 스레딩 추상화 구현 (Linux/macOS 공통)
 * @author LibEtude Project
 * @version 1.0.0
 *
 * pthread API를 추상화된 인터페이스로 래핑합니다.
 */

#if defined(__linux__) || defined(__APPLE__)

#include "libetude/platform/threading.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <dispatch/dispatch.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/sysinfo.h>
#endif

// ============================================================================
// POSIX 특화 구조체 정의
// ============================================================================

/**
 * @brief POSIX 스레드 구조체
 */
typedef struct ETThread {
    pthread_t handle;             /**< POSIX 스레드 핸들 */
    ETThreadFunc func;            /**< 스레드 함수 */
    void* arg;                    /**< 스레드 인자 */
    void* result;                 /**< 스레드 결과 */
    ETThreadAttributes attributes; /**< 스레드 속성 */
    bool detached;                /**< 분리 상태 */
    bool terminated;              /**< 종료 상태 */
    pthread_attr_t pthread_attr;  /**< POSIX 스레드 속성 */
} ETThread;

/**
 * @brief POSIX 뮤텍스 구조체
 */
typedef struct ETMutex {
    pthread_mutex_t mutex;        /**< POSIX 뮤텍스 */
    pthread_mutexattr_t attr;     /**< 뮤텍스 속성 */
    ETMutexType type;             /**< 뮤텍스 타입 */
    pthread_t owner_thread;       /**< 소유 스레드 */
    int lock_count;               /**< 잠금 카운트 (재귀용) */
    bool initialized;             /**< 초기화 상태 */
} ETMutex;

/**
 * @brief POSIX 세마포어 구조체
 */
typedef struct ETSemaphore {
#ifdef __APPLE__
    dispatch_semaphore_t sem;     /**< macOS dispatch 세마포어 */
#else
    sem_t sem;                    /**< POSIX 세마포어 */
#endif
    int max_count;                /**< 최대 카운트 */
    char name[64];                /**< 세마포어 이름 */
    bool named;                   /**< 명명된 세마포어 여부 */
} ETSemaphore;

/**
 * @brief POSIX 조건변수 구조체
 */
typedef struct ETCondition {
    pthread_cond_t cond;          /**< POSIX 조건변수 */
    pthread_condattr_t attr;      /**< 조건변수 속성 */
    bool initialized;             /**< 초기화 상태 */
} ETCondition;

// ============================================================================
// 내부 함수들
// ============================================================================

/**
 * @brief POSIX 스레드 래퍼 함수
 */
static void* posix_thread_wrapper(void* arg) {
    ETThread* thread = (ETThread*)arg;
    if (!thread || !thread->func) {
        return NULL;
    }

    // 스레드 이름 설정
    if (thread->attributes.name[0] != '\0') {
#ifdef __APPLE__
        pthread_setname_np(thread->attributes.name);
#elif defined(__linux__)
        pthread_setname_np(pthread_self(), thread->attributes.name);
#endif
    }

    // 스레드 함수 실행
    thread->result = thread->func(thread->arg);
    thread->terminated = true;

    return thread->result;
}

/**
 * @brief POSIX 오류를 공통 오류로 변환
 */
static ETResult posix_error_to_et_result(int error) {
    switch (error) {
        case 0:
            return ET_SUCCESS;
        case EINVAL:
            return ET_ERROR_INVALID_PARAMETER;
        case ENOMEM:
            return ET_ERROR_OUT_OF_MEMORY;
        case EACCES:
        case EPERM:
            return ET_ERROR_ACCESS_DENIED;
        case ETIMEDOUT:
            return ET_ERROR_TIMEOUT;
        case EBUSY:
        case EAGAIN:
            return ET_ERROR_BUSY;
        case EEXIST:
            return ET_ERROR_ALREADY_EXISTS;
        case ENOENT:
            return ET_ERROR_NOT_FOUND;
        case EDEADLK:
            return ET_ERROR_DEADLOCK;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

/**
 * @brief ETThreadPriority를 POSIX 우선순위로 변환
 */
static int et_priority_to_posix(ETThreadPriority priority) {
    // POSIX 스케줄링 정책에 따라 우선순위 범위가 다름
    int min_prio = sched_get_priority_min(SCHED_OTHER);
    int max_prio = sched_get_priority_max(SCHED_OTHER);
    int range = max_prio - min_prio;

    switch (priority) {
        case ET_THREAD_PRIORITY_IDLE:
            return min_prio;
        case ET_THREAD_PRIORITY_LOW:
            return min_prio + range / 4;
        case ET_THREAD_PRIORITY_NORMAL:
            return min_prio + range / 2;
        case ET_THREAD_PRIORITY_HIGH:
            return min_prio + (range * 3) / 4;
        case ET_THREAD_PRIORITY_CRITICAL:
            return max_prio;
        default:
            return min_prio + range / 2;
    }
}

/**
 * @brief POSIX 우선순위를 ETThreadPriority로 변환
 */
static ETThreadPriority posix_priority_to_et(int priority) {
    int min_prio = sched_get_priority_min(SCHED_OTHER);
    int max_prio = sched_get_priority_max(SCHED_OTHER);
    int range = max_prio - min_prio;

    if (priority <= min_prio + range / 8) {
        return ET_THREAD_PRIORITY_IDLE;
    } else if (priority <= min_prio + (range * 3) / 8) {
        return ET_THREAD_PRIORITY_LOW;
    } else if (priority <= min_prio + (range * 5) / 8) {
        return ET_THREAD_PRIORITY_NORMAL;
    } else if (priority <= min_prio + (range * 7) / 8) {
        return ET_THREAD_PRIORITY_HIGH;
    } else {
        return ET_THREAD_PRIORITY_CRITICAL;
    }
}

/**
 * @brief 현재 시간을 나노초로 가져옵니다
 */
static uint64_t get_current_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief 절대 시간을 계산합니다 (타임아웃용)
 */
static void calculate_absolute_time(struct timespec* abs_time, uint32_t timeout_ms) {
    clock_gettime(CLOCK_REALTIME, abs_time);
    abs_time->tv_sec += timeout_ms / 1000;
    abs_time->tv_nsec += (timeout_ms % 1000) * 1000000;

    if (abs_time->tv_nsec >= 1000000000) {
        abs_time->tv_sec++;
        abs_time->tv_nsec -= 1000000000;
    }
}

// ============================================================================
// 스레드 관리 함수들
// ============================================================================

static ETResult posix_create_thread(ETThread** thread, ETThreadFunc func, void* arg) {
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

    // POSIX 스레드 속성 초기화
    int result = pthread_attr_init(&new_thread->pthread_attr);
    if (result != 0) {
        free(new_thread);
        return posix_error_to_et_result(result);
    }

    // POSIX 스레드 생성
    result = pthread_create(&new_thread->handle, &new_thread->pthread_attr,
                           posix_thread_wrapper, new_thread);
    if (result != 0) {
        pthread_attr_destroy(&new_thread->pthread_attr);
        free(new_thread);
        return posix_error_to_et_result(result);
    }

    *thread = new_thread;
    return ET_SUCCESS;
}

static ETResult posix_create_thread_with_attributes(ETThread** thread, ETThreadFunc func,
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

    // POSIX 스레드 속성 초기화
    int result = pthread_attr_init(&new_thread->pthread_attr);
    if (result != 0) {
        free(new_thread);
        return posix_error_to_et_result(result);
    }

    // 스택 크기 설정
    if (attributes->stack_size > 0) {
        result = pthread_attr_setstacksize(&new_thread->pthread_attr, attributes->stack_size);
        if (result != 0) {
            pthread_attr_destroy(&new_thread->pthread_attr);
            free(new_thread);
            return posix_error_to_et_result(result);
        }
    }

    // 분리 상태 설정
    if (attributes->detached) {
        result = pthread_attr_setdetachstate(&new_thread->pthread_attr, PTHREAD_CREATE_DETACHED);
        if (result != 0) {
            pthread_attr_destroy(&new_thread->pthread_attr);
            free(new_thread);
            return posix_error_to_et_result(result);
        }
    }

    // POSIX 스레드 생성
    result = pthread_create(&new_thread->handle, &new_thread->pthread_attr,
                           posix_thread_wrapper, new_thread);
    if (result != 0) {
        pthread_attr_destroy(&new_thread->pthread_attr);
        free(new_thread);
        return posix_error_to_et_result(result);
    }

    // 우선순위 설정 (생성 후)
    if (attributes->priority != ET_THREAD_PRIORITY_NORMAL) {
        struct sched_param param;
        param.sched_priority = et_priority_to_posix(attributes->priority);
        pthread_setschedparam(new_thread->handle, SCHED_OTHER, &param);
    }

    // CPU 친화성 설정
    if (attributes->cpu_affinity >= 0) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(attributes->cpu_affinity, &cpuset);
        pthread_setaffinity_np(new_thread->handle, sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
        // macOS에서는 thread_policy_set 사용
        thread_affinity_policy_data_t policy = { attributes->cpu_affinity };
        thread_policy_set(pthread_mach_thread_np(new_thread->handle),
                          THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
#endif
    }

    *thread = new_thread;
    return ET_SUCCESS;
}

static ETResult posix_join_thread(ETThread* thread, void** result) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->detached) {
        return ET_ERROR_INVALID_OPERATION;
    }

    // 스레드 종료 대기
    void* thread_result;
    int posix_result = pthread_join(thread->handle, &thread_result);
    if (posix_result != 0) {
        return posix_error_to_et_result(posix_result);
    }

    if (result) {
        *result = thread_result;
    }

    return ET_SUCCESS;
}

static ETResult posix_detach_thread(ETThread* thread) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->detached) {
        return ET_ERROR_INVALID_OPERATION;
    }

    int result = pthread_detach(thread->handle);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    thread->detached = true;
    return ET_SUCCESS;
}

static void posix_destroy_thread(ETThread* thread) {
    if (!thread) {
        return;
    }

    pthread_attr_destroy(&thread->pthread_attr);
    free(thread);
}

// ============================================================================
// 스레드 속성 관리 함수들
// ============================================================================

static ETResult posix_set_thread_priority(ETThread* thread, ETThreadPriority priority) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    struct sched_param param;
    param.sched_priority = et_priority_to_posix(priority);

    int result = pthread_setschedparam(thread->handle, SCHED_OTHER, &param);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    thread->attributes.priority = priority;
    return ET_SUCCESS;
}

static ETResult posix_get_thread_priority(ETThread* thread, ETThreadPriority* priority) {
    if (!thread || !priority) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int policy;
    struct sched_param param;

    int result = pthread_getschedparam(thread->handle, &policy, &param);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    *priority = posix_priority_to_et(param.sched_priority);
    return ET_SUCCESS;
}

static ETResult posix_set_thread_affinity(ETThread* thread, int cpu_id) {
    if (!thread) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    if (cpu_id < 0) {
        // 모든 CPU에서 실행 가능
        int num_cpus = get_nprocs();
        for (int i = 0; i < num_cpus; i++) {
            CPU_SET(i, &cpuset);
        }
    } else {
        CPU_SET(cpu_id, &cpuset);
    }

    int result = pthread_setaffinity_np(thread->handle, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }
#elif defined(__APPLE__)
    if (cpu_id >= 0) {
        thread_affinity_policy_data_t policy = { cpu_id };
        kern_return_t result = thread_policy_set(pthread_mach_thread_np(thread->handle),
                                                 THREAD_AFFINITY_POLICY,
                                                 (thread_policy_t)&policy, 1);
        if (result != KERN_SUCCESS) {
            return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }
#endif

    thread->attributes.cpu_affinity = cpu_id;
    return ET_SUCCESS;
}

static ETResult posix_get_current_thread_id(ETThreadID* id) {
    if (!id) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __linux__
    *id = (ETThreadID)syscall(SYS_gettid);
#elif defined(__APPLE__)
    uint64_t thread_id;
    pthread_threadid_np(NULL, &thread_id);
    *id = (ETThreadID)thread_id;
#else
    *id = (ETThreadID)pthread_self();
#endif

    return ET_SUCCESS;
}

static ETResult posix_get_thread_state(ETThread* thread, ETThreadState* state) {
    if (!thread || !state) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (thread->terminated) {
        *state = ET_THREAD_STATE_TERMINATED;
        return ET_SUCCESS;
    }

    // POSIX에서는 스레드 상태를 직접 조회하는 표준 방법이 없음
    // pthread_kill을 사용하여 스레드 존재 여부 확인
    int result = pthread_kill(thread->handle, 0);
    if (result == 0) {
        *state = ET_THREAD_STATE_RUNNING;
    } else if (result == ESRCH) {
        *state = ET_THREAD_STATE_TERMINATED;
        thread->terminated = true;
    } else {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static ETResult posix_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;

    int result = nanosleep(&ts, NULL);
    if (result != 0) {
        return posix_error_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_yield(void) {
    int result = sched_yield();
    if (result != 0) {
        return posix_error_to_et_result(errno);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 뮤텍스 관리 함수들
// ============================================================================

static ETResult posix_create_mutex(ETMutex** mutex) {
    if (!mutex) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETMutex* new_mutex = (ETMutex*)calloc(1, sizeof(ETMutex));
    if (!new_mutex) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 뮤텍스 속성 초기화
    int result = pthread_mutexattr_init(&new_mutex->attr);
    if (result != 0) {
        free(new_mutex);
        return posix_error_to_et_result(result);
    }

    // 뮤텍스 초기화
    result = pthread_mutex_init(&new_mutex->mutex, &new_mutex->attr);
    if (result != 0) {
        pthread_mutexattr_destroy(&new_mutex->attr);
        free(new_mutex);
        return posix_error_to_et_result(result);
    }

    new_mutex->type = ET_MUTEX_NORMAL;
    new_mutex->owner_thread = 0;
    new_mutex->lock_count = 0;
    new_mutex->initialized = true;

    *mutex = new_mutex;
    return ET_SUCCESS;
}

static ETResult posix_create_mutex_with_attributes(ETMutex** mutex, const ETMutexAttributes* attributes) {
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

    // 뮤텍스 속성 초기화
    int result = pthread_mutexattr_init(&new_mutex->attr);
    if (result != 0) {
        free(new_mutex);
        return posix_error_to_et_result(result);
    }

    // 뮤텍스 타입 설정
    int mutex_type;
    switch (attributes->type) {
        case ET_MUTEX_NORMAL:
            mutex_type = PTHREAD_MUTEX_NORMAL;
            break;
        case ET_MUTEX_RECURSIVE:
            mutex_type = PTHREAD_MUTEX_RECURSIVE;
            break;
        case ET_MUTEX_TIMED:
            mutex_type = PTHREAD_MUTEX_NORMAL;  // POSIX에서는 별도 타입 없음
            break;
        default:
            mutex_type = PTHREAD_MUTEX_NORMAL;
            break;
    }

    result = pthread_mutexattr_settype(&new_mutex->attr, mutex_type);
    if (result != 0) {
        pthread_mutexattr_destroy(&new_mutex->attr);
        free(new_mutex);
        return posix_error_to_et_result(result);
    }

    // 프로세스 간 공유 설정
    if (attributes->shared) {
        result = pthread_mutexattr_setpshared(&new_mutex->attr, PTHREAD_PROCESS_SHARED);
        if (result != 0) {
            pthread_mutexattr_destroy(&new_mutex->attr);
            free(new_mutex);
            return posix_error_to_et_result(result);
        }
    }

    // 뮤텍스 초기화
    result = pthread_mutex_init(&new_mutex->mutex, &new_mutex->attr);
    if (result != 0) {
        pthread_mutexattr_destroy(&new_mutex->attr);
        free(new_mutex);
        return posix_error_to_et_result(result);
    }

    new_mutex->type = attributes->type;
    new_mutex->owner_thread = 0;
    new_mutex->lock_count = 0;
    new_mutex->initialized = true;

    *mutex = new_mutex;
    return ET_SUCCESS;
}

static ETResult posix_lock_mutex(ETMutex* mutex) {
    if (!mutex || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int result = pthread_mutex_lock(&mutex->mutex);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    mutex->owner_thread = pthread_self();
    mutex->lock_count++;

    return ET_SUCCESS;
}

static ETResult posix_try_lock_mutex(ETMutex* mutex) {
    if (!mutex || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int result = pthread_mutex_trylock(&mutex->mutex);
    if (result == EBUSY) {
        return ET_ERROR_BUSY;
    } else if (result != 0) {
        return posix_error_to_et_result(result);
    }

    mutex->owner_thread = pthread_self();
    mutex->lock_count++;

    return ET_SUCCESS;
}

static ETResult posix_timed_lock_mutex(ETMutex* mutex, uint32_t timeout_ms) {
    if (!mutex || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    struct timespec abs_time;
    calculate_absolute_time(&abs_time, timeout_ms);

    int result = pthread_mutex_timedlock(&mutex->mutex, &abs_time);
    if (result == ETIMEDOUT) {
        return ET_ERROR_TIMEOUT;
    } else if (result != 0) {
        return posix_error_to_et_result(result);
    }

    mutex->owner_thread = pthread_self();
    mutex->lock_count++;

    return ET_SUCCESS;
}

static ETResult posix_unlock_mutex(ETMutex* mutex) {
    if (!mutex || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!pthread_equal(mutex->owner_thread, pthread_self())) {
        return ET_ERROR_INVALID_OPERATION;
    }

    mutex->lock_count--;
    if (mutex->lock_count == 0) {
        mutex->owner_thread = 0;
    }

    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static void posix_destroy_mutex(ETMutex* mutex) {
    if (!mutex) {
        return;
    }

    if (mutex->initialized) {
        pthread_mutex_destroy(&mutex->mutex);
        pthread_mutexattr_destroy(&mutex->attr);
    }

    free(mutex);
}

// ============================================================================
// 세마포어 관리 함수들
// ============================================================================

static ETResult posix_create_semaphore(ETSemaphore** semaphore, int initial_count) {
    if (!semaphore || initial_count < 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETSemaphore* new_sem = (ETSemaphore*)calloc(1, sizeof(ETSemaphore));
    if (!new_sem) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    new_sem->max_count = INT32_MAX;
    new_sem->name[0] = '\0';
    new_sem->named = false;

#ifdef __APPLE__
    // macOS에서는 dispatch_semaphore 사용
    new_sem->sem = dispatch_semaphore_create(initial_count);
    if (!new_sem->sem) {
        free(new_sem);
        return ET_ERROR_OUT_OF_MEMORY;
    }
#else
    // Linux에서는 POSIX 세마포어 사용
    int result = sem_init(&new_sem->sem, 0, initial_count);
    if (result != 0) {
        free(new_sem);
        return posix_error_to_et_result(errno);
    }
#endif

    *semaphore = new_sem;
    return ET_SUCCESS;
}

static ETResult posix_create_semaphore_with_attributes(ETSemaphore** semaphore, int initial_count,
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
    new_sem->named = (new_sem->name[0] != '\0');

#ifdef __APPLE__
    // macOS에서는 dispatch_semaphore 사용 (명명된 세마포어 지원 안함)
    new_sem->sem = dispatch_semaphore_create(initial_count);
    if (!new_sem->sem) {
        free(new_sem);
        return ET_ERROR_OUT_OF_MEMORY;
    }
#else
    if (new_sem->named) {
        // 명명된 세마포어 생성
        sem_t* named_sem = sem_open(new_sem->name, O_CREAT | O_EXCL, 0644, initial_count);
        if (named_sem == SEM_FAILED) {
            free(new_sem);
            return posix_error_to_et_result(errno);
        }
        new_sem->sem = *named_sem;
    } else {
        // 익명 세마포어 생성
        int result = sem_init(&new_sem->sem, attributes->shared ? 1 : 0, initial_count);
        if (result != 0) {
            free(new_sem);
            return posix_error_to_et_result(errno);
        }
    }
#endif

    *semaphore = new_sem;
    return ET_SUCCESS;
}

static ETResult posix_wait_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __APPLE__
    dispatch_semaphore_wait(semaphore->sem, DISPATCH_TIME_FOREVER);
    return ET_SUCCESS;
#else
    int result = sem_wait(&semaphore->sem);
    if (result != 0) {
        return posix_error_to_et_result(errno);
    }
    return ET_SUCCESS;
#endif
}

static ETResult posix_try_wait_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __APPLE__
    long result = dispatch_semaphore_wait(semaphore->sem, DISPATCH_TIME_NOW);
    return (result == 0) ? ET_SUCCESS : ET_ERROR_BUSY;
#else
    int result = sem_trywait(&semaphore->sem);
    if (result == 0) {
        return ET_SUCCESS;
    } else if (errno == EAGAIN) {
        return ET_ERROR_BUSY;
    } else {
        return posix_error_to_et_result(errno);
    }
#endif
}

static ETResult posix_timed_wait_semaphore(ETSemaphore* semaphore, uint32_t timeout_ms) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __APPLE__
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, timeout_ms * 1000000ULL);
    long result = dispatch_semaphore_wait(semaphore->sem, timeout);
    return (result == 0) ? ET_SUCCESS : ET_ERROR_TIMEOUT;
#else
    struct timespec abs_time;
    calculate_absolute_time(&abs_time, timeout_ms);

    int result = sem_timedwait(&semaphore->sem, &abs_time);
    if (result == 0) {
        return ET_SUCCESS;
    } else if (errno == ETIMEDOUT) {
        return ET_ERROR_TIMEOUT;
    } else {
        return posix_error_to_et_result(errno);
    }
#endif
}

static ETResult posix_post_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __APPLE__
    dispatch_semaphore_signal(semaphore->sem);
    return ET_SUCCESS;
#else
    int result = sem_post(&semaphore->sem);
    if (result != 0) {
        return posix_error_to_et_result(errno);
    }
    return ET_SUCCESS;
#endif
}

static ETResult posix_get_semaphore_count(ETSemaphore* semaphore, int* count) {
    if (!semaphore || !count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

#ifdef __APPLE__
    // macOS dispatch_semaphore는 카운트 조회를 지원하지 않음
    *count = -1;  // 알 수 없음을 나타냄
    return ET_ERROR_NOT_IMPLEMENTED;
#else
    int result = sem_getvalue(&semaphore->sem, count);
    if (result != 0) {
        return posix_error_to_et_result(errno);
    }
    return ET_SUCCESS;
#endif
}

static void posix_destroy_semaphore(ETSemaphore* semaphore) {
    if (!semaphore) {
        return;
    }

#ifdef __APPLE__
    if (semaphore->sem) {
        dispatch_release(semaphore->sem);
    }
#else
    if (semaphore->named) {
        sem_close(&semaphore->sem);
        if (semaphore->name[0] != '\0') {
            sem_unlink(semaphore->name);
        }
    } else {
        sem_destroy(&semaphore->sem);
    }
#endif

    free(semaphore);
}

// ============================================================================
// 조건변수 관리 함수들
// ============================================================================

static ETResult posix_create_condition(ETCondition** condition) {
    if (!condition) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETCondition* new_cond = (ETCondition*)calloc(1, sizeof(ETCondition));
    if (!new_cond) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 조건변수 속성 초기화
    int result = pthread_condattr_init(&new_cond->attr);
    if (result != 0) {
        free(new_cond);
        return posix_error_to_et_result(result);
    }

    // 조건변수 초기화
    result = pthread_cond_init(&new_cond->cond, &new_cond->attr);
    if (result != 0) {
        pthread_condattr_destroy(&new_cond->attr);
        free(new_cond);
        return posix_error_to_et_result(result);
    }

    new_cond->initialized = true;

    *condition = new_cond;
    return ET_SUCCESS;
}

static ETResult posix_create_condition_with_attributes(ETCondition** condition,
                                                       const ETConditionAttributes* attributes) {
    if (!condition || !attributes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!et_condition_attributes_validate(attributes)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETCondition* new_cond = (ETCondition*)calloc(1, sizeof(ETCondition));
    if (!new_cond) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 조건변수 속성 초기화
    int result = pthread_condattr_init(&new_cond->attr);
    if (result != 0) {
        free(new_cond);
        return posix_error_to_et_result(result);
    }

    // 프로세스 간 공유 설정
    if (attributes->shared) {
        result = pthread_condattr_setpshared(&new_cond->attr, PTHREAD_PROCESS_SHARED);
        if (result != 0) {
            pthread_condattr_destroy(&new_cond->attr);
            free(new_cond);
            return posix_error_to_et_result(result);
        }
    }

    // 조건변수 초기화
    result = pthread_cond_init(&new_cond->cond, &new_cond->attr);
    if (result != 0) {
        pthread_condattr_destroy(&new_cond->attr);
        free(new_cond);
        return posix_error_to_et_result(result);
    }

    new_cond->initialized = true;

    *condition = new_cond;
    return ET_SUCCESS;
}

static ETResult posix_wait_condition(ETCondition* condition, ETMutex* mutex) {
    if (!condition || !mutex || !condition->initialized || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int result = pthread_cond_wait(&condition->cond, &mutex->mutex);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static ETResult posix_timed_wait_condition(ETCondition* condition, ETMutex* mutex, uint32_t timeout_ms) {
    if (!condition || !mutex || !condition->initialized || !mutex->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    struct timespec abs_time;
    calculate_absolute_time(&abs_time, timeout_ms);

    int result = pthread_cond_timedwait(&condition->cond, &mutex->mutex, &abs_time);
    if (result == ETIMEDOUT) {
        return ET_ERROR_TIMEOUT;
    } else if (result != 0) {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static ETResult posix_signal_condition(ETCondition* condition) {
    if (!condition || !condition->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int result = pthread_cond_signal(&condition->cond);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static ETResult posix_broadcast_condition(ETCondition* condition) {
    if (!condition || !condition->initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int result = pthread_cond_broadcast(&condition->cond);
    if (result != 0) {
        return posix_error_to_et_result(result);
    }

    return ET_SUCCESS;
}

static void posix_destroy_condition(ETCondition* condition) {
    if (!condition) {
        return;
    }

    if (condition->initialized) {
        pthread_cond_destroy(&condition->cond);
        pthread_condattr_destroy(&condition->attr);
    }

    free(condition);
}

// ============================================================================
// POSIX 스레딩 인터페이스 구조체
// ============================================================================

static ETThreadInterface g_posix_thread_interface = {
    // 스레드 관리
    .create_thread = posix_create_thread,
    .create_thread_with_attributes = posix_create_thread_with_attributes,
    .join_thread = posix_join_thread,
    .detach_thread = posix_detach_thread,
    .destroy_thread = posix_destroy_thread,

    // 스레드 속성 관리
    .set_thread_priority = posix_set_thread_priority,
    .get_thread_priority = posix_get_thread_priority,
    .set_thread_affinity = posix_set_thread_affinity,
    .get_current_thread_id = posix_get_current_thread_id,
    .get_thread_state = posix_get_thread_state,
    .sleep = posix_sleep,
    .yield = posix_yield,

    // 뮤텍스 관리
    .create_mutex = posix_create_mutex,
    .create_mutex_with_attributes = posix_create_mutex_with_attributes,
    .lock_mutex = posix_lock_mutex,
    .try_lock_mutex = posix_try_lock_mutex,
    .timed_lock_mutex = posix_timed_lock_mutex,
    .unlock_mutex = posix_unlock_mutex,
    .destroy_mutex = posix_destroy_mutex,

    // 세마포어 관리
    .create_semaphore = posix_create_semaphore,
    .create_semaphore_with_attributes = posix_create_semaphore_with_attributes,
    .wait_semaphore = posix_wait_semaphore,
    .try_wait_semaphore = posix_try_wait_semaphore,
    .timed_wait_semaphore = posix_timed_wait_semaphore,
    .post_semaphore = posix_post_semaphore,
    .get_semaphore_count = posix_get_semaphore_count,
    .destroy_semaphore = posix_destroy_semaphore,

    // 조건변수 관리
    .create_condition = posix_create_condition,
    .create_condition_with_attributes = posix_create_condition_with_attributes,
    .wait_condition = posix_wait_condition,
    .timed_wait_condition = posix_timed_wait_condition,
    .signal_condition = posix_signal_condition,
    .broadcast_condition = posix_broadcast_condition,
    .destroy_condition = posix_destroy_condition,

    // 플랫폼별 확장 데이터
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief POSIX 스레딩 인터페이스를 가져옵니다
 * @return POSIX 스레딩 인터페이스 포인터
 */
ETThreadInterface* et_get_posix_thread_interface(void) {
    return &g_posix_thread_interface;
}

/**
 * @brief POSIX 스레딩 인터페이스를 생성합니다
 * @param interface 생성된 인터페이스 포인터 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_posix_thread_interface(ETThreadInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *interface = &g_posix_thread_interface;
    return ET_SUCCESS;
}

/**
 * @brief POSIX 스레딩 인터페이스를 해제합니다
 * @param interface 해제할 인터페이스
 */
void et_destroy_posix_thread_interface(ETThreadInterface* interface) {
    // 정적 인터페이스이므로 별도 정리 작업 없음
    (void)interface;
}

#endif // defined(__linux__) || defined(__APPLE__)