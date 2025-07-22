/**
 * @file test_memory_optimization.c
 * @brief LibEtude 메모리 최적화 전략 테스트
 *
 * 인플레이스 연산, 메모리 재사용, 단편화 방지 기능을 테스트합니다.
 */

#include "tests/framework/test_framework.h"
#include "libetude/memory_optimization.h"
#include "libetude/memory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 테스트 헬퍼 함수
static void create_fragmented_pool(ETMemoryPool* pool);
static void verify_memory_pattern(void* ptr, size_t size, uint8_t pattern);

// =============================================================================
// 인플레이스 연산 테스트
// =============================================================================

void test_inplace_context_creation(void) {
    printf("인플레이스 컨텍스트 생성 테스트...\n");

    // 기본 컨텍스트 생성
    ETInPlaceContext* ctx = et_create_inplace_context(1024, 32, true);
    TEST_ASSERT_NOT_NULL(ctx, "인플레이스 컨텍스트 생성 실패");
    TEST_ASSERT_EQUAL(ctx->buffer_size, 1024, "버퍼 크기 불일치");
    TEST_ASSERT_EQUAL(ctx->alignment, 32, "정렬 크기 불일치");
    TEST_ASSERT_TRUE(ctx->thread_safe, "스레드 안전성 설정 실패");
    TEST_ASSERT_FALSE(ctx->is_external, "외부 버퍼 플래그 오류");

    et_destroy_inplace_context(ctx);

    // 외부 버퍼 사용 컨텍스트
    void* external_buffer = aligned_alloc(64, 2048);
    TEST_ASSERT_NOT_NULL(external_buffer, "외부 버퍼 할당 실패");

    ctx = et_create_inplace_context_from_buffer(external_buffer, 2048, 64, false);
    TEST_ASSERT_NOT_NULL(ctx, "외부 버퍼 컨텍스트 생성 실패");
    TEST_ASSERT_EQUAL(ctx->buffer, external_buffer, "외부 버퍼 주소 불일치");
    TEST_ASSERT_TRUE(ctx->is_external, "외부 버퍼 플래그 오류");

    et_destroy_inplace_context(ctx);
    free(external_buffer);

    printf("✓ 인플레이스 컨텍스트 생성 테스트 통과\n");
}

void test_inplace_operations(void) {
    printf("인플레이스 연산 테스트...\n");

    ETInPlaceContext* ctx = et_create_inplace_context(1024, 32, false);
    TEST_ASSERT_NOT_NULL(ctx, "컨텍스트 생성 실패");

    // 테스트 데이터 준비
    uint8_t src_data[256];
    uint8_t dest_data[256];
    uint8_t backup_data[256];

    for (int i = 0; i < 256; i++) {
        src_data[i] = (uint8_t)i;
        dest_data[i] = 0;
        backup_data[i] = (uint8_t)i;
    }

    // 인플레이스 메모리 복사 테스트
    int result = et_inplace_memcpy(ctx, dest_data, src_data, 256);
    TEST_ASSERT_EQUAL(result, 0, "인플레이스 memcpy 실패");
    TEST_ASSERT_EQUAL(memcmp(dest_data, backup_data, 256), 0, "복사된 데이터 불일치");
    TEST_ASSERT_TRUE(ctx->operation_count > 0, "연산 카운트 업데이트 실패");

    // 겹치는 영역 복사 테스트
    uint8_t overlap_data[512];
    for (int i = 0; i < 512; i++) {
        overlap_data[i] = (uint8_t)(i % 256);
    }

    result = et_inplace_memcpy(ctx, overlap_data + 128, overlap_data, 256);
    TEST_ASSERT_EQUAL(result, 0, "겹치는 영역 복사 실패");

    // 인플레이스 스왑 테스트
    uint8_t data1[128], data2[128];
    for (int i = 0; i < 128; i++) {
        data1[i] = (uint8_t)i;
        data2[i] = (uint8_t)(255 - i);
    }

    uint8_t expected1[128], expected2[128];
    memcpy(expected1, data2, 128);
    memcpy(expected2, data1, 128);

    result = et_inplace_swap(ctx, data1, data2, 128);
    TEST_ASSERT_EQUAL(result, 0, "인플레이스 스왑 실패");
    TEST_ASSERT_EQUAL(memcmp(data1, expected1, 128), 0, "스왑 결과 불일치 (data1)");
    TEST_ASSERT_EQUAL(memcmp(data2, expected2, 128), 0, "스왑 결과 불일치 (data2)");

    et_destroy_inplace_context(ctx);

    printf("✓ 인플레이스 연산 테스트 통과\n");
}

