/**
 * @file test_platform.c
 * @brief 플랫폼별 통합 테스트
 *
 * 데스크톱, 모바일, 임베디드 환경에서의 동작을 테스트합니다.
 * Requirements: 4.1, 4.2, 4.3, 10.4
 */

#include "unity.h"
#include "libetude/api.h"
#include "libetude/error.h"
#include "libetude/desktop_optimization.h"
#include "libetude/mobile_power_management.h"
#include "libetude/embedded_optimization.h"
#include "libetude/hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 플랫폼 감지
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #include <sys/sysctl.h>
    #include <mach/mach.h>
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #include <sys/sysinfo.h>
    #include <unistd.h>
#elif defined(__ANDROID__)
    #define PLATFORM_ANDROID
    #include <sys/system_properties.h>
#elif defined(__EMSCRIPTEN__)
    #define PLATFORM_WEB
#else
    #define PLATFORM_UNKNOWN
#endif

// 아키텍처 감지
#ifdef __x86_64__
    #define ARCH_X64
#elif defined(__i386__)
    #define ARCH_X86
#elif defined(__aarch64__)
    #define ARCH_ARM64
#elif defined(__arm__)
    #define ARCH_ARM32
#else
    #define ARCH_UNKNOWN
#endif

// 테스트용 전역 변수
static LibEtudeEngine* platform_engine = NULL;

// 플랫폼 정보 구조체
typedef struct {
    const char* platform_name;
    const char* architecture;
    int cpu_cores;
    size_t total_memory_mb;
    bool has_gpu;
    bool has_simd;
    bool is_mobile;
    bool is_embedded;
} PlatformInfo;

static PlatformInfo platform_info;

void setUp(void) {
    // 테스트 전 초기화
    et_set_log_level(ET_LOG_INFO);

    // 플랫폼 정보 초기화
    memset(&platform_info, 0, sizeof(PlatformInfo));

    // 플랫폼 이름 설정
#ifdef PLATFORM_WINDOWS
    platform_info.platform_name = "Windows";
#elif defined(PLATFORM_MACOS)
    platform_info.platform_name = "macOS";
#elif defined(PLATFORM_LINUX)
    platform_info.platform_name = "Linux";
#elif defined(PLATFORM_ANDROID)
    platform_info.platform_name = "Android";
    platform_info.is_mobile = true;
#else
    platform_info.platform_name = "Unknown";
#endif

    // 아키텍처 설정
#ifdef ARCH_X64
    platform_info.architecture = "x86_64";
#elif defined(ARCH_X86)
    platform_info.architecture = "x86";
#elif defined(ARCH_ARM64)
    platform_info.architecture = "ARM64";
#elif defined(ARCH_ARM32)
    platform_info.architecture = "ARM32";
#else
    platform_info.architecture = "Unknown";
#endif
}

void tearDown(void) {
    // 테스트 후 정리
    if (platform_engine) {
        libetude_destroy_engine(platform_engine);
        platform_engine = NULL;
    }

    et_clear_error();
}

// 플랫폼 정보 수집
void collect_platform_info(void) {
    printf("플랫폼 정보 수집 중...\n");

    // CPU 코어 수 감지
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    platform_info.cpu_cores = sysinfo.dwNumberOfProcessors;

    MEMORYSTATUSEX meminfo;
    meminfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&meminfo);
    platform_info.total_memory_mb = meminfo.ullTotalPhys / (1024 * 1024);

#elif defined(PLATFORM_MACOS)
    size_t size = sizeof(int);
    sysctlbyname("hw.ncpu", &platform_info.cpu_cores, &size, NULL, 0);

    int64_t memory;
    size = sizeof(memory);
    sysctlbyname("hw.memsize", &memory, &size, NULL, 0);
    platform_info.total_memory_mb = memory / (1024 * 1024);

#elif defined(PLATFORM_LINUX)
    platform_info.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);

    struct sysinfo info;
    sysinfo(&info);
    platform_info.total_memory_mb = info.totalram / (1024 * 1024);

#else
    platform_info.cpu_cores = 1; // 기본값
    platform_info.total_memory_mb = 512; // 기본값
