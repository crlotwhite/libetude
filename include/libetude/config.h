/**
 * @file config.h
 * @brief LibEtude 설정 및 컴파일 옵션
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude의 컴파일 시간 설정과 플랫폼별 정의를 포함합니다.
 */

#ifndef LIBETUDE_CONFIG_H
#define LIBETUDE_CONFIG_H

// ============================================================================
// 버전 정보
// ============================================================================

#define LIBETUDE_VERSION_MAJOR 1
#define LIBETUDE_VERSION_MINOR 0
#define LIBETUDE_VERSION_PATCH 0
#define LIBETUDE_VERSION_STRING "1.0.0"

// ============================================================================
// 플랫폼 감지
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #ifndef LIBETUDE_PLATFORM_WINDOWS
        #define LIBETUDE_PLATFORM_WINDOWS
    #endif
    #if defined(_WIN64)
        #ifndef LIBETUDE_PLATFORM_WIN64
            #define LIBETUDE_PLATFORM_WIN64
        #endif
    #else
        #ifndef LIBETUDE_PLATFORM_WIN32
            #define LIBETUDE_PLATFORM_WIN32
        #endif
    #endif
#elif defined(__APPLE__)
    #ifndef LIBETUDE_PLATFORM_MACOS
        #define LIBETUDE_PLATFORM_MACOS
    #endif
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #ifndef LIBETUDE_PLATFORM_IOS
            #define LIBETUDE_PLATFORM_IOS
        #endif
    #endif
#elif defined(__ANDROID__)
    #ifndef LIBETUDE_PLATFORM_ANDROID
        #define LIBETUDE_PLATFORM_ANDROID
    #endif
    #ifndef LIBETUDE_PLATFORM_LINUX
        #define LIBETUDE_PLATFORM_LINUX
    #endif
#elif defined(__linux__)
    #ifndef LIBETUDE_PLATFORM_LINUX
        #define LIBETUDE_PLATFORM_LINUX
    #endif
#else
    #define LIBETUDE_PLATFORM_UNKNOWN
#endif

// ============================================================================
// 컴파일러 감지
// ============================================================================

#if defined(_MSC_VER)
    #define LIBETUDE_COMPILER_MSVC
    #define LIBETUDE_COMPILER_VERSION _MSC_VER
#elif defined(__GNUC__)
    #define LIBETUDE_COMPILER_GCC
    #define LIBETUDE_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__clang__)
    #define LIBETUDE_COMPILER_CLANG
    #define LIBETUDE_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#endif

// ============================================================================
// API 내보내기/가져오기 매크로
// ============================================================================

#ifdef LIBETUDE_PLATFORM_WINDOWS
    #ifdef LIBETUDE_STATIC_LIB
        #define LIBETUDE_API
    #elif defined(LIBETUDE_EXPORTS)
        #define LIBETUDE_API __declspec(dllexport)
    #else
        #define LIBETUDE_API __declspec(dllimport)
    #endif
    #define LIBETUDE_CALL __cdecl
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define LIBETUDE_API __attribute__((visibility("default")))
    #else
        #define LIBETUDE_API
    #endif
    #define LIBETUDE_CALL
#endif

// ============================================================================
// 인라인 함수 매크로
// ============================================================================

#ifdef LIBETUDE_COMPILER_MSVC
    #define LIBETUDE_INLINE __forceinline
    #define LIBETUDE_NOINLINE __declspec(noinline)
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define LIBETUDE_INLINE __attribute__((always_inline)) inline
    #define LIBETUDE_NOINLINE __attribute__((noinline))
#else
    #define LIBETUDE_INLINE inline
    #define LIBETUDE_NOINLINE
#endif

// ============================================================================
// 메모리 정렬 매크로
// ============================================================================

#ifdef LIBETUDE_COMPILER_MSVC
    #define LIBETUDE_ALIGN(n) __declspec(align(n))
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define LIBETUDE_ALIGN(n) __attribute__((aligned(n)))
#else
    #define LIBETUDE_ALIGN(n)
#endif

// ============================================================================
// 최적화 힌트 매크로
// ============================================================================

#ifdef LIBETUDE_COMPILER_MSVC
    #define LIBETUDE_LIKELY(x) (x)
    #define LIBETUDE_UNLIKELY(x) (x)
    #define LIBETUDE_RESTRICT __restrict
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define LIBETUDE_LIKELY(x) __builtin_expect(!!(x), 1)
    #define LIBETUDE_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define LIBETUDE_RESTRICT __restrict__
#else
    #define LIBETUDE_LIKELY(x) (x)
    #define LIBETUDE_UNLIKELY(x) (x)
    #define LIBETUDE_RESTRICT
#endif

// ============================================================================
// 디버그 매크로
// ============================================================================

#ifdef NDEBUG
    #define LIBETUDE_DEBUG 0
    #define LIBETUDE_ASSERT(x) ((void)0)