// =============================================================================
// 메모리 재사용 풀 테스트
// =============================================================================

void test_reuse_pool_creation(void) {
    printf("메모리 재사용 풀 생성 테스트...\n");

    ETMemoryReusePool* pool = et_create_reuse_pool(64, 4096, 16, true);
    TEST_ASSERT_NOT_NULL(pool, "재사용 풀 생성 실패");
    TEST_ASSERT_EQUAL(pool->min_size, 64, "최소 크기 설정 오류");
    TEST_ASSERT_EQUAL(pool->max_size, 4096, "최대 크기 설정 오류");
    TEST_ASSERT_TRUE(pool->thread_safe, "스레드 안전성 설정 실패");

    et_destroy_reuse_pool(pool);

    printf("✓ 메모리 재사용 풀 생성 테스트 통과\n");
}

void test_reuse_pool_operations(void) {
    printf("메모리 재사용 풀 연산 테스트...\n");

    ETMemoryReusePool* pool = et_create_reuse_pool(64, 1024, 8, false);
    TEST_ASSERT_NOT_NULL(pool, "재사용 풀 생성 실패");

    // 첫 번째 할당 (캐시 미스)
    void* ptr1 = et_reuse_alloc(pool, 128);
    TEST_ASSERT_NOT_NULL(ptr1, "첫 번째 할당 실패");

    void* ptr2 = et_reuse_alloc(pool, 256);
    TEST_ASSERT_NOT_NULL(ptr2, "두 번째 할당 실패");

    // 통계 확인
    size_t total_requests, reuse_hits;
    float hit_rate;
    et_get_reuse_pool_stats(pool, &total_requests, &reuse_hits, &hit_rate);
    TEST_ASSERT_EQUAL(total_requests, 2, "총 요청 수 불일치");
    TEST_ASSERT_EQUAL(reuse_hits, 0, "재사용 히트 수 오류"); // 아직 재사용 없음

    // 메모리 반환
    et_reuse_free(pool, ptr1, 128);
    et_reuse_free(pool, ptr2, 256);

    // 재할당 (캐시 히트 예상)
    void* ptr3 = et_reuse_alloc(pool, 128);
    TEST_ASSERT_NOT_NULL(ptr3, "재할당 실패");
    TEST_ASSERT_EQUAL(ptr3, ptr1, "재사용된 포인터 불일치"); // 같은 포인터 재사용

    void* ptr4 = et_reuse_alloc(pool, 256);
    TEST_ASSERT_NOT_NULL(ptr4, "재할당 실패");

    // 재사용 통계 확인
    et_get_reuse_pool_stats(pool, &total_requests, &reuse_hits, &hit_rate);
    TEST_ASSERT_TRUE(reuse_hits > 0, "재사용 히트 수 업데이트 실패");
    TEST_ASSERT_TRUE(hit_rate > 0.0f, "재사용 성공률 계산 오류");

    // 범위 밖 크기 테스트
    void* ptr_large = et_reuse_alloc(pool, 2048); // max_size 초과
    TEST_ASSERT_NOT_NULL(ptr_large, "범위 밖 할당 실패");
    free(ptr_large); // 일반 해제

    void* ptr_small = et_reuse_alloc(pool, 32); // min_size 미만
    TEST_ASSERT_NOT_NULL(ptr_small, "범위 밖 할당 실패");
    free(ptr_small); // 일반 해제

    et_reuse_free(pool, ptr3, 128);
    et_reuse_free(pool, ptr4, 256);

    et_destroy_reuse_pool(pool);

    printf("✓ 메모리 재사용 풀 연산 테스트 통과\n");
}