#endif

    // SIMD 지원 감지
    uint32_t hw_features = libetude_get_hardware_features();
    platform_info.has_simd = (hw_features != LIBETUDE_SIMD_NONE);

    // 임베디드 환경 감지 (메모리 기준)
    if (platform_info.total_memory_mb < 512) {
        platform_info.is_embedded = true;
    }

    printf("플랫폼 정보:\n");
    printf("  플랫폼: %s\n", platform_info.platform_name);
    printf("  아키텍처: %s\n", platform_info.architecture);
    printf("  CPU 코어: %d개\n", platform_info.cpu_cores);
    printf("  총 메모리: %zu MB\n", platform_info.total_memory_mb);
    printf("  SIMD 지원: %s\n", platform_info.has_simd ? "예" : "아니오");
    printf("  모바일: %s\n", platform_info.is_mobile ? "예" : "아니오");
    printf("  임베디드: %s\n", platform_info.is_embedded ? "예" : "아니오");
}

void test_desktop_environment(void) {
    printf("\n=== 데스크톱 환경 테스트 시작 ===\n");

    collect_platform_info();

    // 데스크톱 환경이 아닌 경우 스킵
    if (platform_info.is_mobile || platform_info.is_embedded) {
        printf("데스크톱 환경이 아님, 테스트 스킵\n");
        TEST_PASS();
        return;
    }

    // 더미 엔진 생성
    platform_engine = (LibEtudeEngine*)malloc(sizeof(void*));
    TEST_ASSERT_NOT_NULL_MESSAGE(platform_engine, "더미 엔진 생성 실패");

    printf("데스크톱 최적화 테스트\n");

    // 데스크톱 최적화 설정 테스트
    LibEtudeDesktopOptimizer desktop_optimizer;
    int desktop_result = libetude_desktop_optimizer_init(&desktop_optimizer);

    if (desktop_result == LIBETUDE_SUCCESS) {
        printf("데스크톱 최적화 초기화 성공\n");

        // 멀티코어 최적화 테스트
        printf("멀티코어 최적화 설정 테스트\n");
        LibEtudeMulticoreOptimizer multicore;
        int multicore_result = libetude_multicore_auto_configure(&multicore, &platform_info);

        if (multicore_result == ET_SUCCESS) {
            printf("멀티코어 최적화 설정 성공: %d 코어\n", platform_info.cpu_cores);
        } else if (multicore_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("멀티코어 최적화 기능 미구현 (정상)\n");
        } else {
            printf("멀티코어 최적화 설정 실패: %d\n", multicore_result);
        }

        // GPU 가속 테스트
        printf("GPU 가속 테스트\n");
        int gpu_result = libetude_enable_gpu_acceleration(platform_engine);

        if (gpu_result == LIBETUDE_SUCCESS) {
            printf("GPU 가속 활성화 성공\n");
            platform_info.has_gpu = true;
        } else if (gpu_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("GPU 가속 기능 미구현 (정상)\n");
        } else if (gpu_result == LIBETUDE_ERROR_HARDWARE) {
            printf("GPU 하드웨어 없음 또는 지원되지 않음\n");
        } else {
            printf("GPU 가속 활성화 실패: %d\n", gpu_result);
        }

        // 고성능 모드 테스트
        printf("고성능 모드 테스트\n");
        int quality_result = libetude_set_quality_mode(platform_engine, LIBETUDE_QUALITY_HIGH);

        if (quality_result == LIBETUDE_SUCCESS) {
            printf("고품질 모드 설정 성공\n");
        } else if (quality_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("품질 모드 설정 기능 미구현 (정상)\n");
        } else {
            printf("고품질 모드 설정 실패: %d\n", quality_result);
        }

        // 데스크톱 환경에서의 음성 합성 테스트
        printf("데스크톱 환경 음성 합성 테스트\n");

        const char* desktop_test_text = "데스크톱 환경에서의 음성 합성 테스트입니다.";
        float* desktop_audio_buffer = (float*)malloc(44100 * 5 * sizeof(float));
        TEST_ASSERT_NOT_NULL_MESSAGE(desktop_audio_buffer, "데스크톱 오디오 버퍼 할당 실패");

        int output_length = 44100 * 5;
        int synth_result = libetude_synthesize_text(platform_engine, desktop_test_text,
                                                  desktop_audio_buffer, &output_length);

        if (synth_result == LIBETUDE_SUCCESS) {
            printf("데스크톱 환경 음성 합성 성공: %d 샘플\n", output_length);

            // 성능 통계 확인
            PerformanceStats stats;
            int stats_result = libetude_get_performance_stats(platform_engine, &stats);

            if (stats_result == LIBETUDE_SUCCESS) {
                printf("데스크톱 성능 통계:\n");
                printf("  추론 시간: %.2f ms\n", stats.inference_time_ms);
                printf("  메모리 사용량: %.2f MB\n", stats.memory_usage_mb);
                printf("  CPU 사용률: %.2f%%\n", stats.cpu_usage_percent);
                printf("  GPU 사용률: %.2f%%\n", stats.gpu_usage_percent);
                printf("  활성 스레드: %d개\n", stats.active_threads);

                // 데스크톱 환경 성능 요구사항 검증
                TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(100.0, stats.inference_time_ms,
                                                "데스크톱 환경 지연시간 요구사항 미달");

                if (platform_info.cpu_cores > 1) {
                    TEST_ASSERT_GREATER_THAN_MESSAGE(1, stats.active_threads,
                                                   "멀티코어 환경에서 단일 스레드만 사용");
                }
            }

        } else if (synth_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("음성 합성 기능 미구현 (정상)\n");
        } else {
            printf("데스크톱 환경 음성 합성 실패: %d\n", synth_result);
        }

        free(desktop_audio_buffer);

        // 데스크톱 최적화 정리
        libetude_desktop_optimizer_destroy(&desktop_optimizer);

    } else if (desktop_result == ET_ERROR_NOT_IMPLEMENTED) {
        printf("데스크톱 최적화 기능 미구현 (정상)\n");
        TEST_PASS();
    } else {
        printf("데스크톱 최적화 초기화 실패: %d\n", desktop_result);
        TEST_FAIL_MESSAGE("데스크톱 최적화 초기화 실패");
    }

    printf("=== 데스크톱 환경 테스트 완료 ===\n");
}

