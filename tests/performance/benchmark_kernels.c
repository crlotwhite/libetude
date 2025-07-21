/**
 * @file benchmark_kernels.c
 * @brief 커널 벤치마크 (임시)
 */

#include "tests/framework/test_framework.h"

void benchmark_kernels_placeholder(void) {
    TEST_ASSERT_TRUE(1); // 임시 벤치마크
}

int main(void) {
    RUN_TEST(benchmark_kernels_placeholder);
    return test_framework_get_failed_count() == 0 ? 0 : 1;
}