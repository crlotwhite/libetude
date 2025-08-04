/**
 * @file windows_large_pages_simple.c
 * @brief Windows Large Page 메모리 지원 간단 구현
 */

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libetude/types.h"
#include "libetude/platform/windows_large_pages.h"

// 상수 정의
#define LARGE_PAGE_MINIMUM_SIZE (2 * 1024 * 1024)

// Large Page 관리자 구조체
typedef struct {
    bool privilege_enabled;
    bool large_pages_supported;
    SIZE_T large_page_size;
    SIZE_T total_allocated;
    SIZE_T fallback_allocated;
    LONG allocation_count;
    LONG fallback_count;
    CRITICAL_SECTION lock;
    bool initialized;
} ETLargePageManager;

// 전역 관리자
static ETLargePageManager g_manager = {0};

/**
 * @brief SeLockMemoryPrivilege 권한 활성화
 */
bool et_windows_enable_large_page_privilege(void) {
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                         &token)) {
        return false;
    }

    if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid)) {
        CloseHandle(token);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    DWORD error = GetLastError();

    CloseHandle(token);
    return (result && error == ERROR_SUCCESS);
}

/**
 * @brief Large Page 지원 초기화
 */
ETResult et_windows_large_pages_init(void) {
    if (g_manager.initialized) {
        return LIBETUDE_SUCCESS;
    }

    InitializeCriticalSection(&g_manager.lock);
    EnterCriticalSection(&g_manager.lock);

    g_manager.large_page_size = GetLargePageMinimum();
    g_manager.large_pages_supported = (g_manager.large_page_size > 0);

    if (g_manager.large_pages_supported) {
        g_manager.privilege_enabled = et_windows_enable_large_page_privilege();
        if (!g_manager.privilege_enabled) {
            g_manager.large_pages_supported = false;
        }
    }

    g_manager.total_allocated = 0;
    g_manager.fallback_allocated = 0;
    g_manager.allocation_count = 0;
    g_manager.fallback_count = 0;
    g_manager.initialized = true;

    LeaveCriticalSection(&g_manager.lock);
    return LIBETUDE_SUCCESS;
}

/**
 * @brief Large Page 지원 정리
 */
void et_windows_large_pages_finalize(void) {
    if (!g_manager.initialized) {
        return;
    }

    EnterCriticalSection(&g_manager.lock);
    g_manager.initialized = false;
    LeaveCriticalSection(&g_manager.lock);

    DeleteCriticalSection(&g_manager.lock);
    memset(&g_manager, 0, sizeof(g_manager));
}

/**
 * @brief Large Page 메모리 할당
 */
void* et_windows_alloc_large_pages(size_t size) {
    if (size == 0) {
        return NULL;
    }

    if (!g_manager.initialized) {
        et_windows_large_pages_init();
    }

    void* memory = NULL;
    bool used_large_pages = false;

    EnterCriticalSection(&g_manager.lock);

    if (g_manager.large_pages_supported && g_manager.privilege_enabled) {
        SIZE_T aligned_size = (size + g_manager.large_page_size - 1) &
                             ~(g_manager.large_page_size - 1);

        memory = VirtualAlloc(NULL, aligned_size,
                            MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                            PAGE_READWRITE);

        if (memory) {
            used_large_pages = true;
            g_manager.total_allocated += aligned_size;
        }
    }

    if (!memory) {
        memory = VirtualAlloc(NULL, size,
                            MEM_COMMIT | MEM_RESERVE,
                            PAGE_READWRITE);

        if (memory) {
            g_manager.fallback_allocated += size;
            InterlockedIncrement(&g_manager.fallback_count);
        }
    }

    if (memory) {
        InterlockedIncrement(&g_manager.allocation_count);
    }

    LeaveCriticalSection(&g_manager.lock);
    return memory;
}

/**
 * @brief Large Page 메모리 해제
 */