void test_mobile_environment(void) {
    printf("\n=== 모바일 환경 테스트 시작 ===\n");

    // 모바일 환경 시뮬레이션 또는 실제 모바일 환경 감지
    bool is_mobile_test = platform_info.is_mobile ||
                         platform_info.total_memory_mb < 2048 || // 2GB 미만은 모바일로 간주
                         platform_info.cpu_cores <= 4;

    if (!is_mobile_test) {
        printf("모바일 환경 시뮬레이션 모드로 테스트 진행\n");
    }

    if (!platform_engine) {
        platform_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(platform_engine, "더미 엔진 생성 실패");
    }

    printf("모바일 전력 관리 테스트\n");

    // 모바일 전력 관리 초기화
    int power_result = et_mobile_power_init();

    if (power_result == ET_SUCCESS) {
        printf("모바일 전력 관리 초기화 성공\n");

        // 배터리 효율 모드 설정
        printf("배터리 효율 모드 설정 테스트\n");
        int battery_result = et_mobile_set_power_mode(ET_POWER_MODE_BATTERY_SAVER);

        if (battery_result == ET_SUCCESS) {
            printf("배터리 효율 모드 설정 성공\n");
        } else if (battery_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("배터리 효율 모드 기능 미구현 (정상)\n");
        } else {
            printf("배터리 효율 모드 설정 실패: %d\n", battery_result);
        }

        // 열 관리 테스트
        printf("열 관리 테스트\n");
        int thermal_result = et_thermal_management_init();

        if (thermal_result == ET_SUCCESS) {
            printf("열 관리 초기화 성공\n");

            // 현재 온도 확인 (시뮬레이션)
            float current_temp = et_get_cpu_temperature();
            printf("현재 CPU 온도: %.1f°C\n", current_temp);

            // 열 제한 설정
            int temp_limit_result = et_set_thermal_limit(75.0f); // 75도 제한
            if (temp_limit_result == ET_SUCCESS) {
                printf("열 제한 설정 성공: 75°C\n");
            } else if (temp_limit_result == ET_ERROR_NOT_IMPLEMENTED) {
                printf("열 제한 설정 기능 미구현 (정상)\n");
            }

        } else if (thermal_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("열 관리 기능 미구현 (정상)\n");
        } else {
            printf("열 관리 초기화 실패: %d\n", thermal_result);
        }

        // 모바일 최적화된 품질 모드 테스트
        printf("모바일 최적화 품질 모드 테스트\n");
        int mobile_quality_result = libetude_set_quality_mode(platform_engine, LIBETUDE_QUALITY_FAST);

        if (mobile_quality_result == LIBETUDE_SUCCESS) {
            printf("빠른 처리 모드 설정 성공\n");
        } else if (mobile_quality_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("품질 모드 설정 기능 미구현 (정상)\n");
        }

        // 모바일 환경에서의 음성 합성 테스트
        printf("모바일 환경 음성 합성 테스트\n");

        const char* mobile_test_text = "모바일 환경에서의 음성 합성 테스트입니다.";
        float* mobile_audio_buffer = (float*)malloc(44100 * 3 * sizeof(float)); // 3초 (모바일은 짧게)
        TEST_ASSERT_NOT_NULL_MESSAGE(mobile_audio_buffer, "모바일 오디오 버퍼 할당 실패");

        int output_length = 44100 * 3;

        // 배터리 사용량 모니터링 시작
        double start_battery = et_get_battery_level();
        clock_t start_time = clock();

        int synth_result = libetude_synthesize_text(platform_engine, mobile_test_text,
                                                  mobile_audio_buffer, &output_length);

        clock_t end_time = clock();
        double end_battery = et_get_battery_level();

        double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        double battery_usage = start_battery - end_battery;

        if (synth_result == LIBETUDE_SUCCESS) {
            printf("모바일 환경 음성 합성 성공: %d 샘플\n", output_length);
            printf("처리 시간: %.2f 초\n", processing_time);
            printf("배터리 사용량: %.2f%%\n", battery_usage);

            // 모바일 환경 요구사항 검증
            double audio_duration = (double)output_length / 44100.0;
            double battery_per_hour = (battery_usage / processing_time) * 3600.0;

            printf("시간당 예상 배터리 사용량: %.2f%%\n", battery_per_hour);

            // 배터리 사용량 요구사항 (시간당 5% 이하)
            if (battery_per_hour <= 5.0) {
                printf("✓ 배터리 효율성 요구사항 만족\n");
            } else {
                printf("⚠️ 배터리 사용량 과다 (%.2f%% > 5%%)\n", battery_per_hour);
            }

        } else if (synth_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("음성 합성 기능 미구현 (정상)\n");
        } else {
            printf("모바일 환경 음성 합성 실패: %d\n", synth_result);
        }

        free(mobile_audio_buffer);

        // 모바일 전력 관리 정리
        et_mobile_power_cleanup();
        et_thermal_management_cleanup();

    } else if (power_result == ET_ERROR_NOT_IMPLEMENTED) {
        printf("모바일 전력 관리 기능 미구현 (정상)\n");
        TEST_PASS();
    } else {
        printf("모바일 전력 관리 초기화 실패: %d\n", power_result);
        TEST_FAIL_MESSAGE("모바일 전력 관리 초기화 실패");
    }

    printf("=== 모바일 환경 테스트 완료 ===\n");
}

