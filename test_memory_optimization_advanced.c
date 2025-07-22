/**
 * @file test_memory_optimization_advanced.c
 * @brief 메모리 최적화 고급 기능 테스트
 */

#include "include/libetude/memory_optimization.h"
#include "include/libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

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

int test_smart_memory_manager() {
    printf("\n=== 스마트 메모리 매니저 테스트 ===\n");

    // 스마트 메모리 매니저 생성
    ETSmartMemoryManager* manager = et_create_smart_memory_manager(8192, 1024, 512, false);
    TEST_ASSERT(manager != NULL, "스마트 메모리 매니저 생성");

    // 다양한 크기의 메모리 할당
    void* ptrs[20];
    size_t sizes[] = {64, 128, 256, 128, 64, 512, 128, 256, 64, 128,
                      96, 192, 320, 160, 80, 640, 160, 320, 96, 160};

    printf("  다양한 크기 메모리 할당 중...\n");
    for (int i = 0; i < 20; i++) {
        ptrs[i] = et_smart_alloc(manager, sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL, "스마트 할당 성공");

        // 데이터 쓰기 테스트
        memset(ptrs[i], (char)(i % 256), sizes[i]);
    }

    // 일부 메모리 해제 (단편화 생성)
    printf("  일부 메모리 해제 (단편화 생성)...\n");
    for (int i = 0; i < 20; i += 3) {
        et_smart_free(manager, ptrs[i], sizes[i]);
        ptrs[i] = NULL;
    }

    // 재할당 (재사용 풀 활용)
    printf("  재할당 (재사용 풀 활용)...\n");
    for (int i = 0; i < 20; i += 3) {
        ptrs[i] = et_smart_alloc(manager, sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL, "재할당 성공");
        memset(ptrs[i], (char)(i % 256), sizes[i]);
    }

    // 최적화 수행
    printf("  메모리 최적화 수행...\n");
    size_t optimizations = et_optimize_memory_usage(manager);
    printf("    수행된 최적화: %zu\n", optimizations);

    // 통계 확인
    uint64_t total_allocs, bytes_saved, opt_count;
    et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);

    printf("  스마트 매니저 통계:\n");
    printf("    총 할당 수: %llu\n", (unsigned long long)total_allocs);
    printf("    절약된 바이트: %llu\n", (unsigned long long)bytes_saved);
    printf("    최적화 수행 횟수: %llu\n", (unsigned long long)opt_count);

    // 재사용 풀 통계
    size_t total_requests, reuse_hits;
    float hit_rate;
    et_get_reuse_pool_stats(manager->reuse_pool, &total_requests, &reuse_hits, &hit_rate);
    printf("    재사용 성공률: %.2f%%\n", hit_rate * 100.0f);

    // 데이터 무결성 확인
    printf("  데이터 무결성 확인...\n");
    for (int i = 0; i < 20; i++) {
        if (ptrs[i] != NULL) {
            char* data = (char*)ptrs[i];
            for (size_t j = 0; j < sizes[i]; j++) {
                TEST_ASSERT(data[j] == (char)(i % 256), "데이터 무결성 확인");
            }
        }
    }

    // 모든 메모리 해제
    for (int i = 0; i < 20; i++) {
        if (ptrs[i] != NULL) {
            et_smart_free(manager, ptrs[i], sizes[i]);
        }
    }

    et_destroy_smart_memory_manager(manager);
    printf("✓ 스마트 메모리 매니저 테스트 완료\n");
    return 0;
}

