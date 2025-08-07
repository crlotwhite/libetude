#include "libetude/platform/memory.h"
#include "libetude/platform/common.h"
#include <windows.h>
#include <malloc.h>
#include <string.h>

// Windows 공유 메모리 구조체
typedef struct ETSharedMemory {
    HANDLE handle;              // 파일 매핑 핸들
    void* mapped_address;       // 매핑된 주소
    size_t size;                // 크기
    char name[256];             // 이름
} ETSharedMemory;

// Windows 메모리 매핑 파일 구조체
typedef struct ETMemoryMap {
    HANDLE file_handle;         // 파일 핸들
    HANDLE mapping_handle;      // 매핑 핸들
    void* mapped_address;       // 매핑된 주소
    size_t size;                // 크기
    ETMemoryMapMode mode;       // 매핑 모드
} ETMemoryMap;

// Windows 메모리 통계
static ETMemoryStats g_windows_memory_stats = {0};

// ============================================================================
// 내부 유틸리티 함수들
// ============================================================================

/**
 * @brief Windows 메모리 보호 모드를 변환합니다
 */
static DWORD et_memory_protection_to_windows(ETMemoryProtection protection) {
    DWORD windows_protection = 0;

    if (protection & ET_MEMORY_PROTECT_EXECUTE) {
        if (protection & ET_MEMORY_PROTECT_WRITE) {
            windows_protection = PAGE_EXECUTE_READWRITE;
        } else if (protection & ET_MEMORY_PROTECT_READ) {
            windows_protection = PAGE_EXECUTE_READ;
        } else {
            windows_protection = PAGE_EXECUTE;
        }
    } else if (protection & ET_MEMORY_PROTECT_WRITE) {
        windows_protection = PAGE_READWRITE;
    } else if (protection & ET_MEMORY_PROTECT_READ) {
        windows_protection = PAGE_READONLY;
    } else {
        windows_protection = PAGE_NOACCESS;
    }

    return windows_protection;
}

/**
 * @brief Windows 파일 매핑 모드를 변환합니다
 */
static DWORD et_memory_map_mode_to_windows_access(ETMemoryMapMode mode) {
    DWORD access = 0;

    if (mode & ET_MEMORY_MAP_WRITE) {
        access = FILE_MAP_WRITE;
    } else if (mode & ET_MEMORY_MAP_READ) {
        access = FILE_MAP_READ;
    }

    if (mode & ET_MEMORY_MAP_EXECUTE) {
        access |= FILE_MAP_EXECUTE;
    }

    return access;
}

/**
 * @brief Windows 파일 매핑 보호 모드를 변환합니다
 */
static DWORD et_memory_map_mode_to_windows_protect(ETMemoryMapMode mode) {
    if (mode & ET_MEMORY_MAP_EXECUTE) {
        if (mode & ET_MEMORY_MAP_WRITE) {
            return PAGE_EXECUTE_READWRITE;
        } else {
            return PAGE_EXECUTE_READ;
        }
    } else if (mode & ET_MEMORY_MAP_WRITE) {
        return PAGE_READWRITE;
    } else {
        return PAGE_READONLY;
    }
}

// ============================================================================
// 기본 메모리 할당 함수들
// ============================================================================

static void* windows_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr != NULL) {
        g_windows_memory_stats.total_allocated += size;
        g_windows_memory_stats.allocation_count++;
        if (g_windows_memory_stats.total_allocated > g_windows_memory_stats.peak_allocated) {
            g_windows_memory_stats.peak_allocated = g_windows_memory_stats.total_allocated;
        }
    }
    return ptr;
}

static void* windows_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (ptr != NULL) {
        size_t total_size = count * size;
        g_windows_memory_stats.total_allocated += total_size;
        g_windows_memory_stats.allocation_count++;
        if (g_windows_memory_stats.total_allocated > g_windows_memory_stats.peak_allocated) {
            g_windows_memory_stats.peak_allocated = g_windows_memory_stats.total_allocated;
        }
    }
    return ptr;
}

