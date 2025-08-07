#include "libetude/platform/memory.h"
#include "libetude/platform/factory.h"
#include <string.h>
#include <stdlib.h>

// 전역 메모리 인터페이스 포인터
static ETMemoryInterface* g_memory_interface = NULL;

// 메모리 통계 (디버그용)
static ETMemoryStats g_memory_stats = {0};

ETResult et_memory_init(void) {
    if (g_memory_interface != NULL) {
        return ET_SUCCESS; // 이미 초기화됨
    }

    // 플랫폼별 팩토리에서 메모리 인터페이스 생성
    ETResult result = et_create_memory_interface(&g_memory_interface);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 메모리 통계 초기화
    memset(&g_memory_stats, 0, sizeof(ETMemoryStats));

    return ET_SUCCESS;
}

void et_memory_cleanup(void) {
    if (g_memory_interface != NULL) {
        et_destroy_memory_interface(g_memory_interface);
        g_memory_interface = NULL;
    }
}

ETMemoryInterface* et_get_memory_interface(void) {
    return g_memory_interface;
}

ETResult et_memory_set_zero(void* ptr, size_t size) {
    if (ptr == NULL) {
        return ET_RESULT_INVALID_PARAMETER;
    }

    memset(ptr, 0, size);
    return ET_RESULT_SUCCESS;
}

ETResult et_memory_copy(void* dest, const void* src, size_t size) {
    if (dest == NULL || src == NULL) {
        return ET_RESULT_INVALID_PARAMETER;
    }

    memcpy(dest, src, size);
    return ET_RESULT_SUCCESS;
}

ETResult et_memory_compare(const void* ptr1, const void* ptr2, size_t size, int* result) {
    if (ptr1 == NULL || ptr2 == NULL || result == NULL) {
        return ET_RESULT_INVALID_PARAMETER;
    }

    *result = memcmp(ptr1, ptr2, size);
    return ET_RESULT_SUCCESS;
}

bool et_memory_is_aligned(const void* ptr, size_t alignment) {
    if (ptr == NULL || alignment == 0) {
        return false;
    }

    // alignment가 2의 거듭제곱인지 확인
    if ((alignment & (alignment - 1)) != 0) {
        return false;
    }

    uintptr_t addr = (uintptr_t)ptr;
    return (addr & (alignment - 1)) == 0;
}

size_t et_memory_get_page_size(void) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL) {
        // 기본값 반환 (대부분의 시스템에서 4KB)
        return 4096;
    }

    // 플랫폼별 구현에서 페이지 크기를 가져올 수 있도록 확장 가능
    // 현재는 기본값 반환
    return 4096;
}

// 래퍼 함수들 - 통계 추적 포함
void* et_malloc(size_t size) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->malloc == NULL) {
        return NULL;
    }

    void* ptr = interface->malloc(size);
    if (ptr != NULL) {
        g_memory_stats.total_allocated += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.total_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.total_allocated;
        }
    }

    return ptr;
}

void* et_calloc(size_t count, size_t size) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->calloc == NULL) {
        return NULL;
    }

    void* ptr = interface->calloc(count, size);
    if (ptr != NULL) {
        size_t total_size = count * size;
        g_memory_stats.total_allocated += total_size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.total_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.total_allocated;
        }
    }

    return ptr;
}

void* et_realloc(void* ptr, size_t size) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->realloc == NULL) {
        return NULL;
    }

    void* new_ptr = interface->realloc(ptr, size);
    // 통계 업데이트는 복잡하므로 여기서는 생략
    // 실제 구현에서는 이전 크기를 추적해야 함

    return new_ptr;
}

void et_free(void* ptr) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->free == NULL || ptr == NULL) {
        return;
    }

    interface->free(ptr);
    g_memory_stats.free_count++;
}

void* et_aligned_malloc(size_t size, size_t alignment) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->aligned_malloc == NULL) {
        return NULL;
    }

    void* ptr = interface->aligned_malloc(size, alignment);
    if (ptr != NULL) {
        g_memory_stats.total_allocated += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.total_allocated > g_memory_stats.peak_allocated) {
            g_memory_stats.peak_allocated = g_memory_stats.total_allocated;
        }
    }

    return ptr;
}

void et_aligned_free(void* ptr) {
    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->aligned_free == NULL || ptr == NULL) {
        return;
    }

    interface->aligned_free(ptr);
    g_memory_stats.free_count++;
}