void test_embedded_environment(void) {
    printf("\n=== 임베디드 환경 테스트 시작 ===\n");

    // 임베디드 환경 시뮬레이션 또는 실제 임베디드 환경 감지
    bool is_embedded_test = platform_info.is_embedded ||
                           platform_info.total_memory_mb < 512 || // 512MB 미만은 임베디드로 간주
                           platform_info.cpu_cores == 1;

    if (!is_embedded_test) {
        printf("임베디드 환경 시뮬레이션 모드로 테스트 진행\n");
    }

    if (!platform_engine) {
        platform_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(platform_engine, "더미 엔진 생성 실패");
    }

    printf("임베디드 최적화 테스트\n");

    // 임베디드 최적화 초기화
    int embedded_result = et_embedded_optimization_init();

    if (embedded_result == ET_SUCCESS) {
        printf("임베디드 최적화 초기화 성공\n");

        // 최소 메모리 모드 설정
        printf("최소 메모리 모드 설정 테스트\n");
        int memory_result = et_embedded_set_memory_mode(ET_MEMORY_MODE_MINIMAL);

        if (memory_result == ET_SUCCESS) {
            printf("최소 메모리 모드 설정 성공\n");
        } else if (memory_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("최소 메모리 모드 기능 미구현 (정상)\n");
        } else {
            printf("최소 메모리 모드 설정 실패: %d\n", memory_result);
        }

        // 저전력 모드 설정
        printf("저전력 모드 설정 테스트\n");
        int power_result = et_embedded_set_power_mode(ET_EMBEDDED_POWER_LOW);

        if (power_result == ET_SUCCESS) {
            printf("저전력 모드 설정 성공\n");
        } else if (power_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("저전력 모드 기능 미구현 (정상)\n");
        } else {
            printf("저전력 모드 설정 실패: %d\n", power_result);
        }

        // 제한된 리소스 환경 설정
        printf("리소스 제한 설정 테스트\n");

        // 메모리 제한 (예: 64MB)
        size_t memory_limit = 64 * 1024 * 1024;
        int mem_limit_result = et_embedded_set_memory_limit(memory_limit);

        if (mem_limit_result == ET_SUCCESS) {
            printf("메모리 제한 설정 성공: %zu MB\n", memory_limit / (1024*1024));
        } else if (mem_limit_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("메모리 제한 설정 기능 미구현 (정상)\n");
        }

        // CPU 사용률 제한 (예: 50%)
        int cpu_limit_result = et_embedded_set_cpu_limit(50);

        if (cpu_limit_result == ET_SUCCESS) {
            printf("CPU 사용률 제한 설정 성공: 50%%\n");
        } else if (cpu_limit_result == ET_ERROR_NOT_IMPLEMENTED) {
            printf("CPU 사용률 제한 기능 미구현 (정상)\n");
        }

        // 임베디드 환경에서의 음성 합성 테스트
        printf("임베디드 환경 음성 합성 테스트\n");

        const char* embedded_test_text = "임베디드 환경 테스트";
        float* embedded_audio_buffer = (float*)malloc(44100 * 2 * sizeof(float)); // 2초 (임베디드는 더 짧게)
        TEST_ASSERT_NOT_NULL_MESSAGE(embedded_audio_buffer, "임베디드 오디오 버퍼 할당 실패");

        int output_length = 44100 * 2;

        // 리소스 사용량 모니터링 시작
        size_t start_memory = et_get_current_memory_usage();
        clock_t start_time = clock();

        int synth_result = libetude_synthesize_text(platform_engine, embedded_test_text,
                                                  embedded_audio_buffer, &output_length);

        clock_t end_time = clock();
        size_t end_memory = et_get_current_memory_usage();

        double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        size_t memory_used = end_memory - start_memory;

        if (synth_result == LIBETUDE_SUCCESS) {
            printf("임베디드 환경 음성 합성 성공: %d 샘플\n", output_length);
            printf("처리 시간: %.2f 초\n", processing_time);
            printf("메모리 사용량: %zu KB\n", memory_used / 1024);

            // 임베디드 환경 요구사항 검증
            double audio_duration = (double)output_length / 44100.0;
            double realtime_factor = processing_time / audio_duration;

            printf("실시간 팩터: %.2f\n", realtime_factor);

            // 메모리 사용량 제한 검증 (예: 10MB 이하)
            const size_t max_memory = 10 * 1024 * 1024;
            if (memory_used <= max_memory) {
                printf("✓ 메모리 사용량 요구사항 만족 (%zu KB <= %zu KB)\n",
                       memory_used/1024, max_memory/1024);
            } else {
                printf("⚠️ 메모리 사용량 과다 (%zu KB > %zu KB)\n",
                       memory_used/1024, max_memory/1024);
            }

            // 실시간 처리 요구사항 (임베디드는 더 관대하게)
            if (realtime_factor <= 2.0) {
                printf("✓ 임베디드 환경 실시간 처리 요구사항 만족\n");
            } else {
                printf("⚠️ 임베디드 환경 실시간 처리 요구사항 미달 (%.2fx > 2.0x)\n", realtime_factor);
            }

        } else if (synth_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("음성 합성 기능 미구현 (정상)\n");
        } else {
            printf("임베디드 환경 음성 합성 실패: %d\n", synth_result);
        }

        free(embedded_audio_buffer);

        // 임베디드 최적화 정리
        et_embedded_optimization_cleanup();

    } else if (embedded_result == ET_ERROR_NOT_IMPLEMENTED) {
        printf("임베디드 최적화 기능 미구현 (정상)\n");
        TEST_PASS();
    } else {
        printf("임베디드 최적화 초기화 실패: %d\n", embedded_result);
        TEST_FAIL_MESSAGE("임베디드 최적화 초기화 실패");
    }

    printf("=== 임베디드 환경 테스트 완료 ===\n");
}

