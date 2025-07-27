/**
 * @file profiler.c
 * @brief LibEtude 성능 프로파일러 구현
 *
 * 연산 시간 측정, 메모리 사용량 추적, CPU/GPU 사용률 모니터링을 제공합니다.
 */

#include "libetude/profiler.h"
#include "libetude/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stddef.h>

#ifdef __APPLE__
#include <mach/mach_time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

// 내부 상수
#define MAX_OP_NAME_LENGTH 64
#define PROFILER_MAGIC 0x50524F46  // "PROF"

// 내부 구조체
typedef struct ActiveProfile {
    char op_name[MAX_OP_NAME_LENGTH];
    uint64_t start_time;
    uint64_t start_cycles;
    size_t start_memory;
    struct ActiveProfile* next;
} ActiveProfile;

// 확장된 프로파일러 구조체
typedef struct ProfilerImpl {
    Profiler public;            // 공개 인터페이스
    uint32_t magic;             // 매직 넘버
    ActiveProfile* active_list; // 활성 프로파일 리스트
    pthread_mutex_t mutex;      // 스레드 안전성
    char* string_pool;          // 문자열 풀
    size_t string_pool_size;
    size_t string_pool_used;
} ProfilerImpl;

// 내부 함수 선언
static ProfilerImpl* get_impl(Profiler* profiler);
static const char* store_string(ProfilerImpl* impl, const char* str);
static ActiveProfile* find_active_profile(ProfilerImpl* impl, const char* name);
static void remove_active_profile(ProfilerImpl* impl, const char* name);
static void update_system_stats(ProfilerImpl* impl);
static void write_json_report(ProfilerImpl* impl, FILE* file);

Profiler* rt_create_profiler(int capacity) {
    if (capacity <= 0) {
        return NULL;
    }

    ProfilerImpl* impl = (ProfilerImpl*)malloc(sizeof(ProfilerImpl));
    if (!impl) {
        return NULL;
    }

    // 공개 구조체 초기화
    impl->public.entries = (ProfileEntry*)calloc(capacity, sizeof(ProfileEntry));
    if (!impl->public.entries) {
        free(impl);
        return NULL;
    }

    impl->public.entry_count = 0;
    impl->public.capacity = capacity;
    impl->public.total_inference_time = 0;
    impl->public.total_memory_peak = 0;
    impl->public.avg_cpu_usage = 0.0f;
    impl->public.avg_gpu_usage = 0.0f;
    impl->public.is_profiling = true;
    impl->public.active_profiles = 0;
    impl->public.session_start_time = rt_get_current_time_ns();
    impl->public.last_update_time = impl->public.session_start_time;

    // 내부 구조체 초기화
    impl->magic = PROFILER_MAGIC;
    impl->active_list = NULL;

    // 문자열 풀 초기화
    impl->string_pool_size = capacity * MAX_OP_NAME_LENGTH;
    impl->string_pool = (char*)malloc(impl->string_pool_size);
    if (!impl->string_pool) {
        free(impl->public.entries);
        free(impl);
        return NULL;
    }
    impl->string_pool_used = 0;

    // 뮤텍스 초기화
    if (pthread_mutex_init(&impl->mutex, NULL) != 0) {
        free(impl->string_pool);
        free(impl->public.entries);
        free(impl);
        return NULL;
    }

    return &impl->public;
}

void rt_destroy_profiler(Profiler* profiler) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl) {
        return;
    }

    pthread_mutex_lock(&impl->mutex);

    // 활성 프로파일 정리
    ActiveProfile* current = impl->active_list;
    while (current) {
        ActiveProfile* next = current->next;
        free(current);
        current = next;
    }

    // 메모리 해제
    free(impl->public.entries);
    free(impl->string_pool);

    pthread_mutex_unlock(&impl->mutex);
    pthread_mutex_destroy(&impl->mutex);

    free(impl);
}

ETResult rt_start_profile(Profiler* profiler, const char* name) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl || !name || !impl->public.is_profiling) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&impl->mutex);

    // 이미 활성화된 프로파일인지 확인
    if (find_active_profile(impl, name)) {
        pthread_mutex_unlock(&impl->mutex);
        return ET_ERROR_INVALID_STATE;
    }

    // 새 활성 프로파일 생성
    ActiveProfile* active = (ActiveProfile*)malloc(sizeof(ActiveProfile));
    if (!active) {
        pthread_mutex_unlock(&impl->mutex);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    strncpy(active->op_name, name, MAX_OP_NAME_LENGTH - 1);
    active->op_name[MAX_OP_NAME_LENGTH - 1] = '\0';
    active->start_time = rt_get_current_time_ns();
    active->start_cycles = rt_get_cpu_cycles();
    active->start_memory = 0; // TODO: 현재 메모리 사용량 조회
    active->next = impl->active_list;

    impl->active_list = active;
    impl->public.active_profiles++;

    pthread_mutex_unlock(&impl->mutex);
    return ET_SUCCESS;
}

