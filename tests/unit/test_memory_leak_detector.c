#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

// 메모리 누수 감지기 함수 선언
int memory_leak_detector_init();
int memory_leak_detector_cleanup();
void* memory_tracked_malloc(size_t size, const char* file, int line, const char* function);
void memory_tracked_free(void* ptr, const char* file, int line, const char* function);
int memory_check_leaks();
int memory_get_leak_stats(size_t* total_allocs, size_t* total_bytes, size_t* peak_bytes);
float memory_analyze_fragmentation();
int memory_compact();
int memory_set_tracking_enabled(bool enabled);
void* memory_create_pool(size_t size_mb, size_t alignment);
void* memory_pool_alloc(size_t size);
int memory_pool_free(void* ptr);
int memory_pool_get_stats(size_t* total_mb, size_t* used_mb, size_t* free_mb, float* fragmentation);

// 에러 코드 정의
#define LIBETUDE_SUCCESS 0
#define LIBETUDE_ERROR_RUNTIME -1
#define LIBETUDE_ERROR_INVALID_ARGUMENT -2

// 메모리 누수 감지기 초기화 테스트
void test_memory_leak_detector_init() {
    printf("메모리 누수 감지기 초기화 테스트... ");

    int result = memory_leak_detector_init();
    assert(result == LIBETUDE_SUCCESS);

    result = memory_leak_detector_cleanup();
    assert(result == LIBETUDE_SUCCESS);

    printf("통과\n");
}

// 메모리 누수 감지 테스트
void test_memory_leak_detection() {
    printf("메모리 누수 감지 테스트... ");

    memory_leak_detector_init();
    memory_set_tracking_enabled(true);

    // 의도적으로 메모리 누수 생성
    void* ptr1 = memory_tracked_malloc(1024, __FILE__, __LINE__, __FUNCTION__);
    void* ptr2 = memory_tracked_malloc(2048, __FILE__, __LINE__, __FUNCTION__);
    void* ptr3 = memory_tracked_malloc(512, __FILE__, __LINE__, __FUNCTION__);

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    // 일부만 해제 (누수 생성)
    memory_tracked_free(ptr1, __FILE__, __LINE__, __FUNCTION__);

    // 누수 통계 확인
    size_t total_allocs, total_bytes, peak_bytes;
    int result = memory_get_leak_stats(&total_allocs, &total_bytes, &peak_bytes);
    assert(result == LIBETUDE_SUCCESS);
    assert(total_allocs == 2); // 2개의 누수
    assert(total_bytes == 2048 + 512); // 2560 바이트 누수
    assert(peak_bytes >= total_bytes);

    printf("누수 통계: %zu개, %zu bytes ", total_allocs, total_bytes);

    // 누수 검사
    result = memory_check_leaks();
    assert(result == LIBETUDE_ERROR_RUNTIME); // 누수가 있으므로 에러 반환

    // 남은 메모리 해제
    memory_tracked_free(ptr2, __FILE__, __LINE__, __FUNCTION__);
    memory_tracked_free(ptr3, __FILE__, __LINE__, __FUNCTION__);

    memory_leak_detector_cleanup();

    printf("통과\n");
}

// 메모리 추적 비활성화 테스트
void test_memory_tracking_disable() {
    printf("메모리 추적 비활성화 테스트... ");

    memory_leak_detector_init();
    memory_set_tracking_enabled(false);

    // 추적이 비활성화된 상태에서 할당
    void* ptr = memory_tracked_malloc(1024, __FILE__, __LINE__, __FUNCTION__);
    assert(ptr != NULL);

    // 통계 확인 (추적되지 않아야 함)
    size_t total_allocs, total_bytes, peak_bytes;
    int result = memory_get_leak_stats(&total_allocs, &total_bytes, &peak_bytes);
    assert(result == LIBETUDE_SUCCESS);
    assert(total_allocs == 0); // 추적되지 않음
    assert(total_bytes == 0);

    memory_tracked_free(ptr, __FILE__, __LINE__, __FUNCTION__);
    memory_leak_detector_cleanup();

    printf("통과\n");
}

