#include <stdio.h>
#include <stdlib.h>
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_simple(void) {
    TEST_ASSERT_EQUAL(1, 1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_simple);
    return UNITY_END();
}