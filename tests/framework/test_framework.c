/**
 * @file test_framework.c
 * @brief 간단한 테스트 프레임워크 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 전역 테스트 상태
static TestSuite* g_current_suite = NULL;
static int g_total_tests = 0;
int g_passed_tests = 0;
int g_failed_tests = 0;

/**
 * @brief 테스트 스위트를 생성합니다
 */
TestSuite* test_suite_create(const char* name) {
    TestSuite* suite = (TestSuite*)malloc(sizeof(TestSuite));
    if (!suite) {
        return NULL;
    }

    strncpy(suite->name, name, sizeof(suite->name) - 1);
    suite->name[sizeof(suite->name) - 1] = '\0';
    suite->test_cases = NULL;
    suite->num_test_cases = 0;
    suite->capacity = 0;
    suite->passed_count = 0;
    suite->failed_count = 0;

    return suite;
}

/**
 * @brief 테스트 스위트를 해제합니다
 */
void test_suite_destroy(TestSuite* suite) {
    if (!suite) {
        return;
    }

    if (suite->test_cases) {
        free(suite->test_cases);
    }
    free(suite);
}

/**
 * @brief 테스트 케이스를 추가합니다
 */
int test_suite_add_case(TestSuite* suite, const char* name,
                       void (*setup)(), void (*teardown)(), void (*test_func)()) {
    if (!suite || !name || !test_func) {
        return -1;
    }

    // 용량 확장이 필요한 경우
    if (suite->num_test_cases >= suite->capacity) {
        int new_capacity = suite->capacity == 0 ? 16 : suite->capacity * 2;
        TestCase* new_cases = (TestCase*)realloc(suite->test_cases,
                                                new_capacity * sizeof(TestCase));
        if (!new_cases) {
            return -1;
        }
        suite->test_cases = new_cases;
        suite->capacity = new_capacity;
    }

    // 새 테스트 케이스 추가
    TestCase* test_case = &suite->test_cases[suite->num_test_cases];
    strncpy(test_case->name, name, sizeof(test_case->name) - 1);
    test_case->name[sizeof(test_case->name) - 1] = '\0';
    test_case->setup = setup;
    test_case->teardown = teardown;
    test_case->test_func = test_func;
    test_case->passed = false;
    test_case->error_message[0] = '\0';

    suite->num_test_cases++;
    return 0;
}

/**
 * @brief 테스트 스위트를 실행합니다
 */
void test_suite_run(TestSuite* suite) {
    if (!suite) {
        return;
    }

    g_current_suite = suite;

    printf("=== Running Test Suite: %s ===\n", suite->name);

    clock_t start_time = clock();

    for (int i = 0; i < suite->num_test_cases; i++) {
        TestCase* test_case = &suite->test_cases[i];

        printf("Running test: %s ... ", test_case->name);
        fflush(stdout);

        // Setup 실행
        if (test_case->setup) {
            test_case->setup();
        }

        // 테스트 실행
        test_case->test_func();

        // Teardown 실행
        if (test_case->teardown) {
            test_case->teardown();
        }

        // 결과 출력
        if (test_case->passed) {
            printf("PASS\n");
            suite->passed_count++;
            g_passed_tests++;
        } else {
            printf("FAIL\n");
            if (test_case->error_message[0] != '\0') {
                printf("  Error: %s\n", test_case->error_message);
            }
            suite->failed_count++;
            g_failed_tests++;
        }

        g_total_tests++;
    }

    clock_t end_time = clock();
    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf("=== Test Suite Results ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n",
           suite->num_test_cases, suite->passed_count, suite->failed_count);
    printf("Elapsed time: %.3f seconds\n", elapsed);
    printf("===========================\n\n");
}

/**
 * @brief 모든 테스트 결과를 출력합니다
 */
void test_print_summary() {
    printf("=== Overall Test Summary ===\n");
    printf("Total tests: %d\n", g_total_tests);
    printf("Passed: %d\n", g_passed_tests);
    printf("Failed: %d\n", g_failed_tests);
    printf("Success rate: %.1f%%\n",
           g_total_tests > 0 ? (100.0 * g_passed_tests / g_total_tests) : 0.0);
    printf("=============================\n");
}

/**
 * @brief 테스트 실패를 기록합니다
 */
void test_fail(const char* message) {
    if (g_current_suite && g_current_suite->num_test_cases > 0) {
        TestCase* current_test = &g_current_suite->test_cases[g_current_suite->num_test_cases - 1];
        current_test->passed = false;
        if (message) {
            strncpy(current_test->error_message, message, sizeof(current_test->error_message) - 1);
            current_test->error_message[sizeof(current_test->error_message) - 1] = '\0';
        }
    }
}

/**
 * @brief 테스트 성공을 기록합니다
 */
void test_pass() {
    if (g_current_suite && g_current_suite->num_test_cases > 0) {
        TestCase* current_test = &g_current_suite->test_cases[g_current_suite->num_test_cases - 1];
        current_test->passed = true;
    }
}

/**
 * @brief 전체 테스트 실행 결과를 반환합니다
 */
int test_get_exit_code() {
    return g_failed_tests > 0 ? 1 : 0;
}