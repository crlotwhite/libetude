/**
 * @file test_memory_optimization_simple.c
 * @brief 메모리 최적화 기능 간단 테스트
 */

#include "include/libetude/memory_optimization.h"
#include "include/libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 간단한 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return -1; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

int test_inplace_context() {
    printf("\n=== 인플레이스 컨텍스트 테스트 ===\n");

    // 컨텍스트 생성
    ETInPlaceContext* ctx = et_create_inplace_context(1024, 32, false);
    TEST_ASSERT(ctx != NULL, "인플레이스 컨텍스트 생성");
    TEST_ASSERT(ctx->buffer_size == 1024, "버퍼 크기 확인");
    TEST_ASSERT(ctx->alignment == 32, "정렬 크기 확인");

    // 테스트 데이터
    char src[256], dest[256];
    for (int i = 0; i < 256; i++) {
        src[i] = (char)i;
        dest[i] = 0;
    }

    // 인플레이스 복사 테스트
    int result = et_inplace_memcpy(ctx, dest, src, 256);
    TEST_ASSERT(result == 0, "인플레이스 memcpy 성공");
    TEST_ASSERT(memcmp(src, dest, 256) == 0, "복사된 데이터 일치");

    // 스왑 테스트
    char data1[64], data2[64];
    for (int i = 0; i < 64; i++) {
        data1[i] = (char)i;
        data2[i] = (char)(255 - i);
    }

    char expected1[64], expected2[64];
    memcpy(expected1, data2, 64);
    memcpy(expected2, data1, 64);

    result = et_inplace_swap(ctx, data1, data2, 64);
    TEST_ASSERT(result == 0, "인플레이스 스왑 성공");
    TEST_ASSERT(memcmp(data1, expected1, 64) == 0, "스왑 결과 확인 (data1)");
    TEST_ASSERT(memcmp(data2, expected2, 64) == 0, "스왑 결과 확인 (data2)");

    et_destroy_inplace_context(ctx);
    printf("✓ 인플레이스 컨텍스트 테스트 완료\n");
    return 0;
}

int test_reuse_pool() {
    printf("\n=== 메모리 재사용 풀 테스트 ===\n");

    // 재사용 풀 생성
    ETMemoryReusePool* pool = et_create_reuse_pool(64, 1024, 8, false);
    TEST_ASSERT(pool != NULL, "재사용 풀 생성");

    // 할당 테스트
    void* ptr1 = et_reuse_alloc(pool, 128);
    TEST_ASSERT(ptr1 != NULL, "첫 번째 할당");

    void* ptr2 = et_reuse_alloc(pool, 256);
    TEST_ASSERT(ptr2 != NULL, "두 번째 할당");

    // 해제 및 재할당
    et_reuse_free(pool, ptr1, 128);
    void* ptr3 = et_reuse_alloc(pool, 128);
    TEST_ASSERT(ptr3 != NULL, "재할당");
    TEST_ASSERT(ptr3 == ptr1, "재사용된 포인터 확인");

    // 통계 확인
    size_t total_requests, reuse_hits;
    float hit_rate;
    et_get_reuse_pool_stats(pool, &total_requests, &reuse_hits, &hit_rate);
    printf("  총 요청: %zu, 재사용 히트: %zu, 성공률: %.2f%%\n",
           total_requests, reuse_hits, hit_rate * 100.0f);

    et_reuse_free(pool, ptr2, 256);
    et_reuse_free(pool, ptr3, 128);
    et_destroy_reuse_pool(pool);

    printf("✓ 메모리 재사용 풀 테스트 완료\n");
    return 0;
}

int test_utility_functions() {
    printf("\n=== 유틸리티 함수 테스트 ===\n");

    // 2의 거듭제곱 올림 테스트
    TEST_ASSERT(et_round_up_to_power_of_2(0) == 1, "0 -> 1");
    TEST_ASSERT(et_round_up_to_power_of_2(1) == 1, "1 -> 1");
    TEST_ASSERT(et_round_up_to_power_of_2(3) == 4, "3 -> 4");
    TEST_ASSERT(et_round_up_to_power_of_2(5) == 8, "5 -> 8");
    TEST_ASSERT(et_round_up_to_power_of_2(16) == 16, "16 -> 16");
    TEST_ASSERT(et_round_up_to_power_of_2(17) == 32, "17 -> 32");
    TEST_ASSERT(et_round_up_to_power_of_2(100) == 128, "100 -> 128");

    printf("✓ 유틸리티 함수 테스트 완료\n");
    return 0;
}

int main() {
    printf("=== LibEtude 메모리 최적화 간단 테스트 ===\n");

    if (test_inplace_context() != 0) return 1;
    if (test_reuse_pool() != 0) return 1;
    if (test_utility_functions() != 0) return 1;

    printf("\n=== 모든 테스트 통과! ===\n");
    return 0;
}