void et_windows_free_large_pages(void* memory, size_t size) {
    if (!memory) {
        return;
    }

    VirtualFree(memory, 0, MEM_RELEASE);

    if (g_manager.initialized) {
        EnterCriticalSection(&g_manager.lock);

        // 간단한 통계 업데이트
        if (g_manager.total_allocated >= size) {
            g_manager.total_allocated -= size;
        } else if (g_manager.fallback_allocated >= size) {
            g_manager.fallback_allocated -= size;
        }

        LeaveCriticalSection(&g_manager.lock);
    }
}

/**
 * @brief Large Page 메모리 재할당
 */
void* et_windows_realloc_large_pages(void* memory, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        et_windows_free_large_pages(memory, old_size);
        return NULL;
    }

    if (!memory) {
        return et_windows_alloc_large_pages(new_size);
    }

    void* new_memory = et_windows_alloc_large_pages(new_size);
    if (!new_memory) {
        return NULL;
    }

    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_memory, memory, copy_size);

    et_windows_free_large_pages(memory, old_size);
    return new_memory;
}

/**
 * @brief 정렬된 메모리 할당
 */
void* et_windows_alloc_aligned_large_pages(size_t size, size_t alignment) {
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    if (g_manager.initialized && alignment > g_manager.large_page_size) {
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
        return VirtualAlloc(NULL, aligned_size,
                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    return et_windows_alloc_large_pages(size);
}

/**
 * @brief Large Page 상태 정보 조회
 */
ETResult et_windows_large_pages_get_info(ETLargePageInfo* info) {
    if (!info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_manager.initialized) {
        et_windows_large_pages_init();
    }

    EnterCriticalSection(&g_manager.lock);

    info->is_supported = g_manager.large_pages_supported;
    info->privilege_enabled = g_manager.privilege_enabled;
    info->large_page_size = g_manager.large_page_size;
    info->total_allocated = g_manager.total_allocated;
    info->fallback_allocated = g_manager.fallback_allocated;
    info->allocation_count = g_manager.allocation_count;
    info->fallback_count = g_manager.fallback_count;

    LeaveCriticalSection(&g_manager.lock);
    return LIBETUDE_SUCCESS;
}

/**
 * @brief Large Page 상태를 문자열로 반환
 */
ETResult et_windows_large_pages_status_to_string(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    ETLargePageInfo info;
    ETResult result = et_windows_large_pages_get_info(&info);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    snprintf(buffer, buffer_size,
        "Large Page Status:\n"
        "  Supported: %s\n"
        "  Privilege Enabled: %s\n"
        "  Large Page Size: %zu bytes (%.1f MB)\n"
        "  Total Allocated: %zu bytes (%.1f MB)\n"
        "  Fallback Allocated: %zu bytes (%.1f MB)\n"
        "  Total Allocations: %ld\n"
        "  Fallback Count: %ld\n"
        "  Large Page Usage: %.1f%%",
        info.is_supported ? "Yes" : "No",
        info.privilege_enabled ? "Yes" : "No",
        info.large_page_size, (double)info.large_page_size / (1024 * 1024),
        info.total_allocated, (double)info.total_allocated / (1024 * 1024),
        info.fallback_allocated, (double)info.fallback_allocated / (1024 * 1024),
        info.allocation_count,
        info.fallback_count,
        (info.allocation_count > 0) ?
            (100.0 * (info.allocation_count - info.fallback_count) / info.allocation_count) : 0.0);

    return LIBETUDE_SUCCESS;
}

/**
 * @brief Large Page 통계 초기화
 */
void et_windows_large_pages_reset_stats(void) {
    if (!g_manager.initialized) {
        return;
    }

    EnterCriticalSection(&g_manager.lock);

    g_manager.total_allocated = 0;
    g_manager.fallback_allocated = 0;
    g_manager.allocation_count = 0;
    g_manager.fallback_count = 0;

    LeaveCriticalSection(&g_manager.lock);
}