/**
 * @file debug_memory.c
 * @brief 메모리 할당자 디버깅용 간단한 테스트
 */

#include "libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("메모리 할당자 디버깅 시작...\n");

    // 1. 메모리 풀 생성 테스트
    printf("1. 메모리 풀 생성 테스트\n");
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    if (pool == NULL) {
        printf("메모리 풀 생성 실패\n");
        return 1;
    }
    printf("메모리 풀 생성 성공: %p\n", pool);

    // 2. 런타임 할당자 생성 테스트
    printf("2. 런타임 할당자 생성 테스트\n");
    RTAllocator* allocator = rt_create_allocator(1024 * 1024, 32);
    if (allocator == NULL) {
        printf("런타임 할당자 생성 실패\n");
        et_destroy_memory_pool(pool);
        return 1;
    }
    printf("런타임 할당자 생성 성공: %p\n", allocator);

    // 3. 기본 할당 테스트
    printf("3. 기본 할당 테스트\n");
    void* ptr = rt_alloc(allocator, 128);
    if (ptr == NULL) {
        printf("메모리 할당 실패\n");
    } else {
        printf("메모리 할당 성공: %p\n", ptr);

        // 4. 메모리 해제 테스트
        printf("4. 메모리 해제 테스트\n");
        rt_free(allocator, ptr);
        printf("메모리 해제 완료\n");
    }

    // 5. 할당자 소멸 테스트
    printf("5. 할당자 소멸 테스트\n");
    rt_destroy_allocator(allocator);
    printf("할당자 소멸 완료\n");

    // 6. 메모리 풀 소멸 테스트
    printf("6. 메모리 풀 소멸 테스트\n");
    et_destroy_memory_pool(pool);
    printf("메모리 풀 소멸 완료\n");

    printf("모든 테스트 완료\n");
    return 0;
}