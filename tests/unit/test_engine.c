/**
 * @file test_engine.c
 * @brief LibEtude 엔진 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/engine.h"
#include "libetude/config.h"

static ETEngine* test_engine = NULL;

void setUp(void) {
    test_engine = NULL;
}

void tearDown(void) {
    if (test_engine) {
        et_destroy_engine(test_engine);
        test_engine = NULL;
    }
}

void test_engine_initialization(void) {
    test_engine = et_create_engine();
    TEST_ASSERT_NOT_NULL(test_engine);
}

void test_basic_engine_operations(void) {
    test_engine = et_create_engine();
    TEST_ASSERT_NOT_NULL(test_engine);

    // 기본 엔진 작업 테스트
    TEST_ASSERT_EQUAL(ET_ENGINE_STATUS_READY, et_engine_get_status(test_engine));
}

void test_engine_configuration(void) {
    test_engine = et_create_engine();
    TEST_ASSERT_NOT_NULL(test_engine);

    // 엔진 설정 테스트 (임시)
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_engine_initialization);
    RUN_TEST(test_basic_engine_operations);
    RUN_TEST(test_engine_configuration);

    return UNITY_END();
}