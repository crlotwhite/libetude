/**
 * @file optimization.c
 * @brief 플랫폼별 컴파일 타임 최적화 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/optimization.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// 컴파일 정보 구조체 정의
// ============================================================================

/**
 * @brief 컴파일 타임 설정 정보를 담는 구조체
 */
typedef struct ETCompilationInfo {
    // 플랫폼 정보
    ETPlatformType platform;
    ETArchitecture architecture;

    // 컴파일러 정보
    char compiler_name[64];
    int compiler_version;

    // 최적화 설정
    bool simd_enabled;
    bool debug_enabled;
    bool profiling_enabled;
    bool minimal_mode;

    // SIMD 기능 지원
    bool sse_support;
    bool sse2_support;
    bool avx_support;
    bool avx2_support;
    bool neon_support;

    // 컴파일 타임 상수
    int default_num_threads;
    int default_memory_pool_size_mb;
    int default_audio_buffer_size;
    int max_concurrent_streams;

    // 빌드 정보
    char build_date[32];
    char build_time[32];
    char version_string[32];
} ETCompilationInfo;

// 전역 컴파일 정보 (컴파일 타임에 초기화됨)
static ETCompilationInfo g_compilation_info = {0};
static bool g_optimization_initialized = false;

// ============================================================================
// 내부 함수들
// ============================================================================

/**
 * @brief 컴파일러 정보를 초기화합니다
 */
static void initialize_compiler_info(ETCompilationInfo* info) {
    // 컴파일러 이름과 버전 설정
#ifdef LIBETUDE_COMPILER_MSVC
    strncpy(info->compiler_name, "MSVC", sizeof(info->compiler_name) - 1);
    info->compiler_version = LIBETUDE_COMPILER_VERSION;
#elif defined(LIBETUDE_COMPILER_GCC)
    strncpy(info->compiler_name, "GCC", sizeof(info->compiler_name) - 1);
    info->compiler_version = LIBETUDE_COMPILER_VERSION;
#elif defined(LIBETUDE_COMPILER_CLANG)
    strncpy(info->compiler_name, "Clang", sizeof(info->compiler_name) - 1);
    info->compiler_version = LIBETUDE_COMPILER_VERSION;
#else
    strncpy(info->compiler_name, "Unknown", sizeof(info->compiler_name) - 1);
    info->compiler_version = 0;
#endif
    info->compiler_name[sizeof(info->compiler_name) - 1] = '\0';
}

/**
 * @brief 플랫폼 정보를 초기화합니다
 */
static void initialize_platform_info(ETCompilationInfo* info) {
    // 플랫폼 타입 설정
#ifdef LIBETUDE_PLATFORM_WINDOWS
    info->platform = ET_PLATFORM_WINDOWS;
#elif defined(LIBETUDE_PLATFORM_LINUX)
    info->platform = ET_PLATFORM_LINUX;
#elif defined(LIBETUDE_PLATFORM_MACOS)
    info->platform = ET_PLATFORM_MACOS;
#elif defined(LIBETUDE_PLATFORM_ANDROID)
    info->platform = ET_PLATFORM_ANDROID;
#elif defined(LIBETUDE_PLATFORM_IOS)
    info->platform = ET_PLATFORM_IOS;
#else
    info->platform = ET_PLATFORM_UNKNOWN;
#endif

    // 아키텍처 설정
#if defined(_M_X64) || defined(__x86_64__)
    info->architecture = ET_ARCH_X64;
#elif defined(_M_IX86) || defined(__i386__)
    info->architecture = ET_ARCH_X86;
#elif defined(_M_ARM64) || defined(__aarch64__)
    info->architecture = ET_ARCH_ARM64;
#elif defined(_M_ARM) || defined(__arm__)
    info->architecture = ET_ARCH_ARM;
#else
    info->architecture = ET_ARCH_UNKNOWN;
#endif
}

/**
 * @brief 최적화 설정을 초기화합니다
 */