ETResult rt_end_profile(Profiler* profiler, const char* name) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl || !name || !impl->public.is_profiling) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uint64_t end_time = rt_get_current_time_ns();
    uint64_t end_cycles = rt_get_cpu_cycles();

    pthread_mutex_lock(&impl->mutex);

    // 활성 프로파일 찾기
    ActiveProfile* active = find_active_profile(impl, name);
    if (!active) {
        pthread_mutex_unlock(&impl->mutex);
        return ET_ERROR_NOT_FOUND;
    }

    // 프로파일 항목이 가득 찬 경우
    if (impl->public.entry_count >= impl->public.capacity) {
        pthread_mutex_unlock(&impl->mutex);
        return ET_ERROR_BUFFER_FULL;
    }

    // 프로파일 항목 생성
    ProfileEntry* entry = &impl->public.entries[impl->public.entry_count];
    entry->op_name = store_string(impl, name);
    entry->start_time = active->start_time;
    entry->end_time = end_time;
    entry->cpu_cycles = end_cycles - active->start_cycles;
    entry->memory_used = 0; // 나중에 업데이트됨
    entry->memory_peak = 0; // 나중에 업데이트됨
    entry->cpu_usage = 0.0f; // 나중에 업데이트됨
    entry->gpu_usage = 0.0f; // 나중에 업데이트됨

    impl->public.entry_count++;
    impl->public.total_inference_time += (end_time - active->start_time);

    // 활성 프로파일 제거
    remove_active_profile(impl, name);
    impl->public.active_profiles--;

    // 시스템 통계 업데이트
    update_system_stats(impl);

    pthread_mutex_unlock(&impl->mutex);
    return ET_SUCCESS;
}

void rt_update_memory_usage(Profiler* profiler, const char* name,
                           size_t memory_used, size_t memory_peak) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl || !name) {
        return;
    }

    pthread_mutex_lock(&impl->mutex);

    // 해당 연산의 최신 프로파일 항목 찾기
    for (int i = impl->public.entry_count - 1; i >= 0; i--) {
        ProfileEntry* entry = &impl->public.entries[i];
        if (strcmp(entry->op_name, name) == 0) {
            entry->memory_used = memory_used;
            entry->memory_peak = memory_peak;

            // 전체 통계 업데이트
            if (memory_peak > impl->public.total_memory_peak) {
                impl->public.total_memory_peak = memory_peak;
            }
            break;
        }
    }

    pthread_mutex_unlock(&impl->mutex);
}

void rt_update_resource_usage(Profiler* profiler, float cpu_usage, float gpu_usage) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl) {
        return;
    }

    pthread_mutex_lock(&impl->mutex);

    // 이동 평균으로 CPU/GPU 사용률 업데이트
    const float alpha = 0.1f; // 평활화 계수
    impl->public.avg_cpu_usage = impl->public.avg_cpu_usage * (1.0f - alpha) + cpu_usage * alpha;
    impl->public.avg_gpu_usage = impl->public.avg_gpu_usage * (1.0f - alpha) + gpu_usage * alpha;

    impl->public.last_update_time = rt_get_current_time_ns();

    pthread_mutex_unlock(&impl->mutex);
}

ETResult rt_generate_report(Profiler* profiler, const char* output_path) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl || !output_path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(output_path, "w");
    if (!file) {
        return ET_ERROR_IO;
    }

    pthread_mutex_lock(&impl->mutex);
    write_json_report(impl, file);
    pthread_mutex_unlock(&impl->mutex);

    fclose(file);
    return ET_SUCCESS;
}

const ProfileEntry* rt_get_profile_stats(Profiler* profiler, const char* op_name) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl) {
        return NULL;
    }

    pthread_mutex_lock(&impl->mutex);

    if (!op_name) {
        // 전체 통계는 첫 번째 항목으로 반환 (임시)
        pthread_mutex_unlock(&impl->mutex);
        return impl->public.entry_count > 0 ? &impl->public.entries[0] : NULL;
    }

    // 특정 연산의 최신 통계 찾기
    for (int i = impl->public.entry_count - 1; i >= 0; i--) {
        if (strcmp(impl->public.entries[i].op_name, op_name) == 0) {
            pthread_mutex_unlock(&impl->mutex);
            return &impl->public.entries[i];
        }
    }

    pthread_mutex_unlock(&impl->mutex);
    return NULL;
}

void rt_reset_profiler(Profiler* profiler) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl) {
        return;
    }

    pthread_mutex_lock(&impl->mutex);

    impl->public.entry_count = 0;
    impl->public.total_inference_time = 0;
    impl->public.total_memory_peak = 0;
    impl->public.avg_cpu_usage = 0.0f;
    impl->public.avg_gpu_usage = 0.0f;
    impl->public.session_start_time = rt_get_current_time_ns();
    impl->public.last_update_time = impl->public.session_start_time;
    impl->string_pool_used = 0;

    pthread_mutex_unlock(&impl->mutex);
}

void rt_enable_profiling(Profiler* profiler, bool enable) {
    ProfilerImpl* impl = get_impl(profiler);
    if (!impl) {
        return;
    }

    pthread_mutex_lock(&impl->mutex);
    impl->public.is_profiling = enable;
    pthread_mutex_unlock(&impl->mutex);
}