void test_reuse_pool_cleanup(void) {
    printf("메모리 재사용 풀 정리 테스트...\n");

    ETMemoryReusePool* pool = et_create_reuse_pool(64, 512, 4, false);
    TEST_ASSERT_NOT_NULL(pool, "재사용 풀 생성 실패");

    // 여러 할당 및 해제로 풀 채우기
    void* ptrs[8];
    for (int i = 0; i < 8; i++) {
        ptrs[i] = et_reuse_alloc(pool, 128);
        TEST_ASSERT_NOT_NULL(ptrs[i], "할당 실패");
    }

    for (int i = 0; i < 8; i++) {
        et_reuse_free(pool, ptrs[i], 128);
    }

    // 강제 정리 수행
    size_t cleaned = et_cleanup_reuse_pool(pool, true);
    TEST_ASSERT_TRUE(cleaned > 0, "정리된 버퍼 수 오류");

    et_destroy_reuse_pool(pool);

    printf("✓ 메모리 재사용 풀 정리 테스트 통과\n");
}

// =============================================================================
// 메모리 단편화 방지 테스트
// =============================================================================

void test_fragmentation_analysis(void) {
    printf("메모리 단편화 분석 테스트...\n");

    ETMemoryPool* pool = et_create_memory_pool(4096, 32);
    TEST_ASSERT_NOT_NULL(pool, "메모리 풀 생성 실패");

    // 단편화 생성
    create_fragmented_pool(pool);

    // 단편화 분석
    ETFragmentationInfo frag_info;
    int result = et_analyze_fragmentation(pool, &frag_info);
    TEST_ASSERT_EQUAL(result, 0, "단편화 분석 실패");

    TEST_ASSERT_TRUE(frag_info.total_free_space > 0, "총 자유 공간 계산 오류");
    TEST_ASSERT_TRUE(frag_info.num_free_blocks > 0, "자유 블록 수 계산 오류");
    TEST_ASSERT_TRUE(frag_info.fragmentation_ratio >= 0.0f &&
                    frag_info.fragmentation_ratio <= 1.0f, "단편화 비율 범위 오류");

    printf("  단편화 분석 결과:\n");
    printf("    총 자유 공간: %zu bytes\n", frag_info.total_free_space);
    printf("    최대 자유 블록: %zu bytes\n", frag_info.largest_free_block);
    printf("    자유 블록 수: %zu\n", frag_info.num_free_blocks);
    printf("    단편화 비율: %.2f%%\n", frag_info.fragmentation_ratio * 100.0f);
    printf("    외부 단편화: %.2f%%\n", frag_info.external_fragmentation * 100.0f);

    et_destroy_memory_pool(pool);

    printf("✓ 메모리 단편화 분석 테스트 통과\n");
}

void test_memory_compaction(void) {
    printf("메모리 압축 테스트...\n");

    ETMemoryPool* pool = et_create_memory_pool(8192, 32);
    TEST_ASSERT_NOT_NULL(pool, "메모리 풀 생성 실패");

    // 단편화 생성
    create_fragmented_pool(pool);

    // 압축 전 단편화 분석
    ETFragmentationInfo before_frag;
    et_analyze_fragmentation(pool, &before_frag);

    // 메모리 압축 수행
    size_t compacted_bytes = et_compact_memory_pool(pool, false);
    printf("  압축된 바이트: %zu\n", compacted_bytes);

    // 압축 후 단편화 분석
    ETFragmentationInfo after_frag;
    et_analyze_fragmentation(pool, &after_frag);

    // 압축 효과 확인 (자유 블록 수가 줄어들어야 함)
    TEST_ASSERT_TRUE(after_frag.num_free_blocks <= before_frag.num_free_blocks,
                    "압축 후 자유 블록 수 증가 오류");

    printf("  압축 전 자유 블록 수: %zu\n", before_frag.num_free_blocks);
    printf("  압축 후 자유 블록 수: %zu\n", after_frag.num_free_blocks);

    et_destroy_memory_pool(pool);

    printf("✓ 메모리 압축 테스트 통과\n");
}

