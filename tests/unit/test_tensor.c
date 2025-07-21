/**
 * @file test_tensor.c
 * @brief 텐서 테스트 (임시)
 */

#include "tests/framework/test_framework.h"

void test_tensor_placeholder(void) {
    TEST_ASSERT_TRUE(1); // 임시 테스트
}

int main(void) {
    RUN_TEST(test_tensor_placeholder);
    return test_framework_get_failed_count() == 0 ? 0 : 1;
}