// 메모리 단편화 분석 테스트
void test_memory_fragmentation_analysis() {
    printf("메모리 단편화 분석 테스트... ");

    // 메모리 풀 생성
    void* pool = memory_create_pool(1, 64); // 1MB 풀
    assert(pool != NULL);

    // 여러 크기의 메모리 할당
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = memory_pool_alloc(1024 * (i + 1)); // 1KB, 2KB, ..., 10KB
        if (ptrs[i] == NULL) {
            // 메모리 부족으로 할당 실패 가능
            break;
        }
    }

    // 일부 메모리 해제 (단편화 유발)
    for (int i = 1; i < 10; i += 2) {
        if (ptrs[i] != NULL) {
            memory_pool_free(ptrs[i]);
        }
    }

    // 단편화 분석
    float fragmentation = memory_analyze_fragmentation();
    printf("단편화 비율: %.2f%% ", fragmentation * 100.0f);

    // 메모리 압축
    int result = memory_compact();
    assert(result == LIBETUDE_SUCCESS);

    // 압축 후 단편화 재측정
    float fragmentation_after = memory_analyze_fragmentation();
    printf("압축 후: %.2f%% ", fragmentation_after * 100.0f);

    // 남은 메모리 해제
    for (int i = 0; i < 10; i += 2) {
        if (ptrs[i] != NULL) {
            memory_pool_free(ptrs[i]);
        }
    }

    printf("통과\n");
}

// 메모리 풀 기본 기능 테스트
void test_memory_pool_basic() {
    printf("메모리 풀 기본 기능 테스트... ");

    // 메모리 풀 생성
    void* pool = memory_create_pool(2, 64); // 2MB 풀
    assert(pool != NULL);

    // 메모리 할당
    void* ptr1 = memory_pool_alloc(1024);
    void* ptr2 = memory_pool_alloc(2048);
    void* ptr3 = memory_pool_alloc(4096);

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    // 포인터들이 서로 다른지 확인
    assert(ptr1 != ptr2);
    assert(ptr2 != ptr3);
    assert(ptr1 != ptr3);

    // 풀 통계 확인
    size_t total_mb, used_mb, free_mb;
    float fragmentation;
    int result = memory_pool_get_stats(&total_mb, &used_mb, &free_mb, &fragmentation);
    assert(result == LIBETUDE_SUCCESS);
    assert(total_mb == 2);
    assert(used_mb > 0);
    assert(free_mb > 0);

    printf("풀 통계: %zuMB 총, %zuMB 사용, %zuMB 여유 ", total_mb, used_mb, free_mb);

    // 메모리 해제
    result = memory_pool_free(ptr1);
    assert(result == LIBETUDE_SUCCESS);

    result = memory_pool_free(ptr2);
    assert(result == LIBETUDE_SUCCESS);

    result = memory_pool_free(ptr3);
    assert(result == LIBETUDE_SUCCESS);

    printf("통과\n");
}

// 메모리 풀 병합 테스트
void test_memory_pool_coalescing() {
    printf("메모리 풀 병합 테스트... ");

    void* pool = memory_create_pool(1, 64); // 1MB 풀
    assert(pool != NULL);

    // 연속된 메모리 할당
    void* ptr1 = memory_pool_alloc(1024);
    void* ptr2 = memory_pool_alloc(1024);
    void* ptr3 = memory_pool_alloc(1024);

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    // 중간 블록 해제
    int result = memory_pool_free(ptr2);
    assert(result == LIBETUDE_SUCCESS);

    // 첫 번째 블록 해제 (병합 발생)
    result = memory_pool_free(ptr1);
    assert(result == LIBETUDE_SUCCESS);

    // 단편화 확인 (병합으로 인해 감소해야 함)
    float fragmentation = memory_analyze_fragmentation();
    printf("병합 후 단편화: %.2f%% ", fragmentation * 100.0f);

    // 마지막 블록 해제
    result = memory_pool_free(ptr3);
    assert(result == LIBETUDE_SUCCESS);

    printf("통과\n");
}