// =============================================================================
// 스마트 메모리 관리 테스트
// =============================================================================

void test_smart_memory_manager_creation(void) {
    printf("스마트 메모리 매니저 생성 테스트...\n");

    ETSmartMemoryManager* manager = et_create_smart_memory_manager(8192, 1024, 512, true);
    TEST_ASSERT_NOT_NULL(manager, "스마트 메모리 매니저 생성 실패");
    TEST_ASSERT_NOT_NULL(manager->primary_pool, "주 메모리 풀 생성 실패");
    TEST_ASSERT_NOT_NULL(manager->reuse_pool, "재사용 풀 생성 실패");
    TEST_ASSERT_NOT_NULL(manager->inplace_ctx, "인플레이스 컨텍스트 생성 실패");
    TEST_ASSERT_TRUE(manager->thread_safe, "스레드 안전성 설정 실패");

    et_destroy_smart_memory_manager(manager);

    printf("✓ 스마트 메모리 매니저 생성 테스트 통과\n");
}

void test_smart_memory_operations(void) {
    printf("스마트 메모리 연산 테스트...\n");

    ETSmartMemoryManager* manager = et_create_smart_memory_manager(4096, 512, 256, false);
    TEST_ASSERT_NOT_NULL(manager, "스마트 메모리 매니저 생성 실패");

    // 스마트 할당 테스트
    void* ptr1 = et_smart_alloc(manager, 128);
    TEST_ASSERT_NOT_NULL(ptr1, "스마트 할당 실패");

    void* ptr2 = et_smart_alloc(manager, 256);
    TEST_ASSERT_NOT_NULL(ptr2, "스마트 할당 실패");

    void* ptr3 = et_smart_alloc(manager, 128);
    TEST_ASSERT_NOT_NULL(ptr3, "스마트 할당 실패");

    // 통계 확인
    uint64_t total_allocs, bytes_saved, opt_count;
    et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);
    TEST_ASSERT_EQUAL(total_allocs, 3, "총 할당 수 불일치");

    // 스마트 해제 (재사용 풀로 반환)
    et_smart_free(manager, ptr1, 128);
    et_smart_free(manager, ptr2, 256);

    // 재할당 (재사용 풀에서 가져오기)
    void* ptr4 = et_smart_alloc(manager, 128);
    TEST_ASSERT_NOT_NULL(ptr4, "재할당 실패");

    // 재사용 효과 확인
    et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);
    TEST_ASSERT_TRUE(bytes_saved > 0, "메모리 절약 효과 없음");

    printf("  총 할당 수: %llu\n", (unsigned long long)total_allocs);
    printf("  절약된 바이트: %llu\n", (unsigned long long)bytes_saved);
    printf("  최적화 수행 횟수: %llu\n", (unsigned long long)opt_count);

    et_smart_free(manager, ptr3, 128);
    et_smart_free(manager, ptr4, 128);

    et_destroy_smart_memory_manager(manager);

    printf("✓ 스마트 메모리 연산 테스트 통과\n");
}