static void* windows_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

static void windows_free(void* ptr) {
    if (ptr != NULL) {
        free(ptr);
        g_windows_memory_stats.free_count++;
    }
}

// ============================================================================
// 정렬된 메모리 할당 함수들
// ============================================================================

static void* windows_aligned_malloc(size_t size, size_t alignment) {
    void* ptr = _aligned_malloc(size, alignment);
    if (ptr != NULL) {
        g_windows_memory_stats.total_allocated += size;
        g_windows_memory_stats.allocation_count++;
        if (g_windows_memory_stats.total_allocated > g_windows_memory_stats.peak_allocated) {
            g_windows_memory_stats.peak_allocated = g_windows_memory_stats.total_allocated;
        }
    }
    return ptr;
}

static void windows_aligned_free(void* ptr) {
    if (ptr != NULL) {
        _aligned_free(ptr);
        g_windows_memory_stats.free_count++;
    }
}

// ============================================================================
// 메모리 페이지 관리 함수들
// ============================================================================

static ETResult windows_lock_pages(void* addr, size_t len) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (VirtualLock(addr, len)) {
        return ET_SUCCESS;
    }

    DWORD error = GetLastError();
    switch (error) {
        case ERROR_NOT_ENOUGH_MEMORY:
            return ET_ERROR_OUT_OF_MEMORY;
        case ERROR_INVALID_ADDRESS:
            return ET_ERROR_INVALID_ARGUMENT;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

static ETResult windows_unlock_pages(void* addr, size_t len) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (VirtualUnlock(addr, len)) {
        return ET_SUCCESS;
    }

    DWORD error = GetLastError();
    switch (error) {
        case ERROR_NOT_LOCKED:
            return ET_SUCCESS; // 이미 잠금 해제됨
        case ERROR_INVALID_ADDRESS:
            return ET_ERROR_INVALID_ARGUMENT;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

static ETResult windows_protect_pages(void* addr, size_t len, ETMemoryProtection protection) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DWORD windows_protection = et_memory_protection_to_windows(protection);
    DWORD old_protection;

    if (VirtualProtect(addr, len, windows_protection, &old_protection)) {
        return ET_SUCCESS;
    }

    DWORD error = GetLastError();
    switch (error) {
        case ERROR_INVALID_ADDRESS:
            return ET_ERROR_INVALID_ARGUMENT;
        case ERROR_INVALID_PARAMETER:
            return ET_ERROR_INVALID_ARGUMENT;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

// ============================================================================
// 공유 메모리 함수들
// ============================================================================

static ETResult windows_create_shared_memory(const char* name, size_t size, ETSharedMemory** shm) {
    if (name == NULL || size == 0 || shm == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSharedMemory* shared_memory = (ETSharedMemory*)malloc(sizeof(ETSharedMemory));
    if (shared_memory == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(shared_memory, 0, sizeof(ETSharedMemory));
    strncpy_s(shared_memory->name, sizeof(shared_memory->name), name, _TRUNCATE);
    shared_memory->size = size;

    // 파일 매핑 객체 생성
    shared_memory->handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // 페이징 파일 사용
        NULL,                   // 기본 보안 속성
        PAGE_READWRITE,         // 읽기/쓰기 권한
        0,                      // 최대 크기 상위 32비트
        (DWORD)size,           // 최대 크기 하위 32비트
        name                    // 객체 이름
    );

    if (shared_memory->handle == NULL) {
        free(shared_memory);
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_ALREADY_EXISTS:
                return ET_ERROR_ALREADY_EXISTS;
            case ERROR_ACCESS_DENIED:
                return ET_ERROR_ACCESS_DENIED;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    *shm = shared_memory;
    return ET_SUCCESS;
}

static ETResult windows_open_shared_memory(const char* name, ETSharedMemory** shm) {
    if (name == NULL || shm == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSharedMemory* shared_memory = (ETSharedMemory*)malloc(sizeof(ETSharedMemory));
    if (shared_memory == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(shared_memory, 0, sizeof(ETSharedMemory));
    strncpy_s(shared_memory->name, sizeof(shared_memory->name), name, _TRUNCATE);

    // 기존 파일 매핑 객체 열기
    shared_memory->handle = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,    // 모든 권한
        FALSE,                  // 상속하지 않음
        name                    // 객체 이름
    );

    if (shared_memory->handle == NULL) {
        free(shared_memory);
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
                return ET_ERROR_NOT_FOUND;
            case ERROR_ACCESS_DENIED:
                return ET_ERROR_ACCESS_DENIED;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    *shm = shared_memory;
    return ET_SUCCESS;
}

static void* windows_map_shared_memory(ETSharedMemory* shm) {
    if (shm == NULL || shm->handle == NULL) {
        return NULL;
    }

    if (shm->mapped_address != NULL) {
        return shm->mapped_address; // 이미 매핑됨
    }

    shm->mapped_address = MapViewOfFile(
        shm->handle,            // 파일 매핑 핸들
        FILE_MAP_ALL_ACCESS,    // 모든 권한
        0,                      // 오프셋 상위 32비트
        0,                      // 오프셋 하위 32비트
        shm->size               // 매핑할 크기
    );

    return shm->mapped_address;
}

static ETResult windows_unmap_shared_memory(ETSharedMemory* shm, void* addr) {
    if (shm == NULL || addr == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (UnmapViewOfFile(addr)) {
        if (shm->mapped_address == addr) {
            shm->mapped_address = NULL;
        }
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

static void windows_destroy_shared_memory(ETSharedMemory* shm) {
    if (shm == NULL) {
        return;
    }

    if (shm->mapped_address != NULL) {
        UnmapViewOfFile(shm->mapped_address);
    }

    if (shm->handle != NULL) {
        CloseHandle(shm->handle);
    }

    free(shm);
}

// ============================================================================
// 메모리 매핑 파일 함수들
// ============================================================================

static ETResult windows_create_memory_map(const char* filename, size_t size, ETMemoryMapMode mode, ETMemoryMap** map) {
    if (filename == NULL || size == 0 || map == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMemoryMap* memory_map = (ETMemoryMap*)malloc(sizeof(ETMemoryMap));
    if (memory_map == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(memory_map, 0, sizeof(ETMemoryMap));
    memory_map->size = size;
    memory_map->mode = mode;

    // 파일 열기
    DWORD file_access = 0;
    DWORD file_creation = OPEN_ALWAYS;

    if (mode & ET_MEMORY_MAP_WRITE) {
        file_access = GENERIC_READ | GENERIC_WRITE;
    } else {
        file_access = GENERIC_READ;
        file_creation = OPEN_EXISTING;
    }

    memory_map->file_handle = CreateFileA(
        filename,
        file_access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        file_creation,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (memory_map->file_handle == INVALID_HANDLE_VALUE) {
        free(memory_map);
        DWORD error = GetLastError();
        switch (error) {
            case ERROR_FILE_NOT_FOUND:
                return ET_ERROR_NOT_FOUND;
            case ERROR_ACCESS_DENIED:
                return ET_ERROR_ACCESS_DENIED;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    // 파일 매핑 객체 생성
    DWORD protect = et_memory_map_mode_to_windows_protect(mode);
    memory_map->mapping_handle = CreateFileMappingA(
        memory_map->file_handle,
        NULL,
        protect,
        0,
        (DWORD)size,
        NULL
    );

    if (memory_map->mapping_handle == NULL) {
        CloseHandle(memory_map->file_handle);
        free(memory_map);
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    *map = memory_map;
    return ET_SUCCESS;
}

static void* windows_map_file(ETMemoryMap* map, size_t offset, size_t length) {
    if (map == NULL || map->mapping_handle == NULL) {
        return NULL;
    }

    DWORD access = et_memory_map_mode_to_windows_access(map->mode);

    void* mapped_address = MapViewOfFile(
        map->mapping_handle,
        access,
        (DWORD)(offset >> 32),  // 오프셋 상위 32비트
        (DWORD)(offset & 0xFFFFFFFF), // 오프셋 하위 32비트
        length
    );

    if (mapped_address != NULL && map->mapped_address == NULL) {
        map->mapped_address = mapped_address;
    }

    return mapped_address;
}

static ETResult windows_unmap_file(ETMemoryMap* map, void* addr, size_t length) {
    if (map == NULL || addr == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (UnmapViewOfFile(addr)) {
        if (map->mapped_address == addr) {
            map->mapped_address = NULL;
        }
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

static void windows_destroy_memory_map(ETMemoryMap* map) {
    if (map == NULL) {
        return;
    }

    if (map->mapped_address != NULL) {
        UnmapViewOfFile(map->mapped_address);
    }

    if (map->mapping_handle != NULL) {
        CloseHandle(map->mapping_handle);
    }

    if (map->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(map->file_handle);
    }

    free(map);
}

// ============================================================================
// 메모리 정보 및 통계 함수들
// ============================================================================

static ETResult windows_get_memory_info(void* ptr, ETMemoryInfo* info) {
    if (ptr == NULL || info == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) {
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    info->address = mbi.BaseAddress;
    info->size = mbi.RegionSize;
    info->alignment = 0; // Windows에서는 직접 제공하지 않음

    // 보호 모드 변환
    info->protection = ET_MEMORY_PROTECT_NONE;
    if (mbi.Protect & PAGE_READONLY) {
        info->protection |= ET_MEMORY_PROTECT_READ;
    }
    if (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) {
        info->protection |= ET_MEMORY_PROTECT_READ | ET_MEMORY_PROTECT_WRITE;
    }
    if (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) {
        info->protection |= ET_MEMORY_PROTECT_EXECUTE;
    }

    return ET_SUCCESS;
}

static ETResult windows_get_memory_stats(ETMemoryStats* stats) {
    if (stats == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *stats = g_windows_memory_stats;
    return ET_SUCCESS;
}

// ============================================================================
// Windows 메모리 인터페이스 생성
// ============================================================================

ETResult et_create_windows_memory_interface(ETMemoryInterface** interface) {
    if (interface == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMemoryInterface* memory_interface = (ETMemoryInterface*)malloc(sizeof(ETMemoryInterface));
    if (memory_interface == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 함수 포인터 설정
    memory_interface->malloc = windows_malloc;
    memory_interface->calloc = windows_calloc;
    memory_interface->realloc = windows_realloc;
    memory_interface->free = windows_free;

    memory_interface->aligned_malloc = windows_aligned_malloc;
    memory_interface->aligned_free = windows_aligned_free;

    memory_interface->lock_pages = windows_lock_pages;
    memory_interface->unlock_pages = windows_unlock_pages;
    memory_interface->protect_pages = windows_protect_pages;

    memory_interface->create_shared_memory = windows_create_shared_memory;
    memory_interface->open_shared_memory = windows_open_shared_memory;
    memory_interface->map_shared_memory = windows_map_shared_memory;
    memory_interface->unmap_shared_memory = windows_unmap_shared_memory;
    memory_interface->destroy_shared_memory = windows_destroy_shared_memory;

    memory_interface->create_memory_map = windows_create_memory_map;
    memory_interface->map_file = windows_map_file;
    memory_interface->unmap_file = windows_unmap_file;
    memory_interface->destroy_memory_map = windows_destroy_memory_map;

    memory_interface->get_memory_info = windows_get_memory_info;
    memory_interface->get_memory_stats = windows_get_memory_stats;

    memory_interface->platform_data = NULL;

    // 통계 초기화
    memset(&g_windows_memory_stats, 0, sizeof(ETMemoryStats));

    *interface = memory_interface;
    return ET_SUCCESS;
}

void et_destroy_windows_memory_interface(ETMemoryInterface* interface) {
    if (interface != NULL) {
        free(interface);
    }
}