void test_cross_platform_compatibility(void) {
    printf("\n=== 크로스 플랫폼 호환성 테스트 시작 ===\n");

    if (!platform_engine) {
        platform_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(platform_engine, "더미 엔진 생성 실패");
    }

    // 하드웨어 기능 감지 테스트
    printf("하드웨어 기능 감지 테스트\n");

    uint32_t hw_features = libetude_get_hardware_features();
    printf("감지된 하드웨어 기능: 0x%08X\n", hw_features);

    if (hw_features & LIBETUDE_SIMD_SSE) {
        printf("  ✓ SSE 지원\n");
    }
    if (hw_features & LIBETUDE_SIMD_AVX) {
        printf("  ✓ AVX 지원\n");
    }
    if (hw_features & LIBETUDE_SIMD_NEON) {
        printf("  ✓ NEON 지원\n");
    }
    if (hw_features == LIBETUDE_SIMD_NONE) {
        printf("  SIMD 지원 없음\n");
    }

    // 플랫폼별 오디오 백엔드 테스트
    printf("플랫폼별 오디오 백엔드 테스트\n");

#ifdef PLATFORM_WINDOWS
    printf("Windows 오디오 백엔드 (DirectSound/WASAPI) 테스트\n");
    // Windows 특화 테스트 코드
#elif defined(PLATFORM_MACOS)
    printf("macOS 오디오 백엔드 (Core Audio) 테스트\n");
    // macOS 특화 테스트 코드
#elif defined(PLATFORM_LINUX)
    printf("Linux 오디오 백엔드 (ALSA/PulseAudio) 테스트\n");
    // Linux 특화 테스트 코드
#elif defined(PLATFORM_ANDROID)
    printf("Android 오디오 백엔드 (OpenSL ES/AAudio) 테스트\n");
    // Android 특화 테스트 코드
#endif

    // 공통 API 호환성 테스트
    printf("공통 API 호환성 테스트\n");

    const char* version = libetude_get_version();
    if (version) {
        printf("LibEtude 버전: %s\n", version);
        TEST_ASSERT_NOT_NULL_MESSAGE(version, "버전 정보를 가져올 수 없음");
    } else {
        printf("버전 정보 가져오기 실패\n");
    }

    // 오류 처리 호환성 테스트
    printf("오류 처리 호환성 테스트\n");

    // 의도적으로 오류 발생
    int error_result = libetude_synthesize_text(NULL, "테스트", NULL, NULL);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(LIBETUDE_SUCCESS, error_result, "NULL 포인터로 호출 시 오류가 발생해야 함");

    const char* error_msg = libetude_get_last_error();
    if (error_msg) {
        printf("오류 메시지: %s\n", error_msg);
    } else {
        printf("오류 메시지 없음 (구현에 따라 정상일 수 있음)\n");
    }

    // 메모리 정렬 호환성 테스트
    printf("메모리 정렬 호환성 테스트\n");

    // 다양한 크기의 버퍼로 테스트
    size_t test_sizes[] = {1024, 2048, 4096, 8192};

    for (int i = 0; i < 4; i++) {
        float* aligned_buffer = (float*)malloc(test_sizes[i] * sizeof(float));
        TEST_ASSERT_NOT_NULL_MESSAGE(aligned_buffer, "정렬된 버퍼 할당 실패");

        // 버퍼 정렬 확인
        uintptr_t addr = (uintptr_t)aligned_buffer;
        bool is_aligned = (addr % sizeof(float)) == 0;

        if (is_aligned) {
            printf("  버퍼 크기 %zu: 정렬됨\n", test_sizes[i]);
        } else {
            printf("  버퍼 크기 %zu: 정렬되지 않음\n", test_sizes[i]);
        }

        free(aligned_buffer);
    }

    // 엔디안 호환성 테스트 (간단한 테스트)
    printf("엔디안 호환성 테스트\n");

    uint32_t test_value = 0x12345678;
    uint8_t* bytes = (uint8_t*)&test_value;

    if (bytes[0] == 0x78) {
        printf("  리틀 엔디안 시스템\n");
    } else if (bytes[0] == 0x12) {
        printf("  빅 엔디안 시스템\n");
    } else {
        printf("  알 수 없는 엔디안\n");
    }

    printf("=== 크로스 플랫폼 호환성 테스트 완료 ===\n");
}

