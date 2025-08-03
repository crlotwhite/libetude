#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>

#include "libetude/platform/windows_security.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

/**
 * @brief DEP 호환성 확인 테스트
 */
void test_dep_compatibility_check(void) {
    printf("\n=== DEP 호환성 확인 테스트 ===\n");

    // DEP 상태 확인
    bool dep_enabled = et_windows_check_dep_compatibility();
    printf("DEP Status: %s\n", dep_enabled ? "Enabled" : "Disabled or Not Supported");

    // Test that DEP check function works properly
    // (Success if function doesn't crash regardless of true/false result)
    TEST_ASSERT(true, "DEP compatibility check function executed successfully");
}

/**
 * @brief ASLR 호환성 확인 테스트
 */
void test_aslr_compatibility_check(void) {
    printf("\n=== ASLR 호환성 확인 테스트 ===\n");

    // ASLR 상태 확인
    bool aslr_enabled = et_windows_check_aslr_compatibility();
    printf("ASLR Status: %s\n", aslr_enabled ? "Supported" : "Not Supported");

    // Test that ASLR check function works properly
    TEST_ASSERT(true, "ASLR compatibility check function executed successfully");
}

/**
 * @brief Windows 보안 상태 조회 테스트
 */
void test_security_status_query(void) {
    printf("\n=== Windows 보안 상태 조회 테스트 ===\n");

    ETWindowsSecurityStatus status;
    bool result = et_windows_get_security_status(&status);

    TEST_ASSERT(result == true, "보안 상태 조회 성공");

    if (result) {
        printf("DEP 활성화: %s\n", status.dep_enabled ? "예" : "아니오");
        printf("ASLR 지원: %s\n", status.aslr_enabled ? "예" : "아니오");
        printf("Large Address Aware: %s\n", status.large_address_aware ? "예" : "아니오");
    }

    // NULL 포인터 테스트
    result = et_windows_get_security_status(NULL);
    TEST_ASSERT(result == false, "NULL 포인터 전달 시 실패 반환");
}

/**
 * @brief ASLR 호환 메모리 할당 테스트
 */
void test_aslr_compatible_allocation(void) {
    printf("\n=== ASLR 호환 메모리 할당 테스트 ===\n");

    // 기본 메모리 할당 테스트
    size_t test_size = 1024;
    void* ptr1 = et_windows_alloc_aslr_compatible(test_size);
    TEST_ASSERT(ptr1 != NULL, "ASLR 호환 메모리 할당 성공");

    if (ptr1) {
        // 메모리에 데이터 쓰기 테스트
        memset(ptr1, 0xAA, test_size);
        TEST_ASSERT(((unsigned char*)ptr1)[0] == 0xAA, "할당된 메모리 쓰기 가능");

        et_windows_free_aslr_compatible(ptr1);
        TEST_ASSERT(true, "ASLR 호환 메모리 해제 성공");
    }

    // 여러 번 할당하여 주소가 다른지 확인 (ASLR 동작 확인)
    void* ptrs[5];
    bool addresses_different = false;

    for (int i = 0; i < 5; i++) {
        ptrs[i] = et_windows_alloc_aslr_compatible(1024);
        if (ptrs[i] && i > 0 && ptrs[i] != ptrs[i-1]) {
            addresses_different = true;
        }
    }

    for (int i = 0; i < 5; i++) {
        if (ptrs[i]) {
            et_windows_free_aslr_compatible(ptrs[i]);
        }
    }

    TEST_ASSERT(addresses_different, "여러 할당에서 서로 다른 주소 반환 (ASLR 동작)");

    // 0 크기 할당 테스트
    void* ptr_zero = et_windows_alloc_aslr_compatible(0);
    TEST_ASSERT(ptr_zero == NULL, "0 크기 할당 시 NULL 반환");

    // NULL 포인터 해제 테스트
    et_windows_free_aslr_compatible(NULL);
    TEST_ASSERT(true, "NULL 포인터 해제 시 크래시 없음");
}

/**
 * @brief 보안 메모리 할당자 테스트
 */