void test_smart_memory_optimization(void) {
    printf("스마트 메모리 최적화 테스트...\n");

    ETSmartMemoryManager* manager = et_create_smart_memory_manager(4096, 512, 256, false);
    TEST_ASSERT_NOT_NULL(manager, "스마트 메모리 매니저 생성 실패");

    // 메모리 사용 패턴 생성 (할당 및 해제 반복)
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        ptrs[i] = et_smart_alloc(manager, 64 + (i % 4) * 32);
        TEST_ASSERT_NOT_NULL(ptrs[i], "할당 실패");
    }

    // 일부 해제하여 단편화 생성
    for (int i = 0; i < 20; i += 2) {
        et_smart_free(manager, ptrs[i], 64 + (i % 4) * 32);
    }

    // 최적화 수행
    size_t optimizations = et_optimize_memory_usage(manager);
    printf("  수행된 최적화: %zu\n", optimizations);

    // 나머지 메모리 해제
    for (int i = 1; i < 20; i += 2) {
        et_smart_free(manager, ptrs[i], 64 + (i % 4) * 32);
    }

    // 최종 통계 확인
    uint64_t total_allocs, bytes_saved, opt_count;
    et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);

    printf("  최종 통계:\n");
    printf("    총 할당 수: %llu\n", (unsigned long long)total_allocs);
    printf("    절약된 바이트: %llu\n", (unsigned long long)bytes_saved);
    printf("    최적화 수행 횟수: %llu\n", (unsigned long long)opt_count);

    et_destroy_smart_memory_manager(manager);

    printf("✓ 스마트 메모리 최적화 테스트 통과\n");
}

// =============================================================================
// 유틸리티 함수 테스트
// =============================================================================

void test_utility_functions(void) {
    printf("유틸리티 함수 테스트...\n");

    // 2의 거듭제곱 올림 테스트
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(0), 1, "0 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(1), 1, "1 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(3), 4, "3 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(5), 8, "5 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(16), 16, "16 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(17), 32, "17 처리 오류");
    TEST_ASSERT_EQUAL(et_round_up_to_power_of_2(100), 128, "100 처리 오류");

    // 메모리 권장사항 생성 테스트
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);
    TEST_ASSERT_NOT_NULL(pool, "테스트 풀 생성 실패");

    char recommendations[1024];
    int rec_count = et_generate_memory_recommendations(pool, recommendations, sizeof(recommendations));
    TEST_ASSERT_TRUE(rec_count >= 0, "권장사항 생성 실패");

    printf("  생성된 권장사항 수: %d\n", rec_count);
    if (rec_count > 0) {
        printf("  권장사항:\n%s", recommendations);
    }

    et_destroy_memory_pool(pool);

    printf("✓ 유틸리티 함수 테스트 통과\n");
}

// =============================================================================
// 통합 테스트
// =============================================================================

