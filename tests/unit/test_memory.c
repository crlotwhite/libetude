/**
 * @file test_memory.c
 * @brief 메모리 테스트 (임시)
 */

#include "tests/framework/test_framework.h"

void test_memory_placeholder(void) {
    TEST_ASSERT_TRUE(1); // 임시 테스트
}

int main(void) {
    RUN_TEST(test_memory_placeholder);
    return test_framework_get_failed_count() == 0 ? 0 : 1;
}