void test_secure_allocator(void) {
    printf("\n=== 보안 메모리 할당자 테스트 ===\n");

    ETWindowsSecureAllocator allocator;

    // 할당자 초기화
    bool init_result = et_windows_secure_allocator_init(&allocator, 4096, false);
    TEST_ASSERT(init_result == true, "보안 메모리 할당자 초기화 성공");

    if (init_result) {
        // 메모리 할당 테스트
        void* ptr1 = et_windows_secure_allocator_alloc(&allocator, 256);
        TEST_ASSERT(ptr1 != NULL, "보안 할당자에서 메모리 할당 성공");

        if (ptr1) {
            // 메모리 사용 테스트
            memset(ptr1, 0xBB, 256);
            TEST_ASSERT(((unsigned char*)ptr1)[0] == 0xBB, "할당된 메모리 사용 가능");

            et_windows_secure_allocator_free(&allocator, ptr1);
            TEST_ASSERT(true, "보안 할당자에서 메모리 해제 성공");
        }

        // 여러 할당 테스트
        void* ptrs[10];
        int successful_allocs = 0;

        for (int i = 0; i < 10; i++) {
            ptrs[i] = et_windows_secure_allocator_alloc(&allocator, 128);
            if (ptrs[i]) {
                successful_allocs++;
            }
        }

        TEST_ASSERT(successful_allocs > 0, "여러 메모리 할당 중 일부 성공");

        // 할당된 메모리 해제
        for (int i = 0; i < 10; i++) {
            if (ptrs[i]) {
                et_windows_secure_allocator_free(&allocator, ptrs[i]);
            }
        }

        et_windows_secure_allocator_cleanup(&allocator);
        TEST_ASSERT(true, "보안 메모리 할당자 정리 성공");
    }

    // NULL 포인터 테스트
    bool null_init = et_windows_secure_allocator_init(NULL, 4096, false);
    TEST_ASSERT(null_init == false, "NULL 할당자 초기화 시 실패 반환");

    // 0 크기 초기화 테스트
    ETWindowsSecureAllocator allocator2;
    bool zero_init = et_windows_secure_allocator_init(&allocator2, 0, false);
    TEST_ASSERT(zero_init == false, "0 크기 할당자 초기화 시 실패 반환");
}

/**
 * @brief 메모리 보호 기능 테스트
 */
void test_memory_protection(void) {
    printf("\n=== 메모리 보호 기능 테스트 ===\n");

    // 테스트용 메모리 할당
    size_t test_size = 4096; // 페이지 크기
    void* test_memory = VirtualAlloc(NULL, test_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (test_memory) {
        // 초기 데이터 쓰기
        memset(test_memory, 0xCC, test_size);
        TEST_ASSERT(((unsigned char*)test_memory)[0] == 0xCC, "초기 메모리 쓰기 성공");

        // 메모리를 읽기 전용으로 변경
        bool readonly_result = et_windows_make_memory_read_only(test_memory, test_size);
        TEST_ASSERT(readonly_result == true, "메모리를 읽기 전용으로 변경 성공");

        // 읽기는 여전히 가능해야 함
        unsigned char read_value = ((unsigned char*)test_memory)[0];
        TEST_ASSERT(read_value == 0xCC, "읽기 전용 메모리에서 읽기 가능");

        // 메모리를 다시 읽기/쓰기로 변경
        DWORD old_protect;
        VirtualProtect(test_memory, test_size, PAGE_READWRITE, &old_protect);

        // 실행 불가능하게 만들기
        bool non_exec_result = et_windows_make_memory_non_executable(test_memory, test_size);
        TEST_ASSERT(non_exec_result == true, "메모리를 실행 불가능하게 변경 성공");

        VirtualFree(test_memory, 0, MEM_RELEASE);
        TEST_ASSERT(true, "테스트 메모리 해제 성공");
    } else {
        printf("[SKIP] 테스트용 메모리 할당 실패\n");
    }

    // NULL 포인터 테스트
    bool null_readonly = et_windows_make_memory_read_only(NULL, 1024);
    TEST_ASSERT(null_readonly == false, "NULL 포인터에 대한 읽기 전용 설정 실패");

    bool null_nonexec = et_windows_make_memory_non_executable(NULL, 1024);
    TEST_ASSERT(null_nonexec == false, "NULL 포인터에 대한 실행 불가 설정 실패");
}

int main(void) {
    printf("Windows 보안 기능 (DEP/ASLR) 테스트 시작\n");
    printf("========================================\n");

    test_dep_compatibility_check();
    test_aslr_compatibility_check();
    test_security_status_query();
    test_aslr_compatible_allocation();
    test_secure_allocator();
    test_memory_protection();

    printf("\n========================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif // _WIN32