/**
 * @file test_streaming.c
 * @brief 스트리밍 통합 테스트 (임시)
 */

#include "tests/framework/test_framework.h"

void test_streaming_placeholder(void) {
    TEST_ASSERT_TRUE(1); // 임시 테스트
}

int main(void) {
    RUN_TEST(test_streaming_placeholder);
    return test_framework_get_failed_count() == 0 ? 0 : 1;
}