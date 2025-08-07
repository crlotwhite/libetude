#ifndef LIBETUDE_PLATFORM_MEMORY_H
#define LIBETUDE_PLATFORM_MEMORY_H

#include "common.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 메모리 매핑 파일 핸들
typedef struct ETMemoryMap ETMemoryMap;

// 공유 메모리 핸들
typedef struct ETSharedMemory ETSharedMemory;

// 메모리 매핑 모드
typedef enum {
    ET_MEMORY_MAP_READ = 0x01,
    ET_MEMORY_MAP_WRITE = 0x02,
    ET_MEMORY_MAP_EXECUTE = 0x04,
    ET_MEMORY_MAP_PRIVATE = 0x08,
    ET_MEMORY_MAP_SHARED = 0x10
} ETMemoryMapMode;

// 메모리 보호 모드
typedef enum {
    ET_MEMORY_PROTECT_NONE = 0x00,
    ET_MEMORY_PROTECT_READ = 0x01,
    ET_MEMORY_PROTECT_WRITE = 0x02,
    ET_MEMORY_PROTECT_EXECUTE = 0x04
} ETMemoryProtection;

// 메모리 할당 정보
typedef struct {
    void* address;              // 할당된 주소
    size_t size;                // 할당된 크기
    size_t alignment;           // 메모리 정렬
    ETMemoryProtection protection; // 메모리 보호 모드
} ETMemoryInfo;

// 메모리 통계 정보
typedef struct {
    uint64_t total_allocated;   // 총 할당된 메모리
    uint64_t peak_allocated;    // 최대 할당된 메모리
    uint32_t allocation_count;  // 할당 횟수
    uint32_t free_count;        // 해제 횟수
} ETMemoryStats;

// 메모리 관리 인터페이스
typedef struct ETMemoryInterface {
    // 기본 메모리 할당
    void* (*malloc)(size_t size);
    void* (*calloc)(size_t count, size_t size);
    void* (*realloc)(void* ptr, size_t size);
    void (*free)(void* ptr);

    // 정렬된 메모리 할당
    void* (*aligned_malloc)(size_t size, size_t alignment);
    void (*aligned_free)(void* ptr);

    // 메모리 페이지 관리
    ETResult (*lock_pages)(void* addr, size_t len);
    ETResult (*unlock_pages)(void* addr, size_t len);
    ETResult (*protect_pages)(void* addr, size_t len, ETMemoryProtection protection);

    // 공유 메모리
    ETResult (*create_shared_memory)(const char* name, size_t size, ETSharedMemory** shm);
    ETResult (*open_shared_memory)(const char* name, ETSharedMemory** shm);
    void* (*map_shared_memory)(ETSharedMemory* shm);
    ETResult (*unmap_shared_memory)(ETSharedMemory* shm, void* addr);
    void (*destroy_shared_memory)(ETSharedMemory* shm);

    // 메모리 매핑 파일
    ETResult (*create_memory_map)(const char* filename, size_t size, ETMemoryMapMode mode, ETMemoryMap** map);
    void* (*map_file)(ETMemoryMap* map, size_t offset, size_t length);
    ETResult (*unmap_file)(ETMemoryMap* map, void* addr, size_t length);
    void (*destroy_memory_map)(ETMemoryMap* map);

    // 메모리 정보 및 통계
    ETResult (*get_memory_info)(void* ptr, ETMemoryInfo* info);
    ETResult (*get_memory_stats)(ETMemoryStats* stats);

    // 플랫폼별 확장 데이터
    void* platform_data;
} ETMemoryInterface;

// 공통 메모리 관리 함수들
ETResult et_memory_init(void);
void et_memory_cleanup(void);
ETMemoryInterface* et_get_memory_interface(void);

// 메모리 유틸리티 함수들
ETResult et_memory_set_zero(void* ptr, size_t size);
ETResult et_memory_copy(void* dest, const void* src, size_t size);
ETResult et_memory_compare(const void* ptr1, const void* ptr2, size_t size, int* result);
bool et_memory_is_aligned(const void* ptr, size_t alignment);
size_t et_memory_get_page_size(void);

// 편의 래퍼 함수들
void* et_malloc(size_t size);
void* et_calloc(size_t count, size_t size);
void* et_realloc(void* ptr, size_t size);
void et_free(void* ptr);
void* et_aligned_malloc(size_t size, size_t alignment);
void et_aligned_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_MEMORY_H