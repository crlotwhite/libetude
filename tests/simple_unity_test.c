/**
 * @file test_unity_simple.c
 * @brief Unity 테스트 프레임워크 동작 확인용 간단한 테스트
 */

#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {
    // 테스트 전 초기화
    printf("Setting up test...\n");
}

void tearDown(void) {
    // 테스트 후 정리
    printf("Tearing down test...\n");
}

void test_basic_assertions(void) {
    printf("Running basic assertions test...\n");

    // 기본 assertion 테스트
    TEST_ASSERT_EQUAL(42, 42);
    TEST_ASSERT_TRUE(1 == 1);
    TEST_ASSERT_FALSE(1 == 2);
    TEST_ASSERT_NOT_EQUAL(1, 2);

    printf("Basic assertions test passed!\n");
}

void test_float_assertions(void) {
    printf("Running float assertions test...\n");

    // 부동소수점 assertion 테스트
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, 3.141f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, 1.0f);

    printf("Float assertions test passed!\n");
}

void test_string_assertions(void) {
    printf("Running string assertions test...\n");

    // 문자열 assertion 테스트
    TEST_ASSERT_EQUAL_STRING("hello", "hello");
    // Unity에서는 NOT_EQUAL_STRING이 없으므로 다른 방법 사용
    TEST_ASSERT_FALSE(strcmp("hello", "world") == 0);

    printf("String assertions test passed!\n");
}

void test_pointer_assertions(void) {
    printf("Running pointer assertions test...\n");

    int value = 42;
    int* ptr = &value;

    // 포인터 assertion 테스트
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_NULL(NULL);
    TEST_ASSERT_EQUAL_PTR(ptr, &value);

    printf("Pointer assertions test passed!\n");
}

void test_array_assertions(void) {
    printf("Running array assertions test...\n");

    int array1[] = {1, 2, 3, 4, 5};
    int array2[] = {1, 2, 3, 4, 5};
    int array3[] = {1, 2, 3, 4, 6};

    // 배열 assertion 테스트
    TEST_ASSERT_EQUAL_INT_ARRAY(array1, array2, 5);
    // TEST_ASSERT_EQUAL_INT_ARRAY(array1, array3, 5); // 이것은 실패해야 함

    printf("Array assertions test passed!\n");
}

void test_math_operations(void) {
    printf("Running math operations test...\n");

    // 간단한 수학 연산 테스트
    int a = 10;
    int b = 5;

    TEST_ASSERT_EQUAL(15, a + b);
    TEST_ASSERT_EQUAL(5, a - b);
    TEST_ASSERT_EQUAL(50, a * b);
    TEST_ASSERT_EQUAL(2, a / b);

    printf("Math operations test passed!\n");
}

void test_conditional_logic(void) {
    printf("Running conditional logic test...\n");

    int x = 10;

    // 조건부 로직 테스트
    TEST_ASSERT_TRUE(x > 5);
    TEST_ASSERT_FALSE(x < 5);
    TEST_ASSERT_TRUE(x >= 10);
    TEST_ASSERT_TRUE(x <= 10);

    printf("Conditional logic test passed!\n");
}

// Unity 테스트 러너
int main(void) {
    printf("=== Unity 테스트 프레임워크 동작 확인 ===\n\n");

    UNITY_BEGIN();

    RUN_TEST(test_basic_assertions);
    RUN_TEST(test_float_assertions);
    RUN_TEST(test_string_assertions);
    RUN_TEST(test_pointer_assertions);
    RUN_TEST(test_array_assertions);
    RUN_TEST(test_math_operations);
    RUN_TEST(test_conditional_logic);

    printf("\n=== 모든 테스트 완료 ===\n");

    return UNITY_END();
}