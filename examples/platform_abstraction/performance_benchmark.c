/**
 * @file performance_benchmark.c
 * @brief 플랫폼 추상화 레이어 성능 측정 및 비교 예제
 *
 * 이 예제는 플랫폼 추상화 레이어의 성능을 측정하고
 * 직접 플랫폼 API 호출과 비교하는 벤치마크를 제공합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libetude/platform/factory.h>
#include <libetude/platform/audio.h>
#include <libetude/platform/system.h>
#include <libetude/platform/threading.h>
#include <libetude/platform/memory.h>
#include <libetude/platform/filesystem.h>
#include <libetude/error.h>

// 플랫폼별 직접 API 포함
#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <time.h>
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <mach/mach_time.h>
#endif

// 전역 인터페이스
static ETSystemInterface* g_system = NULL;
static ETThreadInterface* g_thread = NULL;
static ETMemoryInterface* g_memory = NULL;
static ETFilesystemInterface* g_filesystem = NULL;

// 벤치마크 결과 구조체
typedef struct {
    const char* name;
    uint64_t abstraction_time_ns;
    uint64_t direct_time_ns;
    double overhead_percent;
    int iterations;
} BenchmarkResult;

static BenchmarkResult g_results[16];
static int g_result_count = 0;

/**
 * @brief 벤치마크 결과 기록
 */
void record_benchmark(const char* name, uint64_t abstraction_time,
                     uint64_t direct_time, int iterations) {
    if (g_result_count >= 16) return;

    BenchmarkResult* result = &g_results[g_result_count++];
    result->name = name;
    result->abstraction_time_ns = abstraction_time;
    result->direct_time_ns = direct_time;
    result->iterations = iterations;

    if (direct_time > 0) {
        result->overhead_percent =
            ((double)(abstraction_time - direct_time) / direct_time) * 100.0;
    } else {
        result->overhead_percent = 0.0;
    }
}

/**
 * @brief 고해상도 타이머 벤치마크
 */
void benchmark_high_resolution_timer(void) {
    printf("고해상도 타이머 벤치마크...\n");

    const int iterations = 1000000;
    uint64_t start, end;

    // 추상화 레이어 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        uint64_t dummy;
        g_system->get_high_resolution_time(&dummy);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t abstraction_time = end - start;

    // 직접 API 측정
    uint64_t direct_time = 0;

#ifdef _WIN32
    LARGE_INTEGER freq, start_direct, end_direct;
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start_direct);
    for (int i = 0; i < iterations; i++) {
        LARGE_INTEGER dummy;
        QueryPerformanceCounter(&dummy);
    }
    QueryPerformanceCounter(&end_direct);

    direct_time = ((end_direct.QuadPart - start_direct.QuadPart) * 1000000000) / freq.QuadPart;

#elif defined(__linux__)
    struct timespec start_direct, end_direct;

    clock_gettime(CLOCK_MONOTONIC, &start_direct);
    for (int i = 0; i < iterations; i++) {
        struct timespec dummy;
        clock_gettime(CLOCK_MONOTONIC, &dummy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_direct);

    direct_time = (end_direct.tv_sec - start_direct.tv_sec) * 1000000000 +
                  (end_direct.tv_nsec - start_direct.tv_nsec);

#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }

    uint64_t start_direct = mach_absolute_time();
    for (int i = 0; i < iterations; i++) {
        uint64_t dummy = mach_absolute_time();
        (void)dummy;
    }
    uint64_t end_direct = mach_absolute_time();

    direct_time = ((end_direct - start_direct) * timebase.numer) / timebase.denom;
#endif

    record_benchmark("고해상도 타이머", abstraction_time, direct_time, iterations);
}

/**
 * @brief 시스템 정보 조회 벤치마크
 */
void benchmark_system_info(void) {
    printf("시스템 정보 조회 벤치마크...\n");

    const int iterations = 10000;
    uint64_t start, end;

    // 추상화 레이어 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        ETSystemInfo info;
        g_system->get_system_info(&info);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t abstraction_time = end - start;

    // 직접 API 측정
    uint64_t direct_time = 0;

#ifdef _WIN32
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);

        MEMORYSTATUSEX meminfo;
        meminfo.dwLength = sizeof(meminfo);
        GlobalMemoryStatusEx(&meminfo);
    }
    g_system->get_high_resolution_time(&end);
    direct_time = end - start;

#elif defined(__linux__)
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
        struct sysinfo info;
        sysinfo(&info);
        (void)cpu_count;
    }
    g_system->get_high_resolution_time(&end);
    direct_time = end - start;

#elif defined(__APPLE__)
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        size_t size = sizeof(int);
        int cpu_count;
        sysctlbyname("hw.ncpu", &cpu_count, &size, NULL, 0);

        uint64_t memory;
        size = sizeof(memory);
        sysctlbyname("hw.memsize", &memory, &size, NULL, 0);
    }
    g_system->get_high_resolution_time(&end);
    direct_time = end - start;
#endif

    record_benchmark("시스템 정보 조회", abstraction_time, direct_time, iterations);
}

