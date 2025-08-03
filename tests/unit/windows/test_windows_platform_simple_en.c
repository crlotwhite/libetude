/* LibEtude Windows Platform Simple Test */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows.h"
#include "libetude/error.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("LibEtude Windows Platform Simple Test Started\n");
    printf("==========================================\n");

    /* Default Configuration Creation Test */
    printf("\n=== Default Configuration Creation Test ===\n");

    ETWindowsPlatformConfig config = et_windows_create_default_config();

    printf("WASAPI Default Enabled: %s\n", config.audio.prefer_wasapi ? "Yes" : "No");
    printf("Default Buffer Size: %lu ms\n", config.audio.buffer_size_ms);
    printf("Large Page Default Enabled: %s\n", config.performance.enable_large_pages ? "Yes" : "No");
    printf("DEP Default Enabled: %s\n", config.security.enforce_dep ? "Yes" : "No");

    assert(config.audio.prefer_wasapi == true);
    assert(config.audio.buffer_size_ms == 20);
    assert(config.performance.enable_large_pages == true);
    assert(config.security.enforce_dep == true);

    printf("Default Configuration Creation Test Passed!\n");

    /* CPU Feature Detection Test */
    printf("\n=== CPU Feature Detection Test ===\n");

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();

    printf("Detected CPU Features:\n");
    printf("- SSE4.1: %s\n", features.has_sse41 ? "Supported" : "Not Supported");
    printf("- AVX: %s\n", features.has_avx ? "Supported" : "Not Supported");
    printf("- AVX2: %s\n", features.has_avx2 ? "Supported" : "Not Supported");
    printf("- AVX-512: %s\n", features.has_avx512 ? "Supported" : "Not Supported");

    printf("CPU Feature Detection Test Passed!\n");

    /* Security Features Test */
    printf("\n=== Security Features Test ===\n");

    bool dep_compatible = et_windows_check_dep_compatibility();
    printf("DEP Compatibility: %s\n", dep_compatible ? "Compatible" : "Not Compatible");

    bool uac_elevated = et_windows_check_uac_permissions();
    printf("UAC Permissions: %s\n", uac_elevated ? "Administrator" : "Regular User");

    printf("Security Features Test Passed!\n");

    /* Memory Allocation Test */
    printf("\n=== Memory Allocation Test ===\n");

    size_t test_size = 1024;  /* 1KB */
    void* aslr_ptr = et_windows_alloc_aslr_compatible(test_size);

    if (aslr_ptr != NULL) {
        printf("ASLR Compatible Memory Allocation Successful\n");

        /* Memory Read/Write Test */
        char* test_data = (char*)aslr_ptr;
        test_data[0] = 'A';
        test_data[test_size - 1] = 'Z';

        assert(test_data[0] == 'A');
        assert(test_data[test_size - 1] == 'Z');
        printf("Memory Read/Write Test Passed!\n");

        /* Memory Cleanup */
        if (VirtualFree(aslr_ptr, 0, MEM_RELEASE) == 0) {
            free(aslr_ptr);
        }
    } else {
        printf("ASLR Compatible Memory Allocation Failed\n");
        return 1;
    }

    printf("Memory Allocation Test Passed!\n");

    printf("\n==========================================\n");
    printf("All Tests Passed! Success\n");

    return 0;
}