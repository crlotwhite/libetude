#include "libetude/platform/memory.h"
#include "libetude/platform/common.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// POSIX 공유 메모리 구조체
typedef struct ETSharedMemory {
    int fd;                     // 파일 디스크립터
    void* mapped_address;       // 매핑된 주소
    size_t size;                // 크기
    char name[256];             // 이름
} ETSharedMemory;

// POSIX 메모리 매핑 파일 구조체
typedef struct ETMemoryMap {
    int fd;                     // 파일 디스크립터
    void* mapped_address;       // 매핑된 주소
    size_t size;                // 크기
    ETMemoryMapMode mode;       // 매핑 모드
} ETMemoryMap;

// POSIX 메모리 통계
static ETMemoryStats g_posix_memory_stats = {0};

// ============================================================================
// 내부 유틸리티 함수들
// ============================================================================

/**
 * @brief POSIX 메모리 보호 모드를 변환합니다
 */
static int et_memory_protection_to_posix(ETMemoryProtection protection) {
    int posix_protection = PROT_NONE;

    if (protection & ET_MEMORY_PROTECT_READ) {
        posix_protection |= PROT_READ;
    }
    if (protection & ET_MEMORY_PROTECT_WRITE) {
        posix_protection |= PROT_WRITE;
    }
    if (protection & ET_MEMORY_PROTECT_EXECUTE) {
        posix_protection |= PROT_EXEC;
    }

    return posix_protection;
}

/**
 * @brief POSIX 파일 매핑 모드를 변환합니다
 */
static int et_memory_map_mode_to_posix_prot(ETMemoryMapMode mode) {
    int prot = PROT_NONE;

    if (mode & ET_MEMORY_MAP_READ) {
        prot |= PROT_READ;
    }
    if (mode & ET_MEMORY_MAP_WRITE) {
        prot |= PROT_WRITE;
    }
    if (mode & ET_MEMORY_MAP_EXECUTE) {
        prot |= PROT_EXEC;
    }

    return prot;
}

/**
 * @brief POSIX 파일 매핑 플래그를 변환합니다
 */
static int et_memory_map_mode_to_posix_flags(ETMemoryMapMode mode) {
    if (mode & ET_MEMORY_MAP_PRIVATE) {
        return MAP_PRIVATE;
    } else {
        return MAP_SHARED;
    }
}//
============================================================================
// 기본 메모리 할당 함수들
// ============================================================================

static void* posix_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr != NULL) {
        g_posix_memory_stats.total_allocated += size;
        g_posix_memory_stats.allocation_count++;
        if (g_posix_memory_stats.total_allocated > g_posix_memory_stats.peak_allocated) {
            g_posix_memory_stats.peak_allocated = g_posix_memory_stats.total_allocated;
        }
    }
    return ptr;
}

static void* posix_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (ptr != NULL) {
        size_t total_size = count * size;
        g_posix_memory_stats.total_allocated += total_size;
        g_posix_memory_stats.allocation_count++;
        if (g_posix_memory_stats.total_allocated > g_posix_memory_stats.peak_allocated) {
            g_posix_memory_stats.peak_allocated = g_posix_memory_stats.total_allocated;
        }
    }
    return ptr;
}

static void* posix_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

static void posix_free(void* ptr) {
    if (ptr != NULL) {
        free(ptr);
        g_posix_memory_stats.free_count++;
    }
}

// ============================================================================
// 정렬된 메모리 할당 함수들
// ============================================================================

static void* posix_aligned_malloc(size_t size, size_t alignment) {
    void* ptr = NULL;

    if (posix_memalign(&ptr, alignment, size) == 0) {
        g_posix_memory_stats.total_allocated += size;
        g_posix_memory_stats.allocation_count++;
        if (g_posix_memory_stats.total_allocated > g_posix_memory_stats.peak_allocated) {
            g_posix_memory_stats.peak_allocated = g_posix_memory_stats.total_allocated;
        }
        return ptr;
    }

    return NULL;
}

static void posix_aligned_free(void* ptr) {
    if (ptr != NULL) {
        free(ptr);
        g_posix_memory_stats.free_count++;
    }
}//
 ============================================================================
// 메모리 페이지 관리 함수들
// ============================================================================