void test_memory_optimization_integration(void) {
    printf("메모리 최적화 통합 테스트...\n");

    // 스마트 메모리 매니저로 전체 시나리오 테스트
    ETSmartMemoryManager* manager = et_create_smart_memory_manager(8192, 1024, 512, true);
    TEST_ASSERT_NOT_NULL(manager, "스마트 메모리 매니저 생성 실패");

    // 다양한 크기의 메모리 할당/해제 패턴 시뮬레이션
    void* ptrs[50];
    size_t sizes[50];

    // 1단계: 다양한 크기 할당
    for (int i = 0; i < 50; i++) {
        sizes[i] = 32 + (i % 8) * 64; // 32, 96, 160, 224, 288, 352, 416, 480 bytes
        ptrs[i] = et_smart_alloc(manager, sizes[i]);
        TEST_ASSERT_NOT_NULL(ptrs[i], "할당 실패");

        // 데이터 쓰기 테스트
        memset(ptrs[i], (uint8_t)(i % 256), sizes[i]);
    }

    // 2단계: 일부 해제 (단편화 생성)
    for (int i = 0; i < 50; i += 3) {
        et_smart_free(manager, ptrs[i], sizes[i]);
        ptrs[i] = NULL;
    }

    // 3단계: 재할당 (재사용 풀 활용)
    for (int i = 0; i < 50; i += 3) {
        ptrs[i] = et_smart_alloc(manager, sizes[i]);
        TEST_ASSERT_NOT_NULL(ptrs[i], "재할당 실패");
        memset(ptrs[i], (uint8_t)(i % 256), sizes[i]);
    }

    // 4단계: 최적화 수행
    size_t optimizations = et_optimize_memory_usage(manager);
    printf("  수행된 최적화: %zu\n", optimizations);

    // 5단계: 데이터 무결성 확인
    for (int i = 0; i < 50; i++) {
        if (ptrs[i] != NULL) {
            verify_memory_pattern(ptrs[i], sizes[i], (uint8_t)(i % 256));
        }
    }

    // 6단계: 모든 메모리 해제
    for (int i = 0; i < 50; i++) {
        if (ptrs[i] != NULL) {
            et_smart_free(manager, ptrs[i], sizes[i]);
        }
    }

    // 최종 통계 출력
    uint64_t total_allocs, bytes_saved, opt_count;
    et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);

    printf("  통합 테스트 결과:\n");
    printf("    총 할당 수: %llu\n", (unsigned long long)total_allocs);
    printf("    절약된 바이트: %llu\n", (unsigned long long)bytes_saved);
    printf("    최적화 수행 횟수: %llu\n", (unsigned long long)opt_count);

    // 재사용 풀 통계
    size_t total_requests, reuse_hits;
    float hit_rate;
    et_get_reuse_pool_stats(manager->reuse_pool, &total_requests, &reuse_hits, &hit_rate);
    printf("    재사용 성공률: %.2f%%\n", hit_rate * 100.0f);

    TEST_ASSERT_TRUE(bytes_saved > 0, "메모리 절약 효과 없음");
    TEST_ASSERT_TRUE(hit_rate > 0.0f, "재사용 효과 없음");

    et_destroy_smart_memory_manager(manager);

    printf("✓ 메모리 최적화 통합 테스트 통과\n");
}

// =============================================================================
// 테스트 헬퍼 함수 구현
// =============================================================================

static void create_fragmented_pool(ETMemoryPool* pool) {
    // 여러 크기의 블록을 할당하고 일부만 해제하여 단편화 생성
    void* ptrs[10];
    size_t sizes[] = {64, 128, 256, 128, 64, 512, 128, 256, 64, 128};

    // 할당
    for (int i = 0; i < 10; i++) {
        ptrs[i] = et_alloc_from_pool(pool, sizes[i]);
        if (ptrs[i] != NULL) {
            memset(ptrs[i], (uint8_t)i, sizes[i]);
        }
    }

    // 일부 해제 (홀수 인덱스만)
    for (int i = 1; i < 10; i += 2) {
        if (ptrs[i] != NULL) {
            et_free_to_pool(pool, ptrs[i]);
            ptrs[i] = NULL;
        }
    }
}

static void verify_memory_pattern(void* ptr, size_t size, uint8_t pattern) {
    uint8_t* data = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL(data[i], pattern, "메모리 패턴 불일치");
    }
}

// =============================================================================
// 메인 테스트 실행 함수
// =============================================================================

int main(void) {
    printf("=== LibEtude 메모리 최적화 전략 테스트 시작 ===\n\n");

    // 인플레이스 연산 테스트
    test_inplace_context_creation();
    test_inplace_operations();

    // 메모리 재사용 풀 테스트
    test_reuse_pool_creation();
    test_reuse_pool_operations();
    test_reuse_pool_cleanup();

    // 메모리 단편화 방지 테스트
    test_fragmentation_analysis();
    test_memory_compaction();

    // 스마트 메모리 관리 테스트
    test_smart_memory_manager_creation();
    test_smart_memory_operations();
    test_smart_memory_optimization();

    // 유틸리티 함수 테스트
    test_utility_functions();

    // 통합 테스트
    test_memory_optimization_integration();

    printf("\n=== 모든 메모리 최적화 테스트 완료 ===\n");
    printf("총 테스트: %d, 성공: %d, 실패: %d\n",
           test_get_total_count(), test_get_passed_count(), test_get_failed_count());

    return test_get_failed_count() == 0 ? 0 : 1;
}