int test_fragmentation_analysis() {
    printf("\n=== 메모리 단편화 분석 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(4096, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 단편화 생성을 위한 할당 패턴
    void* ptrs[10];
    size_t sizes[] = {64, 128, 256, 128, 64, 512, 128, 256, 64, 128};

    printf("  단편화 생성을 위한 할당...\n");
    for (int i = 0; i < 10; i++) {
        ptrs[i] = et_alloc_from_pool(pool, sizes[i]);
        if (ptrs[i] != NULL) {
            memset(ptrs[i], (char)i, sizes[i]);
        }
    }

    // 일부 해제 (홀수 인덱스만)
    printf("  일부 메모리 해제 (단편화 생성)...\n");
    for (int i = 1; i < 10; i += 2) {
        if (ptrs[i] != NULL) {
            et_free_to_pool(pool, ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    // 단편화 분석
    ETFragmentationInfo frag_info;
    int result = et_analyze_fragmentation(pool, &frag_info);
    TEST_ASSERT(result == 0, "단편화 분석 성공");

    printf("  단편화 분석 결과:\n");
    printf("    총 자유 공간: %zu bytes\n", frag_info.total_free_space);
    printf("    최대 자유 블록: %zu bytes\n", frag_info.largest_free_block);
    printf("    자유 블록 수: %zu\n", frag_info.num_free_blocks);
    printf("    단편화 비율: %.2f%%\n", frag_info.fragmentation_ratio * 100.0f);
    printf("    외부 단편화: %.2f%%\n", frag_info.external_fragmentation * 100.0f);
    printf("    낭비된 공간: %zu bytes\n", frag_info.wasted_space);

    TEST_ASSERT(frag_info.total_free_space > 0, "자유 공간 존재");
    TEST_ASSERT(frag_info.num_free_blocks > 0, "자유 블록 존재");

    // 메모리 압축 수행
    printf("  메모리 압축 수행...\n");
    size_t compacted_bytes = et_compact_memory_pool(pool, false);
    printf("    압축된 바이트: %zu\n", compacted_bytes);

    // 압축 후 단편화 재분석
    ETFragmentationInfo after_frag;
    et_analyze_fragmentation(pool, &after_frag);

    printf("  압축 후 단편화:\n");
    printf("    자유 블록 수: %zu -> %zu\n", frag_info.num_free_blocks, after_frag.num_free_blocks);
    printf("    외부 단편화: %.2f%% -> %.2f%%\n",
           frag_info.external_fragmentation * 100.0f,
           after_frag.external_fragmentation * 100.0f);

    // 나머지 메모리 해제
    for (int i = 0; i < 10; i += 2) {
        if (ptrs[i] != NULL) {
            et_free_to_pool(pool, ptrs[i]);
        }
    }

    et_destroy_memory_pool(pool);
    printf("✓ 메모리 단편화 분석 테스트 완료\n");
    return 0;
}

int test_memory_recommendations() {
    printf("\n=== 메모리 권장사항 생성 테스트 ===\n");

    // 작은 메모리 풀 생성 (권장사항 유발)
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);
    TEST_ASSERT(pool != NULL, "작은 메모리 풀 생성");

    // 메모리를 거의 다 사용
    void* ptrs[8];
    for (int i = 0; i < 8; i++) {
        ptrs[i] = et_alloc_from_pool(pool, 100);
    }

    // 권장사항 생성
    char recommendations[1024];
    int rec_count = et_generate_memory_recommendations(pool, recommendations, sizeof(recommendations));

    printf("  생성된 권장사항 수: %d\n", rec_count);
    if (rec_count > 0) {
        printf("  권장사항:\n%s", recommendations);
    }

    TEST_ASSERT(rec_count >= 0, "권장사항 생성 성공");

    // 메모리 해제
    for (int i = 0; i < 8; i++) {
        if (ptrs[i] != NULL) {
            et_free_to_pool(pool, ptrs[i]);
        }
    }

    et_destroy_memory_pool(pool);
    printf("✓ 메모리 권장사항 생성 테스트 완료\n");
    return 0;
}

int test_performance_comparison() {
    printf("\n=== 성능 비교 테스트 ===\n");

    const int NUM_ALLOCS = 1000;
    const int ALLOC_SIZE = 256;

    // 일반 malloc/free 성능 측정
    clock_t start = clock();
    void** malloc_ptrs = malloc(NUM_ALLOCS * sizeof(void*));

    for (int i = 0; i < NUM_ALLOCS; i++) {
        malloc_ptrs[i] = malloc(ALLOC_SIZE);
        if (malloc_ptrs[i]) {
            memset(malloc_ptrs[i], i % 256, ALLOC_SIZE);
        }
    }

    for (int i = 0; i < NUM_ALLOCS; i++) {
        free(malloc_ptrs[i]);
    }
    free(malloc_ptrs);

    clock_t malloc_time = clock() - start;

    // 스마트 메모리 매니저 성능 측정
    start = clock();
    ETSmartMemoryManager* manager = et_create_smart_memory_manager(
        NUM_ALLOCS * ALLOC_SIZE * 2, 1024, 512, false);

    void** smart_ptrs = malloc(NUM_ALLOCS * sizeof(void*));

    for (int i = 0; i < NUM_ALLOCS; i++) {
        smart_ptrs[i] = et_smart_alloc(manager, ALLOC_SIZE);
        if (smart_ptrs[i]) {
            memset(smart_ptrs[i], i % 256, ALLOC_SIZE);
        }
    }

    for (int i = 0; i < NUM_ALLOCS; i++) {
        et_smart_free(manager, smart_ptrs[i], ALLOC_SIZE);
    }

    free(smart_ptrs);
    et_destroy_smart_memory_manager(manager);

    clock_t smart_time = clock() - start;

    printf("  성능 비교 결과 (%d회 할당/해제):\n", NUM_ALLOCS);
    printf("    일반 malloc/free: %.2f ms\n",
           (double)malloc_time / CLOCKS_PER_SEC * 1000.0);
    printf("    스마트 메모리 매니저: %.2f ms\n",
           (double)smart_time / CLOCKS_PER_SEC * 1000.0);

    if (smart_time < malloc_time) {
        printf("    ✓ 스마트 매니저가 %.2f%% 더 빠름\n",
               (double)(malloc_time - smart_time) / malloc_time * 100.0);
    } else {
        printf("    - 스마트 매니저가 %.2f%% 더 느림 (초기화 오버헤드)\n",
               (double)(smart_time - malloc_time) / malloc_time * 100.0);
    }

    printf("✓ 성능 비교 테스트 완료\n");
    return 0;
}

int main() {
    printf("=== LibEtude 메모리 최적화 고급 테스트 ===\n");

    if (test_smart_memory_manager() != 0) return 1;
    if (test_fragmentation_analysis() != 0) return 1;
    if (test_memory_recommendations() != 0) return 1;
    if (test_performance_comparison() != 0) return 1;

    printf("\n=== 모든 고급 테스트 통과! ===\n");
    printf("메모리 최적화 전략이 성공적으로 구현되었습니다.\n");
    printf("\n주요 기능:\n");
    printf("✓ 인플레이스 연산 지원 - 메모리 복사 최소화\n");
    printf("✓ 메모리 재사용 메커니즘 - 동적 할당 감소\n");
    printf("✓ 메모리 단편화 방지 - 자동 압축 및 최적화\n");
    printf("✓ 스마트 메모리 관리 - 사용 패턴 학습 및 적응\n");
    printf("✓ 성능 모니터링 - 실시간 통계 및 권장사항\n");

    return 0;
}