static void initialize_optimization_settings(ETCompilationInfo* info) {
    // SIMD 지원 설정
#ifdef LIBETUDE_SIMD_ENABLED
    info->simd_enabled = true;
#else
    info->simd_enabled = false;
#endif

    // 디버그 모드 설정
#if LIBETUDE_DEBUG
    info->debug_enabled = true;
#else
    info->debug_enabled = false;
#endif

    // 프로파일링 설정
#if ET_PROFILE_ENABLED
    info->profiling_enabled = true;
#else
    info->profiling_enabled = false;
#endif

    // 최소 모드 설정
#ifdef LIBETUDE_MINIMAL
    info->minimal_mode = true;
#else
    info->minimal_mode = false;
#endif
}

/**
 * @brief SIMD 기능 지원을 초기화합니다
 */
static void initialize_simd_support(ETCompilationInfo* info) {
    // SSE 지원
#ifdef LIBETUDE_HAVE_SSE
    info->sse_support = true;
#else
    info->sse_support = false;
#endif

    // SSE2 지원
#ifdef LIBETUDE_HAVE_SSE2
    info->sse2_support = true;
#else
    info->sse2_support = false;
#endif

    // AVX 지원
#ifdef LIBETUDE_HAVE_AVX
    info->avx_support = true;
#else
    info->avx_support = false;
#endif

    // AVX2 지원
#ifdef LIBETUDE_HAVE_AVX2
    info->avx2_support = true;
#else
    info->avx2_support = false;
#endif

    // NEON 지원
#ifdef LIBETUDE_HAVE_NEON
    info->neon_support = true;
#else
    info->neon_support = false;
#endif
}

/**
 * @brief 컴파일 타임 상수를 초기화합니다
 */
static void initialize_compile_time_constants(ETCompilationInfo* info) {
    info->default_num_threads = LIBETUDE_DEFAULT_NUM_THREADS;
    info->default_memory_pool_size_mb = LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB;
    info->default_audio_buffer_size = LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE;
    info->max_concurrent_streams = LIBETUDE_MAX_CONCURRENT_STREAMS;
}

/**
 * @brief 빌드 정보를 초기화합니다
 */
static void initialize_build_info(ETCompilationInfo* info) {
    // 빌드 날짜와 시간 (컴파일 타임에 설정됨)
    strncpy(info->build_date, __DATE__, sizeof(info->build_date) - 1);
    info->build_date[sizeof(info->build_date) - 1] = '\0';

    strncpy(info->build_time, __TIME__, sizeof(info->build_time) - 1);
    info->build_time[sizeof(info->build_time) - 1] = '\0';

    // 버전 문자열
    strncpy(info->version_string, LIBETUDE_VERSION_STRING, sizeof(info->version_string) - 1);
    info->version_string[sizeof(info->version_string) - 1] = '\0';
}

// ============================================================================
// 공개 함수 구현
// ============================================================================

ETResult et_optimization_initialize(void) {
    if (g_optimization_initialized) {
        return ET_SUCCESS;
    }

    // 컴파일 정보 초기화
    memset(&g_compilation_info, 0, sizeof(g_compilation_info));

    initialize_compiler_info(&g_compilation_info);
    initialize_platform_info(&g_compilation_info);
    initialize_optimization_settings(&g_compilation_info);
    initialize_simd_support(&g_compilation_info);
    initialize_compile_time_constants(&g_compilation_info);
    initialize_build_info(&g_compilation_info);

    g_optimization_initialized = true;

    ET_IF_DEBUG({
        printf("[DEBUG] 컴파일 타임 최적화 시스템 초기화 완료\n");
        printf("  - 플랫폼: %d, 아키텍처: %d\n",
               g_compilation_info.platform, g_compilation_info.architecture);
        printf("  - 컴파일러: %s (버전: %d)\n",
               g_compilation_info.compiler_name, g_compilation_info.compiler_version);
        printf("  - SIMD 활성화: %s\n", g_compilation_info.simd_enabled ? "예" : "아니오");
        printf("  - 디버그 모드: %s\n", g_compilation_info.debug_enabled ? "예" : "아니오");
    });

    return ET_SUCCESS;
}

ETResult et_get_compilation_info(struct ETCompilationInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_optimization_initialized) {
        ETResult result = et_optimization_initialize();
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 컴파일 정보 복사
    memcpy(info, &g_compilation_info, sizeof(ETCompilationInfo));

    return ET_SUCCESS;
}

