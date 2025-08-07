/**
 * @file test_memory.c
 * @brief 메모리 관리 추상화 레이어 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/memory.h"
#include "libetude/platform/factory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// 테스트 결과 카운터
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("PASS: %s\n", message); \
            tests_passed++; \
        } else { \
            printf("FAIL: %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

/**
 * @brief 기본 메모리 할당 테스트
 */
void test_basic_memory_allocation(void) {
    printf("\n=== 기본 메모리 할당 테스트 ===\n");

    // 메모리 할당
    void* ptr1 = et_malloc(1024);
    TEST_ASSERT(ptr1 != NULL, "메모리 할당 성공");

    // 메모리 초기화
    memset(ptr1, 0xAA, 1024);
    TEST_ASSERT(((char*)ptr1)[0] == (char)0xAA, "메모리 쓰기 성공");

    // 메모리 해제
    et_free(ptr1);
    printf("메모리 해제 완료\n");

    // calloc 테스트
    void* ptr2 = et_calloc(256, 4);
    TEST_ASSERT(ptr2 != NULL, "calloc 할당 성공");
    TEST_ASSERT(((char*)ptr2)[0] == 0, "calloc 초기화 확인");

    et_free(ptr2);
}

/**
 * @brief 정렬된 메모리 할당 테스트
 */
void test_aligned_memory_allocation(void) {
    printf("\n=== 정렬된 메모리 할당 테스트 ===\n");

    // 16바이트 정렬 메모리 할당
    void* aligned_ptr = et_aligned_malloc(1024, 16);
    TEST_ASSERT(aligned_ptr != NULL, "정렬된 메모리 할당 성공");
    TEST_ASSERT(et_memory_is_aligned(aligned_ptr, 16), "16바이트 정렬 확인");

    et_aligned_free(aligned_ptr);

    // 64바이트 정렬 메모리 할당
    aligned_ptr = et_aligned_malloc(2048, 64);
    TEST_ASSERT(aligned_ptr != NULL, "64바이트 정렬 메모리 할당 성공");
    TEST_ASSERT(et_memory_is_aligned(aligned_ptr, 64), "64바이트 정렬 확인");

    et_aligned_free(aligned_ptr);
}

/**
 * @brief 메모리 유틸리티 함수 테스트
 */
void test_memory_utilities(void) {
    printf("\n=== 메모리 유틸리티 함수 테스트 ===\n");

    char buffer1[256];
    char buffer2[256];

    // 메모리 초기화 테스트
    ETResult result = et_memory_set_zero(buffer1, sizeof(buffer1));
    TEST_ASSERT(result == ET_SUCCESS, "메모리 초기화 성공");
    TEST_ASSERT(buffer1[0] == 0 && buffer1[255] == 0, "메모리 초기화 확인");

    // 메모리 복사 테스트
    memset(buffer1, 0xBB, sizeof(buffer1));
    result = et_memory_copy(buffer2, buffer1, sizeof(buffer1));
    TEST_ASSERT(result == ET_SUCCESS, "메모리 복사 성공");
    TEST_ASSERT(buffer2[0] == (char)0xBB, "메모리 복사 확인");

    // 메모리 비교 테스트
    int compare_result;
    result = et_memory_compare(buffer1, buffer2, sizeof(buffer1), &compare_result);
    TEST_ASSERT(result == ET_SUCCESS, "메모리 비교 성공");
    TEST_ASSERT(compare_result == 0, "메모리 내용 일치 확인");

    // 정렬 확인 테스트
    TEST_ASSERT(et_memory_is_aligned((void*)0x1000, 16), "정렬된 주소 확인");
    TEST_ASSERT(!et_memory_is_aligned((void*)0x1001, 16), "정렬되지 않은 주소 확인");
}

/**
 * @brief 공유 메모리 테스트
 */
void test_shared_memory(void) {
    printf("\n=== 공유 메모리 테스트 ===\n");

    ETMemoryInterface* interface = et_get_memory_interface();
    if (interface == NULL || interface->create_shared_memory == NULL) {
        printf("공유 메모리 기능이 지원되지 않습니다.\n");
        return;
    }

    ETSharedMemory* shm = NULL;
    ETResult result = interface->create_shared_memory("test_shm", 4096, &shm);

    if (result == ET_SUCCESS) {
        TEST_ASSERT(shm != NULL, "공유 메모리 생성 성공");

        // 메모리 매핑
        void* mapped_addr = interface->map_shared_memory(shm);
        TEST_ASSERT(mapped_addr != NULL, "공유 메모리 매핑 성공");

        // 데이터 쓰기
        strcpy((char*)mapped_addr, "Hello, Shared Memory!");

        // 매핑 해제
        result = interface->unmap_shared_memory(shm, mapped_addr);
        TEST_ASSERT(result == ET_SUCCESS, "공유 메모리 매핑 해제 성공");

        // 공유 메모리 해제
        interface->destroy_shared_memory(shm);
        printf("공유 메모리 해제 완료\n");
    } else {
        printf("공유 메모리 생성 실패 (플랫폼 제한일 수 있음)\n");
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("메모리 관리 추상화 레이어 테스트 시작\n");

    // 플랫폼 팩토리 초기화
    ETResult result = et_platform_factory_init();
    if (result != ET_SUCCESS) {
        printf("플랫폼 팩토리 초기화 실패\n");
        return 1;
    }

    // 메모리 인터페이스 초기화
    result = et_memory_init();
    if (result != ET_SUCCESS) {
        printf("메모리 인터페이스 초기화 실패\n");
        return 1;
    }

    // 테스트 실행
    test_basic_memory_allocation();
    test_aligned_memory_allocation();
    test_memory_utilities();
    test_shared_memory();

    // 정리
    et_memory_cleanup();
    et_platform_factory_cleanup();

    // 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("통과: %d개\n", tests_passed);
    printf("실패: %d개\n", tests_failed);
    printf("총 테스트: %d개\n", tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}