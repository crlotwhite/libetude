/**
 * @file linux_utils.c
 * @brief Linux 플랫폼 특화 유틸리티 함수들
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/error.h"

#ifdef __linux__

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/**
 * @brief 시스템 정보를 가져옵니다
 */
ETResult linux_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sysinfo sys_info;
    if (sysinfo(&sys_info) != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux sysinfo 호출 실패: %s", strerror(errno));
        return ET_ERROR_SYSTEM;
    }

    struct utsname uname_info;
    if (uname(&uname_info) != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux uname 호출 실패: %s", strerror(errno));
        return ET_ERROR_SYSTEM;
    }

    info->total_memory = sys_info.totalram * sys_info.mem_unit;
    info->available_memory = sys_info.freeram * sys_info.mem_unit;
    info->cpu_count = get_nprocs();

    // 시스템 이름 복사
    strncpy(info->system_name, uname_info.sysname, sizeof(info->system_name) - 1);
    info->system_name[sizeof(info->system_name) - 1] = '\0';

    return ET_SUCCESS;
}

/**
 * @brief 고해상도 타이머를 가져옵니다
 */
ETResult linux_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux clock_gettime 호출 실패: %s", strerror(errno));
        return ET_ERROR_SYSTEM;
    }

    *time_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return ET_SUCCESS;
}

/**
 * @brief CPU 친화성을 설정합니다
 */
ETResult linux_set_thread_affinity(pthread_t thread, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux CPU 친화성 설정 실패 (CPU %d): %s",
                     cpu_id, strerror(result));
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

/**
 * @brief 스레드 우선순위를 설정합니다
 */
ETResult linux_set_thread_priority(pthread_t thread, int priority) {
    struct sched_param param;
    param.sched_priority = priority;

    int result = pthread_setschedparam(thread, SCHED_FIFO, &param);
    if (result != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux 스레드 우선순위 설정 실패 (우선순위 %d): %s",
                     priority, strerror(result));
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 페이지를 잠급니다 (실시간 처리용)
 */
ETResult linux_lock_memory_pages(void* addr, size_t len) {
    if (!addr || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (mlock(addr, len) != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux 메모리 페이지 잠금 실패: %s", strerror(errno));
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 페이지 잠금을 해제합니다
 */
ETResult linux_unlock_memory_pages(void* addr, size_t len) {
    if (!addr || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (munlock(addr, len) != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "Linux 메모리 페이지 잠금 해제 실패: %s", strerror(errno));
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

#endif // __linux__