// 대용량 할당 테스트
void test_large_allocation() {
    printf("대용량 할당 테스트... ");

    memory_leak_detector_init();
    memory_set_tracking_enabled(true);

    // 대용량 메모리 할당
    void* large_ptr = memory_tracked_malloc(1024 * 1024, __FILE__, __LINE__, __FUNCTION__); // 1MB
    assert(large_ptr != NULL);

    // 통계 확인
    size_t total_allocs, total_bytes, peak_bytes;
    int result = memory_get_leak_stats(&total_allocs, &total_bytes, &peak_bytes);
    assert(result == LIBETUDE_SUCCESS);
    assert(total_allocs == 1);
    assert(total_bytes == 1024 * 1024);
    assert(peak_bytes >= total_bytes);

    printf("대용량 할당: %zu bytes ", total_bytes);

    // 메모리 해제
    memory_tracked_free(large_ptr, __FILE__, __LINE__, __FUNCTION__);

    // 누수 검사 (누수가 없어야 함)
    result = memory_check_leaks();
    assert(result == LIBETUDE_SUCCESS);

    memory_leak_detector_cleanup();

    printf("통과\n");
}

// 다중 할당/해제 테스트
void test_multiple_allocations() {
    printf("다중 할당/해제 테스트... ");

    memory_leak_detector_init();
    memory_set_tracking_enabled(true);

    const int num_allocs = 100;
    void* ptrs[num_allocs];

    // 다양한 크기로 할당
    for (int i = 0; i < num_allocs; i++) {
        size_t size = (i + 1) * 16; // 16, 32, 48, ... bytes
        ptrs[i] = memory_tracked_malloc(size, __FILE__, __LINE__, __FUNCTION__);
        assert(ptrs[i] != NULL);
    }

    // 통계 확인
    size_t total_allocs, total_bytes, peak_bytes;
    int result = memory_get_leak_stats(&total_allocs, &total_bytes, &peak_bytes);
    assert(result == LIBETUDE_SUCCESS);
    assert(total_allocs == num_allocs);

    printf("%zu개 할당, %zu bytes ", total_allocs, total_bytes);

    // 모든 메모리 해제
    for (int i = 0; i < num_allocs; i++) {
        memory_tracked_free(ptrs[i], __FILE__, __LINE__, __FUNCTION__);
    }

    // 누수 검사
    result = memory_check_leaks();
    assert(result == LIBETUDE_SUCCESS);

    memory_leak_detector_cleanup();

    printf("통과\n");
}

// 오류 처리 테스트
void test_error_handling() {
    printf("오류 처리 테스트... ");

    // NULL 포인터 테스트
    size_t total_allocs, total_bytes, peak_bytes;
    int result = memory_get_leak_stats(NULL, &total_bytes, &peak_bytes);
    assert(result == LIBETUDE_ERROR_INVALID_ARGUMENT);

    result = memory_get_leak_stats(&total_allocs, NULL, &peak_bytes);
    assert(result == LIBETUDE_ERROR_INVALID_ARGUMENT);

    result = memory_get_leak_stats(&total_allocs, &total_bytes, NULL);
    assert(result == LIBETUDE_ERROR_INVALID_ARGUMENT);

    // NULL 포인터 해제 (크래시하지 않아야 함)
    memory_tracked_free(NULL, __FILE__, __LINE__, __FUNCTION__);

    // 잘못된 풀 통계 요청
    result = memory_pool_get_stats(NULL, NULL, NULL, NULL);
    assert(result == LIBETUDE_ERROR_INVALID_ARGUMENT);

    printf("통과\n");
}

int main() {
    printf("메모리 누수 감지기 및 단편화 최소화 테스트 시작\n");
    printf("================================================\n");

    test_memory_leak_detector_init();
    test_memory_leak_detection();
    test_memory_tracking_disable();
    test_memory_fragmentation_analysis();
    test_memory_pool_basic();
    test_memory_pool_coalescing();
    test_large_allocation();
    test_multiple_allocations();
    test_error_handling();

    printf("================================================\n");
    printf("모든 메모리 누수 감지기 테스트 통과!\n");

    return 0;
}