void et_optimization_finalize(void) {
    if (!g_optimization_initialized) {
        return;
    }

    ET_IF_DEBUG({
        printf("[DEBUG] 컴파일 타임 최적화 시스템 정리 완료\n");
    });

    g_optimization_initialized = false;
}

// ============================================================================
// 컴파일 타임 최적화 유틸리티 함수들
// ============================================================================

/**
 * @brief 현재 플랫폼에서 사용 가능한 최적화 기능을 확인합니다
 * @return 사용 가능한 기능 플래그
 */
uint32_t et_get_available_optimizations(void) {
    uint32_t features = 0;

    ET_IF_SSE(features |= ET_FEATURE_SSE);
    ET_IF_SSE2(features |= ET_FEATURE_SSE2);
    ET_IF_AVX(features |= ET_FEATURE_AVX);
    ET_IF_AVX2(features |= ET_FEATURE_AVX2);
    ET_IF_NEON(features |= ET_FEATURE_NEON);

    return features;
}

/**
 * @brief 컴파일 타임 설정을 문자열로 출력합니다
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_compilation_info_string(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_optimization_initialized) {
        ETResult result = et_optimization_initialize();
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    int written = snprintf(buffer, buffer_size,
        "LibEtude 컴파일 정보:\n"
        "  버전: %s\n"
        "  빌드 날짜: %s %s\n"
        "  플랫폼: %d (%s)\n"
        "  아키텍처: %d\n"
        "  컴파일러: %s (버전: %d)\n"
        "  SIMD 지원: %s\n"
        "  디버그 모드: %s\n"
        "  프로파일링: %s\n"
        "  최소 모드: %s\n"
        "  지원 SIMD: SSE=%s, SSE2=%s, AVX=%s, AVX2=%s, NEON=%s\n",
        g_compilation_info.version_string,
        g_compilation_info.build_date,
        g_compilation_info.build_time,
        g_compilation_info.platform,
        (g_compilation_info.platform == ET_PLATFORM_WINDOWS) ? "Windows" :
        (g_compilation_info.platform == ET_PLATFORM_LINUX) ? "Linux" :
        (g_compilation_info.platform == ET_PLATFORM_MACOS) ? "macOS" : "Unknown",
        g_compilation_info.architecture,
        g_compilation_info.compiler_name,
        g_compilation_info.compiler_version,
        g_compilation_info.simd_enabled ? "예" : "아니오",
        g_compilation_info.debug_enabled ? "예" : "아니오",
        g_compilation_info.profiling_enabled ? "예" : "아니오",
        g_compilation_info.minimal_mode ? "예" : "아니오",
        g_compilation_info.sse_support ? "예" : "아니오",
        g_compilation_info.sse2_support ? "예" : "아니오",
        g_compilation_info.avx_support ? "예" : "아니오",
        g_compilation_info.avx2_support ? "예" : "아니오",
        g_compilation_info.neon_support ? "예" : "아니오"
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 성능 측정 함수들 (디버그 모드에서만 활성화)
// ============================================================================

#if ET_PROFILE_ENABLED

static uint64_t g_profile_start_times[256];
static char g_profile_names[256][64];
static int g_profile_depth = 0;

void et_profile_begin(const char* name) {
    if (g_profile_depth >= 256) {
        return;
    }

    strncpy(g_profile_names[g_profile_depth], name, sizeof(g_profile_names[0]) - 1);
    g_profile_names[g_profile_depth][sizeof(g_profile_names[0]) - 1] = '\0';

    // 고해상도 타이머 사용 (플랫폼별 구현 필요)
    g_profile_start_times[g_profile_depth] = 0; // TODO: 실제 타이머 구현

    g_profile_depth++;
}

void et_profile_end(const char* name) {
    if (g_profile_depth <= 0) {
        return;
    }

    g_profile_depth--;

    uint64_t end_time = 0; // TODO: 실제 타이머 구현
    uint64_t elapsed = end_time - g_profile_start_times[g_profile_depth];

    printf("[PROFILE] %s: %llu ns\n", name, (unsigned long long)elapsed);
}

#endif // ET_PROFILE_ENABLED