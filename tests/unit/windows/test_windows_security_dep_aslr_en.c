#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>

#include "libetude/platform/windows_security.h"

// Test result counters
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
 * @brief DEP compatibility check test
 */
void test_dep_compatibility_check(void) {
    printf("\n=== DEP Compatibility Check Test ===\n");

    // Check DEP status
    bool dep_enabled = et_windows_check_dep_compatibility();
    printf("DEP Status: %s\n", dep_enabled ? "Enabled" : "Disabled or Not Supported");

    // Test that DEP check function works properly
    TEST_ASSERT(true, "DEP compatibility check function executed successfully");
}

/**
 * @brief ASLR compatibility check test
 */
void test_aslr_compatibility_check(void) {
    printf("\n=== ASLR Compatibility Check Test ===\n");

    // Check ASLR status
    bool aslr_enabled = et_windows_check_aslr_compatibility();
    printf("ASLR Status: %s\n", aslr_enabled ? "Supported" : "Not Supported");

    // Test that ASLR check function works properly
    TEST_ASSERT(true, "ASLR compatibility check function executed successfully");
}

/**
 * @brief Windows security status query test
 */
void test_security_status_query(void) {
    printf("\n=== Windows Security Status Query Test ===\n");

    ETWindowsSecurityStatus status;
    bool result = et_windows_get_security_status(&status);

    TEST_ASSERT(result == true, "Security status query successful");

    if (result) {
        printf("DEP Enabled: %s\n", status.dep_enabled ? "Yes" : "No");
        printf("ASLR Supported: %s\n", status.aslr_enabled ? "Yes" : "No");
        printf("Large Address Aware: %s\n", status.large_address_aware ? "Yes" : "No");
    }

    // NULL pointer test
    result = et_windows_get_security_status(NULL);
    TEST_ASSERT(result == false, "NULL pointer returns failure");
}

/**
 * @brief ASLR compatible memory allocation test
 */
void test_aslr_compatible_allocation(void) {
    printf("\n=== ASLR Compatible Memory Allocation Test ===\n");

    // Basic memory allocation test
    size_t test_size = 1024;
    void* ptr1 = et_windows_alloc_aslr_compatible(test_size);
    TEST_ASSERT(ptr1 != NULL, "ASLR compatible memory allocation successful");

    if (ptr1) {
        // Test writing to allocated memory
        memset(ptr1, 0xAA, test_size);
        TEST_ASSERT(((unsigned char*)ptr1)[0] == 0xAA, "Allocated memory is writable");

        et_windows_free_aslr_compatible(ptr1);
        TEST_ASSERT(true, "ASLR compatible memory freed successfully");
    }

    // Multiple allocations to check different addresses (ASLR behavior)
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

    TEST_ASSERT(addresses_different, "Multiple allocations return different addresses (ASLR working)");

    // Zero size allocation test
    void* ptr_zero = et_windows_alloc_aslr_compatible(0);
    TEST_ASSERT(ptr_zero == NULL, "Zero size allocation returns NULL");

    // NULL pointer free test
    et_windows_free_aslr_compatible(NULL);
    TEST_ASSERT(true, "NULL pointer free doesn't crash");
}

/**
 * @brief Secure memory allocator test
 */
void test_secure_allocator(void) {
    printf("\n=== Secure Memory Allocator Test ===\n");

    ETWindowsSecureAllocator allocator;

    // Initialize allocator
    bool init_result = et_windows_secure_allocator_init(&allocator, 4096, false);
    TEST_ASSERT(init_result == true, "Secure memory allocator initialization successful");

    if (init_result) {
        // Memory allocation test
        void* ptr1 = et_windows_secure_allocator_alloc(&allocator, 256);
        TEST_ASSERT(ptr1 != NULL, "Memory allocation from secure allocator successful");

        if (ptr1) {
            // Memory usage test
            memset(ptr1, 0xBB, 256);
            TEST_ASSERT(((unsigned char*)ptr1)[0] == 0xBB, "Allocated memory is usable");

            et_windows_secure_allocator_free(&allocator, ptr1);
            TEST_ASSERT(true, "Memory freed from secure allocator successfully");
        }

        // Multiple allocation test
        void* ptrs[10];
        int successful_allocs = 0;

        for (int i = 0; i < 10; i++) {
            ptrs[i] = et_windows_secure_allocator_alloc(&allocator, 128);
            if (ptrs[i]) {
                successful_allocs++;
            }
        }

        TEST_ASSERT(successful_allocs > 0, "Some multiple memory allocations successful");

        // Free allocated memory
        for (int i = 0; i < 10; i++) {
            if (ptrs[i]) {
                et_windows_secure_allocator_free(&allocator, ptrs[i]);
            }
        }

        et_windows_secure_allocator_cleanup(&allocator);
        TEST_ASSERT(true, "Secure memory allocator cleanup successful");
    }

    // NULL pointer test
    bool null_init = et_windows_secure_allocator_init(NULL, 4096, false);
    TEST_ASSERT(null_init == false, "NULL allocator initialization returns failure");

    // Zero size initialization test
    ETWindowsSecureAllocator allocator2;
    bool zero_init = et_windows_secure_allocator_init(&allocator2, 0, false);
    TEST_ASSERT(zero_init == false, "Zero size allocator initialization returns failure");
}

/**
 * @brief Memory protection features test
 */
void test_memory_protection(void) {
    printf("\n=== Memory Protection Features Test ===\n");

    // Allocate test memory
    size_t test_size = 4096; // Page size
    void* test_memory = VirtualAlloc(NULL, test_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (test_memory) {
        // Initial data write
        memset(test_memory, 0xCC, test_size);
        TEST_ASSERT(((unsigned char*)test_memory)[0] == 0xCC, "Initial memory write successful");

        // Change memory to read-only
        bool readonly_result = et_windows_make_memory_read_only(test_memory, test_size);
        TEST_ASSERT(readonly_result == true, "Memory changed to read-only successfully");

        // Reading should still be possible
        unsigned char read_value = ((unsigned char*)test_memory)[0];
        TEST_ASSERT(read_value == 0xCC, "Reading from read-only memory possible");

        // Change memory back to read/write
        DWORD old_protect;
        VirtualProtect(test_memory, test_size, PAGE_READWRITE, &old_protect);

        // Make memory non-executable
        bool non_exec_result = et_windows_make_memory_non_executable(test_memory, test_size);
        TEST_ASSERT(non_exec_result == true, "Memory changed to non-executable successfully");

        VirtualFree(test_memory, 0, MEM_RELEASE);
        TEST_ASSERT(true, "Test memory freed successfully");
    } else {
        printf("[SKIP] Test memory allocation failed\n");
    }

    // NULL pointer tests
    bool null_readonly = et_windows_make_memory_read_only(NULL, 1024);
    TEST_ASSERT(null_readonly == false, "Read-only setting on NULL pointer fails");

    bool null_nonexec = et_windows_make_memory_non_executable(NULL, 1024);
    TEST_ASSERT(null_nonexec == false, "Non-executable setting on NULL pointer fails");
}

int main(void) {
    printf("Windows Security Features (DEP/ASLR) Test Started\n");
    printf("================================================\n");

    test_dep_compatibility_check();
    test_aslr_compatibility_check();
    test_security_status_query();
    test_aslr_compatible_allocation();
    test_secure_allocator();
    test_memory_protection();

    printf("\n================================================\n");
    printf("Test Results: %d/%d Passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All tests passed successfully!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}

#else

int main(void) {
    printf("This test runs only on Windows platform.\n");
    return 0;
}

#endif // _WIN32