#else
    #define LIBETUDE_DEBUG 1
    #include <assert.h>
    #define LIBETUDE_ASSERT(x) assert(x)
#endif

// ============================================================================
// 기능 감지 매크로
// ============================================================================

// SIMD 지원 감지
#if defined(__SSE__)
    #define LIBETUDE_HAVE_SSE 1
#endif

#if defined(__SSE2__)
    #define LIBETUDE_HAVE_SSE2 1
#endif

#if defined(__SSE3__)
    #define LIBETUDE_HAVE_SSE3 1
#endif

#if defined(__SSSE3__)
    #define LIBETUDE_HAVE_SSSE3 1
#endif

#if defined(__SSE4_1__)
    #define LIBETUDE_HAVE_SSE4_1 1
#endif

#if defined(__SSE4_2__)
    #define LIBETUDE_HAVE_SSE4_2 1
#endif

#if defined(__AVX__)
    #define LIBETUDE_HAVE_AVX 1
#endif

#if defined(__AVX2__)
    #define LIBETUDE_HAVE_AVX2 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define LIBETUDE_HAVE_NEON 1
#endif

// OpenMP 지원 감지
#ifdef _OPENMP
    #define LIBETUDE_HAVE_OPENMP 1
#endif

// ============================================================================
// 기본 설정값
// ============================================================================

/** 기본 스레드 수 (0 = 자동 감지) */
#ifndef LIBETUDE_DEFAULT_NUM_THREADS
    #define LIBETUDE_DEFAULT_NUM_THREADS 0
#endif

/** 기본 메모리 풀 크기 (MB) */
#ifndef LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB
    #define LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB 256
#endif

/** 기본 오디오 버퍼 크기 (샘플) */
#ifndef LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE
    #define LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE 1024
#endif

/** 최대 동시 스트림 수 */
#ifndef LIBETUDE_MAX_CONCURRENT_STREAMS
    #define LIBETUDE_MAX_CONCURRENT_STREAMS 8
#endif

/** 최대 텍스트 길이 */
#ifndef LIBETUDE_MAX_TEXT_LENGTH
    #define LIBETUDE_MAX_TEXT_LENGTH 4096
#endif

// ============================================================================
// 성능 튜닝 옵션
// ============================================================================

/** 벡터화 최적화 활성화 */
#ifndef LIBETUDE_ENABLE_VECTORIZATION
    #define LIBETUDE_ENABLE_VECTORIZATION 1
#endif

/** 병렬 처리 활성화 */
#ifndef LIBETUDE_ENABLE_PARALLEL_PROCESSING
    #define LIBETUDE_ENABLE_PARALLEL_PROCESSING 1
#endif

/** 메모리 풀 사용 */
#ifndef LIBETUDE_USE_MEMORY_POOL
    #define LIBETUDE_USE_MEMORY_POOL 1
#endif

/** 인플레이스 연산 사용 */
#ifndef LIBETUDE_USE_INPLACE_OPERATIONS
    #define LIBETUDE_USE_INPLACE_OPERATIONS 1
#endif

// ============================================================================
// 임베디드 시스템 최적화
// ============================================================================

#ifdef LIBETUDE_MINIMAL
    /** 최소 메모리 모드 */
    #define LIBETUDE_MINIMAL_MEMORY_MODE 1

    /** 기본 메모리 풀 크기를 줄임 */
    #undef LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB
    #define LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB 64

    /** 최대 동시 스트림 수를 줄임 */
    #undef LIBETUDE_MAX_CONCURRENT_STREAMS
    #define LIBETUDE_MAX_CONCURRENT_STREAMS 2

    /** 일부 고급 기능 비활성화 */
    #define LIBETUDE_DISABLE_GPU_ACCELERATION 1
    #define LIBETUDE_DISABLE_PROFILING 1
#endif

// ============================================================================
// 유틸리티 매크로
// ============================================================================

/** 배열 크기 계산 */
#define LIBETUDE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/** 최솟값 계산 */
#define LIBETUDE_MIN(a, b) ((a) < (b) ? (a) : (b))

/** 최댓값 계산 */
#define LIBETUDE_MAX(a, b) ((a) > (b) ? (a) : (b))

/** 값을 범위로 제한 */
#define LIBETUDE_CLAMP(x, min, max) LIBETUDE_MAX(min, LIBETUDE_MIN(max, x))

/** 포인터 유효성 검사 */
#define LIBETUDE_CHECK_PTR(ptr) \
    do { \
        if (LIBETUDE_UNLIKELY((ptr) == NULL)) { \
            return LIBETUDE_ERROR_INVALID_ARGUMENT; \
        } \
    } while(0)

/** 오류 코드 검사 */
#define LIBETUDE_CHECK_ERROR(expr) \
    do { \
        int _err = (expr); \
        if (LIBETUDE_UNLIKELY(_err != LIBETUDE_SUCCESS)) { \
            return _err; \
        } \
    } while(0)

#endif // LIBETUDE_CONFIG_H