uint64_t rt_get_current_time_ns(void) {
#ifdef __APPLE__
    static mach_timebase_info_data_t timebase_info;
    static bool initialized = false;

    if (!initialized) {
        mach_timebase_info(&timebase_info);
        initialized = true;
    }

    uint64_t time = mach_absolute_time();
    return time * timebase_info.numer / timebase_info.denom;
#elif defined(_WIN32)
    static LARGE_INTEGER frequency;
    static bool initialized = false;

    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = true;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000000ULL) / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t rt_get_cpu_cycles(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    // 플랫폼별 구현이 없는 경우 시간으로 대체
    return rt_get_current_time_ns();
#endif
}

// 내부 함수 구현

static ProfilerImpl* get_impl(Profiler* profiler) {
    if (!profiler) {
        return NULL;
    }

    // ProfilerImpl의 첫 번째 멤버가 public이므로 직접 캐스팅 가능
    ProfilerImpl* impl = (ProfilerImpl*)profiler;
    return (impl->magic == PROFILER_MAGIC) ? impl : NULL;
}

static const char* store_string(ProfilerImpl* impl, const char* str) {
    size_t len = strlen(str) + 1;
    if (impl->string_pool_used + len > impl->string_pool_size) {
        return str; // 풀이 가득 찬 경우 원본 반환
    }

    char* stored = impl->string_pool + impl->string_pool_used;
    strcpy(stored, str);
    impl->string_pool_used += len;
    return stored;
}

static ActiveProfile* find_active_profile(ProfilerImpl* impl, const char* name) {
    ActiveProfile* current = impl->active_list;
    while (current) {
        if (strcmp(current->op_name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void remove_active_profile(ProfilerImpl* impl, const char* name) {
    ActiveProfile** current = &impl->active_list;
    while (*current) {
        if (strcmp((*current)->op_name, name) == 0) {
            ActiveProfile* to_remove = *current;
            *current = (*current)->next;
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
}

static void update_system_stats(ProfilerImpl* impl) {
    // 시스템 리소스 사용률 업데이트
    // 플랫폼별 구현 필요

#ifdef __APPLE__
    // macOS 구현
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // CPU 시간을 사용률로 변환 (간단한 근사)
        uint64_t current_time = rt_get_current_time_ns();
        uint64_t elapsed = current_time - impl->public.last_update_time;
        if (elapsed > 0) {
            uint64_t cpu_time = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000000ULL +
                               (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1000ULL;
            float cpu_usage = (float)cpu_time / (float)elapsed;
            // 직접 업데이트하여 재귀 호출 방지
            const float alpha = 0.1f;
            impl->public.avg_cpu_usage = impl->public.avg_cpu_usage * (1.0f - alpha) + cpu_usage * alpha;
        }
    }
#elif defined(_WIN32)
    // Windows 구현
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        // 메모리 사용량 업데이트
        if (pmc.PeakWorkingSetSize > impl->public.total_memory_peak) {
            impl->public.total_memory_peak = pmc.PeakWorkingSetSize;
        }
    }
#else
    // Linux 구현
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        size_t memory_kb = usage.ru_maxrss;
        size_t memory_bytes = memory_kb * 1024;
        if (memory_bytes > impl->public.total_memory_peak) {
            impl->public.total_memory_peak = memory_bytes;
        }
    }
#endif
}

static void write_json_report(ProfilerImpl* impl, FILE* file) {
    fprintf(file, "{\n");
    fprintf(file, "  \"session\": {\n");
    fprintf(file, "    \"start_time\": %llu,\n", impl->public.session_start_time);
    fprintf(file, "    \"last_update\": %llu,\n", impl->public.last_update_time);
    fprintf(file, "    \"total_inference_time\": %llu,\n", impl->public.total_inference_time);
    fprintf(file, "    \"total_memory_peak\": %zu,\n", impl->public.total_memory_peak);
    fprintf(file, "    \"avg_cpu_usage\": %.3f,\n", impl->public.avg_cpu_usage);
    fprintf(file, "    \"avg_gpu_usage\": %.3f\n", impl->public.avg_gpu_usage);
    fprintf(file, "  },\n");

    fprintf(file, "  \"operations\": [\n");
    for (int i = 0; i < impl->public.entry_count; i++) {
        const ProfileEntry* entry = &impl->public.entries[i];
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", entry->op_name);
        fprintf(file, "      \"start_time\": %llu,\n", entry->start_time);
        fprintf(file, "      \"end_time\": %llu,\n", entry->end_time);
        fprintf(file, "      \"duration_ns\": %llu,\n", entry->end_time - entry->start_time);
        fprintf(file, "      \"cpu_cycles\": %llu,\n", entry->cpu_cycles);
        fprintf(file, "      \"memory_used\": %zu,\n", entry->memory_used);
        fprintf(file, "      \"memory_peak\": %zu,\n", entry->memory_peak);
        fprintf(file, "      \"cpu_usage\": %.3f,\n", entry->cpu_usage);
        fprintf(file, "      \"gpu_usage\": %.3f\n", entry->gpu_usage);
        fprintf(file, "    }%s\n", (i < impl->public.entry_count - 1) ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
}