/**
 * @brief 메모리 할당 벤치마크
 */
void benchmark_memory_allocation(void) {
    printf("메모리 할당 벤치마크...\n");

    const int iterations = 100000;
    const size_t alloc_size = 1024;
    uint64_t start, end;

    // 추상화 레이어 측정 (일반 할당)
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        void* ptr = g_memory->malloc(alloc_size);
        if (ptr) g_memory->free(ptr);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t abstraction_time = end - start;

    // 직접 API 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        void* ptr = malloc(alloc_size);
        if (ptr) free(ptr);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t direct_time = end - start;

    record_benchmark("메모리 할당 (일반)", abstraction_time, direct_time, iterations);

    // 정렬된 메모리 할당 벤치마크
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        void* ptr = g_memory->aligned_malloc(alloc_size, 32);
        if (ptr) g_memory->aligned_free(ptr);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t aligned_abstraction_time = end - start;

    // 직접 정렬된 할당
    uint64_t aligned_direct_time = 0;

#ifdef _WIN32
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        void* ptr = _aligned_malloc(alloc_size, 32);
        if (ptr) _aligned_free(ptr);
    }
    g_system->get_high_resolution_time(&end);
    aligned_direct_time = end - start;

#else
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        void* ptr;
        if (posix_memalign(&ptr, 32, alloc_size) == 0) {
            free(ptr);
        }
    }
    g_system->get_high_resolution_time(&end);
    aligned_direct_time = end - start;
#endif

    record_benchmark("메모리 할당 (정렬)", aligned_abstraction_time,
                    aligned_direct_time, iterations);
}

/**
 * @brief 스레드 생성 벤치마크
 */
void* dummy_thread_func(void* arg) {
    return NULL;
}

void benchmark_thread_creation(void) {
    printf("스레드 생성 벤치마크...\n");

    const int iterations = 1000;
    uint64_t start, end;

    // 추상화 레이어 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        ETThread* thread;
        if (g_thread->create_thread(&thread, dummy_thread_func, NULL) == ET_SUCCESS) {
            void* result;
            g_thread->join_thread(thread, &result);
            g_thread->destroy_thread(thread);
        }
    }
    g_system->get_high_resolution_time(&end);
    uint64_t abstraction_time = end - start;

    // 직접 API 측정
    uint64_t direct_time = 0;

#ifdef _WIN32
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dummy_thread_func,
                                   NULL, 0, NULL);
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }
    g_system->get_high_resolution_time(&end);
    direct_time = end - start;

#else
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, dummy_thread_func, NULL) == 0) {
            void* result;
            pthread_join(thread, &result);
        }
    }
    g_system->get_high_resolution_time(&end);
    direct_time = end - start;
#endif

    record_benchmark("스레드 생성", abstraction_time, direct_time, iterations);
}

/**
 * @brief 파일 I/O 벤치마크
 */
void benchmark_file_io(void) {
    printf("파일 I/O 벤치마크...\n");

    const int iterations = 1000;
    const char* test_data = "LibEtude 플랫폼 추상화 레이어 테스트 데이터입니다.";
    const size_t data_size = strlen(test_data);
    uint64_t start, end;

    // 추상화 레이어 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "test_abstraction_%d.txt", i);

        ETFile* file;
        if (g_filesystem->open_file(filename, ET_FILE_MODE_WRITE, &file) == ET_SUCCESS) {
            size_t written;
            g_filesystem->write_file(file, test_data, data_size, &written);
            g_filesystem->close_file(file);
        }

        // 파일 삭제 (정리)
        remove(filename);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t abstraction_time = end - start;

    // 직접 API 측정
    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "test_direct_%d.txt", i);

        FILE* file = fopen(filename, "w");
        if (file) {
            fwrite(test_data, 1, data_size, file);
            fclose(file);
        }

        // 파일 삭제 (정리)
        remove(filename);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t direct_time = end - start;

    record_benchmark("파일 I/O", abstraction_time, direct_time, iterations);
}

/**
 * @brief 함수 호출 오버헤드 벤치마크
 */
void benchmark_function_call_overhead(void) {
    printf("함수 호출 오버헤드 벤치마크...\n");

    const int iterations = 10000000;
    uint64_t start, end;

    // 함수 포인터를 통한 간접 호출 (추상화 레이어 방식)
    ETResult (*get_time_func)(uint64_t*) = g_system->get_high_resolution_time;

    g_system->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        uint64_t dummy;
        get_time_func(&dummy);
    }
    g_system->get_high_resolution_time(&end);
    uint64_t indirect_time = end - start;

    // 직접 함수 호출
    uint64_t direct_time = 0;

#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER start_direct, end_direct;
    QueryPerformanceCounter(&start_direct);
    for (int i = 0; i < iterations; i++) {
        LARGE_INTEGER dummy;
        QueryPerformanceCounter(&dummy);
    }
    QueryPerformanceCounter(&end_direct);

    direct_time = ((end_direct.QuadPart - start_direct.QuadPart) * 1000000000) / freq.QuadPart;