static ETResult posix_lock_pages(void* addr, size_t len) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (mlock(addr, len) == 0) {
        return ET_SUCCESS;
    }

    switch (errno) {
        case ENOMEM:
            return ET_ERROR_OUT_OF_MEMORY;
        case EPERM:
            return ET_ERROR_ACCESS_DENIED;
        case EINVAL:
            return ET_ERROR_INVALID_ARGUMENT;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

static ETResult posix_unlock_pages(void* addr, size_t len) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (munlock(addr, len) == 0) {
        return ET_SUCCESS;
    }

    switch (errno) {
        case ENOMEM:
            return ET_SUCCESS; // 이미 잠금 해제됨
        case EINVAL:
            return ET_ERROR_INVALID_ARGUMENT;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}

static ETResult posix_protect_pages(void* addr, size_t len, ETMemoryProtection protection) {
    if (addr == NULL || len == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int posix_protection = et_memory_protection_to_posix(protection);

    if (mprotect(addr, len, posix_protection) == 0) {
        return ET_SUCCESS;
    }

    switch (errno) {
        case EACCES:
            return ET_ERROR_ACCESS_DENIED;
        case EINVAL:
            return ET_ERROR_INVALID_ARGUMENT;
        case ENOMEM:
            return ET_ERROR_OUT_OF_MEMORY;
        default:
            return ET_ERROR_PLATFORM_SPECIFIC;
    }
}// ===
=========================================================================
// 공유 메모리 함수들
// ============================================================================

static ETResult posix_create_shared_memory(const char* name, size_t size, ETSharedMemory** shm) {
    if (name == NULL || size == 0 || shm == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSharedMemory* shared_memory = (ETSharedMemory*)malloc(sizeof(ETSharedMemory));
    if (shared_memory == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(shared_memory, 0, sizeof(ETSharedMemory));
    snprintf(shared_memory->name, sizeof(shared_memory->name), "/%s", name);
    shared_memory->size = size;

    // 공유 메모리 객체 생성
    shared_memory->fd = shm_open(shared_memory->name, O_CREAT | O_RDWR, 0666);
    if (shared_memory->fd == -1) {
        free(shared_memory);
        switch (errno) {
            case EEXIST:
                return ET_ERROR_ALREADY_EXISTS;
            case EACCES:
                return ET_ERROR_ACCESS_DENIED;
            case ENAMETOOLONG:
                return ET_ERROR_INVALID_ARGUMENT;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    // 크기 설정
    if (ftruncate(shared_memory->fd, size) == -1) {
        close(shared_memory->fd);
        shm_unlink(shared_memory->name);
        free(shared_memory);
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    *shm = shared_memory;
    return ET_SUCCESS;
}

static ETResult posix_open_shared_memory(const char* name, ETSharedMemory** shm) {
    if (name == NULL || shm == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSharedMemory* shared_memory = (ETSharedMemory*)malloc(sizeof(ETSharedMemory));
    if (shared_memory == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(shared_memory, 0, sizeof(ETSharedMemory));
    snprintf(shared_memory->name, sizeof(shared_memory->name), "/%s", name);

    // 기존 공유 메모리 객체 열기
    shared_memory->fd = shm_open(shared_memory->name, O_RDWR, 0);
    if (shared_memory->fd == -1) {
        free(shared_memory);
        switch (errno) {
            case ENOENT:
                return ET_ERROR_NOT_FOUND;
            case EACCES:
                return ET_ERROR_ACCESS_DENIED;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    // 크기 확인
    struct stat st;
    if (fstat(shared_memory->fd, &st) == -1) {
        close(shared_memory->fd);
        free(shared_memory);
        return ET_ERROR_PLATFORM_SPECIFIC;
    }
    shared_memory->size = st.st_size;

    *shm = shared_memory;
    return ET_SUCCESS;
}static void
* posix_map_shared_memory(ETSharedMemory* shm) {
    if (shm == NULL || shm->fd == -1) {
        return NULL;
    }

    if (shm->mapped_address != NULL) {
        return shm->mapped_address; // 이미 매핑됨
    }

    shm->mapped_address = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->mapped_address == MAP_FAILED) {
        shm->mapped_address = NULL;
        return NULL;
    }

    return shm->mapped_address;
}

static ETResult posix_unmap_shared_memory(ETSharedMemory* shm, void* addr) {
    if (shm == NULL || addr == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (munmap(addr, shm->size) == 0) {
        if (shm->mapped_address == addr) {
            shm->mapped_address = NULL;
        }
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

static void posix_destroy_shared_memory(ETSharedMemory* shm) {
    if (shm == NULL) {
        return;
    }

    if (shm->mapped_address != NULL) {
        munmap(shm->mapped_address, shm->size);
    }

    if (shm->fd != -1) {
        close(shm->fd);
        shm_unlink(shm->name); // 공유 메모리 객체 삭제
    }

    free(shm);
}//
 ============================================================================
// 메모리 매핑 파일 함수들
// ============================================================================

static ETResult posix_create_memory_map(const char* filename, size_t size, ETMemoryMapMode mode, ETMemoryMap** map) {
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
    int flags = O_RDONLY;
    if (mode & ET_MEMORY_MAP_WRITE) {
        flags = O_RDWR | O_CREAT;
    }

    memory_map->fd = open(filename, flags, 0666);
    if (memory_map->fd == -1) {
        free(memory_map);
        switch (errno) {
            case ENOENT:
                return ET_ERROR_NOT_FOUND;
            case EACCES:
                return ET_ERROR_ACCESS_DENIED;
            default:
                return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    // 파일 크기 확인 및 설정
    struct stat st;
    if (fstat(memory_map->fd, &st) == -1) {
        close(memory_map->fd);
        free(memory_map);
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    if (st.st_size < (off_t)size && (mode & ET_MEMORY_MAP_WRITE)) {
        if (ftruncate(memory_map->fd, size) == -1) {
            close(memory_map->fd);
            free(memory_map);
            return ET_ERROR_PLATFORM_SPECIFIC;
        }
    }

    *map = memory_map;
    return ET_SUCCESS;
}

static void* posix_map_file(ETMemoryMap* map, size_t offset, size_t length) {
    if (map == NULL || map->fd == -1) {
        return NULL;
    }

    int prot = et_memory_map_mode_to_posix_prot(map->mode);
    int flags = et_memory_map_mode_to_posix_flags(map->mode);

    void* mapped_address = mmap(NULL, length, prot, flags, map->fd, offset);
    if (mapped_address == MAP_FAILED) {
        return NULL;
    }

    if (map->mapped_address == NULL) {
        map->mapped_address = mapped_address;
    }

    return mapped_address;
}static ET
Result posix_unmap_file(ETMemoryMap* map, void* addr, size_t length) {
    if (map == NULL || addr == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (munmap(addr, length) == 0) {
        if (map->mapped_address == addr) {
            map->mapped_address = NULL;
        }
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

static void posix_destroy_memory_map(ETMemoryMap* map) {
    if (map == NULL) {
        return;
    }

    if (map->mapped_address != NULL) {
        munmap(map->mapped_address, map->size);
    }

    if (map->fd != -1) {
        close(map->fd);
    }

    free(map);
}

// ============================================================================
// 메모리 정보 및 통계 함수들
// ============================================================================

static ETResult posix_get_memory_info(void* ptr, ETMemoryInfo* info) {
    if (ptr == NULL || info == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // POSIX에서는 메모리 정보를 직접 조회하는 표준 방법이 없음
    // 기본값으로 설정
    info->address = ptr;
    info->size = 0; // 알 수 없음
    info->alignment = 0; // 알 수 없음
    info->protection = ET_MEMORY_PROTECT_READ | ET_MEMORY_PROTECT_WRITE; // 기본값

    return ET_SUCCESS;
}

static ETResult posix_get_memory_stats(ETMemoryStats* stats) {
    if (stats == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *stats = g_posix_memory_stats;
    return ET_SUCCESS;
}// =====
=======================================================================
// POSIX 메모리 인터페이스 생성
// ============================================================================

ETResult et_create_posix_memory_interface(ETMemoryInterface** interface) {
    if (interface == NULL) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMemoryInterface* memory_interface = (ETMemoryInterface*)malloc(sizeof(ETMemoryInterface));
    if (memory_interface == NULL) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 함수 포인터 설정
    memory_interface->malloc = posix_malloc;
    memory_interface->calloc = posix_calloc;
    memory_interface->realloc = posix_realloc;
    memory_interface->free = posix_free;

    memory_interface->aligned_malloc = posix_aligned_malloc;
    memory_interface->aligned_free = posix_aligned_free;

    memory_interface->lock_pages = posix_lock_pages;
    memory_interface->unlock_pages = posix_unlock_pages;
    memory_interface->protect_pages = posix_protect_pages;

    memory_interface->create_shared_memory = posix_create_shared_memory;
    memory_interface->open_shared_memory = posix_open_shared_memory;
    memory_interface->map_shared_memory = posix_map_shared_memory;
    memory_interface->unmap_shared_memory = posix_unmap_shared_memory;
    memory_interface->destroy_shared_memory = posix_destroy_shared_memory;

    memory_interface->create_memory_map = posix_create_memory_map;
    memory_interface->map_file = posix_map_file;
    memory_interface->unmap_file = posix_unmap_file;
    memory_interface->destroy_memory_map = posix_destroy_memory_map;

    memory_interface->get_memory_info = posix_get_memory_info;
    memory_interface->get_memory_stats = posix_get_memory_stats;

    memory_interface->platform_data = NULL;

    // 통계 초기화
    memset(&g_posix_memory_stats, 0, sizeof(ETMemoryStats));

    *interface = memory_interface;
    return ET_SUCCESS;
}

void et_destroy_posix_memory_interface(ETMemoryInterface* interface) {
    if (interface != NULL) {
        free(interface);
    }
}