/**
 * @file test_pipeline.c
 * @brief 파이프라인 통합 테스트 (임시)
 */

#include "tests/framework/test_framework.h"

void test_pipeline_placeholder(void) {
    TEST_ASSERT_TRUE(1); // 임시 테스트
}

int main(void) {
    RUN_TEST(test_pipeline_placeholder);
    return test_framework_get_failed_count() == 0 ? 0 : 1;
}