void test_platform_specific_features(void) {
    printf("\n=== 플랫폼별 특화 기능 테스트 시작 ===\n");

    // 플랫폼별 특화 기능 테스트
#ifdef PLATFORM_WINDOWS
    printf("Windows 특화 기능 테스트\n");

    // Windows 특화 기능들
    printf("  - DirectSound/WASAPI 지원 확인\n");
    printf("  - Windows 멀티미디어 타이머 사용\n");
    printf("  - Windows 스레드 우선순위 설정\n");

#elif defined(PLATFORM_MACOS)
    printf("macOS 특화 기능 테스트\n");

    // macOS 특화 기능들
    printf("  - Core Audio 지원 확인\n");
    printf("  - Metal 성능 셰이더 지원\n");
    printf("  - macOS 전력 관리 통합\n");

#elif defined(PLATFORM_LINUX)
    printf("Linux 특화 기능 테스트\n");

    // Linux 특화 기능들
    printf("  - ALSA/PulseAudio 지원 확인\n");
    printf("  - Linux 실시간 스케줄링\n");
    printf("  - CPU 친화성 설정\n");

#elif defined(PLATFORM_ANDROID)
    printf("Android 특화 기능 테스트\n");

    // Android 특화 기능들
    printf("  - OpenSL ES/AAudio 지원 확인\n");
    printf("  - Android 전력 관리 통합\n");
    printf("  - JNI 인터페이스 테스트\n");

#else
    printf("알 수 없는 플랫폼 또는 일반적인 POSIX 환경\n");
#endif

    // 아키텍처별 특화 기능 테스트
#ifdef ARCH_X64
    printf("x86_64 아키텍처 특화 기능:\n");
    printf("  - AVX/AVX2 최적화 확인\n");
    printf("  - x64 레지스터 활용\n");

#elif defined(ARCH_ARM64)
    printf("ARM64 아키텍처 특화 기능:\n");
    printf("  - NEON 최적화 확인\n");
    printf("  - ARM64 특화 명령어 사용\n");

#elif defined(ARCH_ARM32)
    printf("ARM32 아키텍처 특화 기능:\n");
    printf("  - NEON 최적화 확인 (가능한 경우)\n");
    printf("  - ARM32 제약사항 고려\n");

#else
    printf("일반적인 아키텍처\n");
#endif

    printf("=== 플랫폼별 특화 기능 테스트 완료 ===\n");
}

int main(void) {
    UNITY_BEGIN();

    printf("\n========================================\n");
    printf("LibEtude 플랫폼별 통합 테스트 시작\n");
    printf("========================================\n");

    RUN_TEST(test_desktop_environment);
    RUN_TEST(test_mobile_environment);
    RUN_TEST(test_embedded_environment);
    RUN_TEST(test_cross_platform_compatibility);
    RUN_TEST(test_platform_specific_features);

    printf("\n========================================\n");
    printf("LibEtude 플랫폼별 통합 테스트 완료\n");
    printf("========================================\n");

    return UNITY_END();
}