#elif defined(__linux__)
    struct timespec start_direct, end_direct;

    clock_gettime(CLOCK_MONOTONIC, &start_direct);
    for (int i = 0; i < iterations; i++) {
        struct timespec dummy;
        clock_gettime(CLOCK_MONOTONIC, &dummy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_direct);

    direct_time = (end_direct.tv_sec - start_direct.tv_sec) * 1000000000 +
                  (end_direct.tv_nsec - start_direct.tv_nsec);

#elif defined(__APPLE__)
    uint64_t start_direct = mach_absolute_time();
    for (int i = 0; i < iterations; i++) {
        uint64_t dummy = mach_absolute_time();
        (void)dummy;
    }
    uint64_t end_direct = mach_absolute_time();

    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }

    direct_time = ((end_direct - start_direct) * timebase.numer) / timebase.denom;
#endif

    record_benchmark("함수 호출 오버헤드", indirect_time, direct_time, iterations);
}

/**
 * @brief 벤치마크 결과 출력
 */
void print_benchmark_results(void) {
    printf("\n=== 벤치마크 결과 요약 ===\n");
    printf("%-25s %15s %15s %10s %10s\n",
           "테스트", "추상화(ns)", "직접(ns)", "오버헤드", "반복수");
    printf("%-25s %15s %15s %10s %10s\n",
           "-------------------------", "---------------", "---------------",
           "----------", "----------");

    double total_overhead = 0.0;
    int valid_results = 0;

    for (int i = 0; i < g_result_count; i++) {
        BenchmarkResult* result = &g_results[i];

        printf("%-25s %15llu %15llu %9.1f%% %10d\n",
               result->name,
               result->abstraction_time_ns,
               result->direct_time_ns,
               result->overhead_percent,
               result->iterations);

        if (result->overhead_percent >= 0) {
            total_overhead += result->overhead_percent;
            valid_results++;
        }
    }

    if (valid_results > 0) {
        printf("\n평균 오버헤드: %.1f%%\n", total_overhead / valid_results);
    }

    printf("\n=== 성능 분석 ===\n");

    // 성능 등급 분류
    for (int i = 0; i < g_result_count; i++) {
        BenchmarkResult* result = &g_results[i];
        const char* grade;

        if (result->overhead_percent < 1.0) {
            grade = "우수 (< 1%)";
        } else if (result->overhead_percent < 5.0) {
            grade = "양호 (1-5%)";
        } else if (result->overhead_percent < 10.0) {
            grade = "보통 (5-10%)";
        } else {
            grade = "개선 필요 (> 10%)";
        }

        printf("%-25s: %s\n", result->name, grade);
    }
}

/**
 * @brief 벤치마크 결과를 파일에 저장
 */
void save_benchmark_results(void) {
    printf("\n벤치마크 결과를 파일에 저장 중...\n");

    ETFile* file;
    ETResult result = g_filesystem->open_file("benchmark_results.csv",
                                             ET_FILE_MODE_WRITE, &file);
    if (result != ET_SUCCESS) {
        printf("결과 파일 생성 실패: %d\n", result);
        return;
    }

    // CSV 헤더
    const char* header = "테스트,추상화시간(ns),직접시간(ns),오버헤드(%),반복수\n";
    size_t written;
    g_filesystem->write_file(file, header, strlen(header), &written);

    // 데이터 행들
    for (int i = 0; i < g_result_count; i++) {
        BenchmarkResult* result = &g_results[i];
        char line[256];

        snprintf(line, sizeof(line), "%s,%llu,%llu,%.1f,%d\n",
                result->name,
                result->abstraction_time_ns,
                result->direct_time_ns,
                result->overhead_percent,
                result->iterations);

        g_filesystem->write_file(file, line, strlen(line), &written);
    }

    g_filesystem->close_file(file);
    printf("결과가 benchmark_results.csv에 저장되었습니다.\n");
}

int main(void) {
    printf("=== LibEtude 플랫폼 추상화 레이어 성능 벤치마크 ===\n\n");

    // 플랫폼 초기화
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        printf("플랫폼 초기화 실패: %d\n", result);
        return 1;
    }

    // 인터페이스 획득
    g_system = et_platform_get_system_interface();
    g_thread = et_platform_get_thread_interface();
    g_memory = et_platform_get_memory_interface();
    g_filesystem = et_platform_get_filesystem_interface();

    if (!g_system || !g_thread || !g_memory || !g_filesystem) {
        printf("인터페이스 획득 실패\n");
        et_platform_cleanup();
        return 1;
    }

    printf("벤치마크 시작...\n\n");

    // 각종 벤치마크 실행
    benchmark_high_resolution_timer();
    benchmark_system_info();
    benchmark_memory_allocation();
    benchmark_thread_creation();
    benchmark_file_io();
    benchmark_function_call_overhead();

    // 결과 출력 및 저장
    print_benchmark_results();
    save_benchmark_results();

    // 정리
    et_platform_cleanup();

    printf("\n벤치마크 완료!\n");
    return 0;
}