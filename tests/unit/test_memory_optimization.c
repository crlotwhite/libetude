/**
 * @file test_memory_optimization.c
 * @brief LibEtude 메모리 최적화 전략 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/memory_optimization.h"
#include "libetude/memory.h"
#include <string.h>

static ETMemoryPool* test_pool = NULL;

void setUp(void) {
    test_pool = NULL;
}

void tearDown(void) {
    if (test_pool) {
        et_destroy_memory_pool(test_pool);
        test_pool = NULL;
    }
}

void test_inplace_context_creation(void) {
    printf("인플레이스 컨텍스트 생성 테스트...\n");

    // 기본 컨텍스트 생성
    ETInPlaceContext* ctx = et_create_inplace_context(1024, 32, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(ctx, "인플레이스 컨텍스트 생성 실패");
    TEST_ASSERT_EQUAL_MESSAGE(1024, ctx->buffer_size, "버퍼 크기 불일치");
    TEST_ASSERT_EQUAL_MESSAGE(32, ctx->alignment, "정렬 크기 불일치");

    // 컨텍스트 정리
    et_destroy_inplace_context(ctx);

    printf("인플레이스 컨텍스트 생성 테스트 완료\n");
}

void test_memory_reuse(void) {
    printf("메모리 재사용 테스트...\n");

    // 메모리 풀 생성
    test_pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 메모리 할당 및 해제 반복
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = et_memory_pool_alloc(test_pool, 1024);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    // 메모리 해제
    for (int i = 0; i < 10; i++) {
        et_memory_pool_free(test_pool, ptrs[i]);
    }

    printf("메모리 재사용 테스트 완료\n");
}

void test_fragmentation_prevention(void) {
    printf("메모리 단편화 방지 테스트...\n");

    // 메모리 풀 생성
    test_pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 다양한 크기의 메모리 할당
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        size_t size = (i % 4 + 1) * 256; // 256, 512, 768, 1024 bytes
        ptrs[i] = et_memory_pool_alloc(test_pool, size);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    // 일부 메모리 해제 (단편화 유발)
    for (int i = 1; i < 20; i += 2) {
        et_memory_pool_free(test_pool, ptrs[i]);
        ptrs[i] = NULL;
    }

    // 새로운 메모리 할당 (단편화된 공간 재사용)
    for (int i = 1; i < 20; i += 2) {
        ptrs[i] = et_memory_pool_alloc(test_pool, 256);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    // 모든 메모리 해제
    for (int i = 0; i < 20; i++) {
        if (ptrs[i]) {
            et_memory_pool_free(test_pool, ptrs[i]);
        }
    }

    printf("메모리 단편화 방지 테스트 완료\n");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_inplace_context_creation);
    RUN_TEST(test_memory_reuse);
    RUN_TEST(test_fragmentation_prevention);

    return UNITY_END();
}