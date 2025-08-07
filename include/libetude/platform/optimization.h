/**
 * @file optimization.h
 * @brief 플랫폼별 컴파일 타임 최적화 시스템
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 컴파일 타임 선택 매크로, 인라인 함수 최적화,
 * 조건부 컴파일, 컴파일러별 최적화 힌트를 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_OPTIMIZATION_H
#define LIBETUDE_PLATFORM_OPTIMIZATION_H

#include "libetude/config.h"
#include "libetude/platform/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 플랫폼별 컴파일 타임 선택 매크로
// ============================================================================

/**
 * @brief 플랫폼별 구현 선택 매크로
 *
 * 컴파일 타임에 플랫폼에 맞는 구현을 선택하여
 * 런타임 오버헤드를 제거합니다.
 */

// 오디오 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_AUDIO_IMPL_HEADER "libetude/platform/windows/windows_audio.h"
    #define ET_AUDIO_IMPL_PREFIX windows_audio
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #define ET_AUDIO_IMPL_HEADER "libetude/platform/linux/linux_audio.h"
    #define ET_AUDIO_IMPL_PREFIX linux_audio
#elif defined(LIBETUDE_PLATFORM_MACOS)
    #define ET_AUDIO_IMPL_HEADER "libetude/platform/macos/macos_audio.h"
    #define ET_AUDIO_IMPL_PREFIX macos_audio
#else
    #define ET_AUDIO_IMPL_HEADER "libetude/platform/generic/generic_audio.h"
    #define ET_AUDIO_IMPL_PREFIX generic_audio
#endif

// 시스템 정보 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_SYSTEM_IMPL_HEADER "libetude/platform/windows/windows_system.h"
    #define ET_SYSTEM_IMPL_PREFIX windows_system
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #define ET_SYSTEM_IMPL_HEADER "libetude/platform/linux/linux_system.h"
    #define ET_SYSTEM_IMPL_PREFIX linux_system
#elif defined(LIBETUDE_PLATFORM_MACOS)
    #define ET_SYSTEM_IMPL_HEADER "libetude/platform/macos/macos_system.h"
    #define ET_SYSTEM_IMPL_PREFIX macos_system
#else
    #define ET_SYSTEM_IMPL_HEADER "libetude/platform/generic/generic_system.h"
    #define ET_SYSTEM_IMPL_PREFIX generic_system
#endif

// 스레딩 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_THREAD_IMPL_HEADER "libetude/platform/windows/windows_threading.h"
    #define ET_THREAD_IMPL_PREFIX windows_threading
#else
    #define ET_THREAD_IMPL_HEADER "libetude/platform/posix/posix_threading.h"
    #define ET_THREAD_IMPL_PREFIX posix_threading
#endif

// 메모리 관리 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_MEMORY_IMPL_HEADER "libetude/platform/windows/windows_memory.h"
    #define ET_MEMORY_IMPL_PREFIX windows_memory
#else
    #define ET_MEMORY_IMPL_HEADER "libetude/platform/posix/posix_memory.h"
    #define ET_MEMORY_IMPL_PREFIX posix_memory
#endif

// 파일시스템 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_FILESYSTEM_IMPL_HEADER "libetude/platform/windows/windows_filesystem.h"
    #define ET_FILESYSTEM_IMPL_PREFIX windows_filesystem
#else
    #define ET_FILESYSTEM_IMPL_HEADER "libetude/platform/posix/posix_filesystem.h"
    #define ET_FILESYSTEM_IMPL_PREFIX posix_filesystem
#endif

// 네트워크 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_NETWORK_IMPL_HEADER "libetude/platform/windows/windows_network.h"
    #define ET_NETWORK_IMPL_PREFIX windows_network
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #define ET_NETWORK_IMPL_HEADER "libetude/platform/linux/linux_network.h"
    #define ET_NETWORK_IMPL_PREFIX linux_network
#elif defined(LIBETUDE_PLATFORM_MACOS)
    #define ET_NETWORK_IMPL_HEADER "libetude/platform/macos/macos_network.h"
    #define ET_NETWORK_IMPL_PREFIX macos_network
#else
    #define ET_NETWORK_IMPL_HEADER "libetude/platform/generic/generic_network.h"
    #define ET_NETWORK_IMPL_PREFIX generic_network
#endif

// 동적 라이브러리 구현 선택
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_DYNLIB_IMPL_HEADER "libetude/platform/windows/windows_dynlib.h"
    #define ET_DYNLIB_IMPL_PREFIX windows_dynlib
#else
    #define ET_DYNLIB_IMPL_HEADER "libetude/platform/posix/posix_dynlib.h"
    #define ET_DYNLIB_IMPL_PREFIX posix_dynlib
#endif

// ============================================================================
// 함수 이름 생성 매크로
// ============================================================================

/**
 * @brief 플랫폼별 함수 이름을 생성하는 매크로
 */
#define ET_CONCAT_IMPL(a, b) a##_##b
#define ET_CONCAT(a, b) ET_CONCAT_IMPL(a, b)

// 플랫폼별 함수 이름 생성
#define ET_AUDIO_FUNC(name) ET_CONCAT(ET_AUDIO_IMPL_PREFIX, name)
#define ET_SYSTEM_FUNC(name) ET_CONCAT(ET_SYSTEM_IMPL_PREFIX, name)
#define ET_THREAD_FUNC(name) ET_CONCAT(ET_THREAD_IMPL_PREFIX, name)
#define ET_MEMORY_FUNC(name) ET_CONCAT(ET_MEMORY_IMPL_PREFIX, name)
#define ET_FILESYSTEM_FUNC(name) ET_CONCAT(ET_FILESYSTEM_IMPL_PREFIX, name)
#define ET_NETWORK_FUNC(name) ET_CONCAT(ET_NETWORK_IMPL_PREFIX, name)
#define ET_DYNLIB_FUNC(name) ET_CONCAT(ET_DYNLIB_IMPL_PREFIX, name)

// ============================================================================
// 인라인 함수 최적화 매크로
// ============================================================================

/**
 * @brief 성능이 중요한 함수들을 인라인으로 처리하여
 * 함수 호출 오버헤드를 제거합니다.
 */

// 기본 인라인 래퍼 매크로
#define ET_INLINE_WRAPPER(return_type, func_name, params, args) \
    LIBETUDE_INLINE return_type func_name params { \
        return ET_CONCAT(ET_AUDIO_IMPL_PREFIX, func_name) args; \
    }

// 오디오 인라인 래퍼들
#define ET_AUDIO_INLINE_WRAPPERS() \
    LIBETUDE_INLINE ETResult et_audio_start_stream_inline(ETAudioDevice* device) { \
        return ET_AUDIO_FUNC(start_stream)(device); \
    } \
    LIBETUDE_INLINE ETResult et_audio_stop_stream_inline(ETAudioDevice* device) { \
        return ET_AUDIO_FUNC(stop_stream)(device); \
    } \
    LIBETUDE_INLINE uint32_t et_audio_get_latency_inline(const ETAudioDevice* device) { \
        return ET_AUDIO_FUNC(get_latency)(device); \
    }

// 시스템 정보 인라인 래퍼들
#define ET_SYSTEM_INLINE_WRAPPERS() \
    LIBETUDE_INLINE ETResult et_system_get_high_resolution_time_inline(uint64_t* time_ns) { \
        return ET_SYSTEM_FUNC(get_high_resolution_time)(time_ns); \
    } \
    LIBETUDE_INLINE uint32_t et_system_get_simd_features_inline(void) { \
        return ET_SYSTEM_FUNC(get_simd_features)(); \
    } \
    LIBETUDE_INLINE bool et_system_has_feature_inline(ETHardwareFeature feature) { \
        return ET_SYSTEM_FUNC(has_feature)(feature); \
    }

// 메모리 관리 인라인 래퍼들
#define ET_MEMORY_INLINE_WRAPPERS() \
    LIBETUDE_INLINE void* et_memory_aligned_malloc_inline(size_t size, size_t alignment) { \
        return ET_MEMORY_FUNC(aligned_malloc)(size, alignment); \
    } \
    LIBETUDE_INLINE void et_memory_aligned_free_inline(void* ptr) { \
        ET_MEMORY_FUNC(aligned_free)(ptr); \
    }

// ============================================================================
// 조건부 컴파일 매크로
// ============================================================================

/**
 * @brief 불필요한 코드를 컴파일 타임에 제거하는 매크로들
 */

// SIMD 기능별 조건부 컴파일
#ifdef LIBETUDE_HAVE_SSE
    #define ET_IF_SSE(code) code
    #define ET_SSE_ENABLED 1
#else
    #define ET_IF_SSE(code)
    #define ET_SSE_ENABLED 0
#endif

#ifdef LIBETUDE_HAVE_SSE2
    #define ET_IF_SSE2(code) code
    #define ET_SSE2_ENABLED 1
#else
    #define ET_IF_SSE2(code)
    #define ET_SSE2_ENABLED 0
#endif

#ifdef LIBETUDE_HAVE_AVX
    #define ET_IF_AVX(code) code
    #define ET_AVX_ENABLED 1
#else
    #define ET_IF_AVX(code)
    #define ET_AVX_ENABLED 0
#endif

#ifdef LIBETUDE_HAVE_AVX2
    #define ET_IF_AVX2(code) code
    #define ET_AVX2_ENABLED 1
#else
    #define ET_IF_AVX2(code)
    #define ET_AVX2_ENABLED 0
#endif

#ifdef LIBETUDE_HAVE_NEON
    #define ET_IF_NEON(code) code
    #define ET_NEON_ENABLED 1
#else
    #define ET_IF_NEON(code)
    #define ET_NEON_ENABLED 0
#endif

// 플랫폼별 조건부 컴파일
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_IF_WINDOWS(code) code
    #define ET_IF_NOT_WINDOWS(code)
#else
    #define ET_IF_WINDOWS(code)
    #define ET_IF_NOT_WINDOWS(code) code
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
    #define ET_IF_LINUX(code) code
    #define ET_IF_NOT_LINUX(code)
#else
    #define ET_IF_LINUX(code)
    #define ET_IF_NOT_LINUX(code) code
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
    #define ET_IF_MACOS(code) code
    #define ET_IF_NOT_MACOS(code)
#else
    #define ET_IF_MACOS(code)
    #define ET_IF_NOT_MACOS(code) code
#endif

// 디버그/릴리스 조건부 컴파일
#if LIBETUDE_DEBUG
    #define ET_IF_DEBUG(code) code
    #define ET_IF_RELEASE(code)
#else
    #define ET_IF_DEBUG(code)
    #define ET_IF_RELEASE(code) code
#endif

// 최소 모드 조건부 컴파일
#ifdef LIBETUDE_MINIMAL
    #define ET_IF_MINIMAL(code) code
    #define ET_IF_NOT_MINIMAL(code)
#else
    #define ET_IF_MINIMAL(code)
    #define ET_IF_NOT_MINIMAL(code) code
#endif

// ============================================================================
// 컴파일러별 최적화 힌트 매크로
// ============================================================================

/**
 * @brief 컴파일러별 최적화 힌트를 제공하는 매크로들
 */

// 분기 예측 힌트 (이미 config.h에 정의되어 있지만 여기서 확장)
#define ET_LIKELY LIBETUDE_LIKELY
#define ET_UNLIKELY LIBETUDE_UNLIKELY

// 함수 속성 매크로
#ifdef LIBETUDE_COMPILER_MSVC
    #define ET_HOT_FUNCTION
    #define ET_COLD_FUNCTION
    #define ET_PURE_FUNCTION
    #define ET_CONST_FUNCTION
    #define ET_MALLOC_FUNCTION
    #define ET_NORETURN_FUNCTION __declspec(noreturn)
    #define ET_DEPRECATED_FUNCTION __declspec(deprecated)
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define ET_HOT_FUNCTION __attribute__((hot))
    #define ET_COLD_FUNCTION __attribute__((cold))
    #define ET_PURE_FUNCTION __attribute__((pure))
    #define ET_CONST_FUNCTION __attribute__((const))
    #define ET_MALLOC_FUNCTION __attribute__((malloc))
    #define ET_NORETURN_FUNCTION __attribute__((noreturn))
    #define ET_DEPRECATED_FUNCTION __attribute__((deprecated))
#else
    #define ET_HOT_FUNCTION
    #define ET_COLD_FUNCTION
    #define ET_PURE_FUNCTION
    #define ET_CONST_FUNCTION
    #define ET_MALLOC_FUNCTION
    #define ET_NORETURN_FUNCTION
    #define ET_DEPRECATED_FUNCTION
#endif

// 루프 최적화 힌트
#ifdef LIBETUDE_COMPILER_MSVC
    #define ET_LOOP_UNROLL(n) __pragma(loop(unroll(n)))
    #define ET_LOOP_VECTORIZE __pragma(loop(ivdep))
    #define ET_LOOP_NO_VECTORIZE __pragma(loop(no_vector))
#elif defined(LIBETUDE_COMPILER_GCC)
    #define ET_LOOP_UNROLL(n) _Pragma("GCC unroll " #n)
    #define ET_LOOP_VECTORIZE _Pragma("GCC ivdep")
    #define ET_LOOP_NO_VECTORIZE _Pragma("GCC novector")
#elif defined(LIBETUDE_COMPILER_CLANG)
    #define ET_LOOP_UNROLL(n) _Pragma("clang loop unroll_count(" #n ")")
    #define ET_LOOP_VECTORIZE _Pragma("clang loop vectorize(enable)")
    #define ET_LOOP_NO_VECTORIZE _Pragma("clang loop vectorize(disable)")
#else
    #define ET_LOOP_UNROLL(n)
    #define ET_LOOP_VECTORIZE
    #define ET_LOOP_NO_VECTORIZE
#endif

// 메모리 접근 힌트
#ifdef LIBETUDE_COMPILER_MSVC
    #define ET_PREFETCH(addr, rw, locality) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define ET_PREFETCH(addr, rw, locality) __builtin_prefetch((addr), (rw), (locality))
#else
    #define ET_PREFETCH(addr, rw, locality) ((void)0)
#endif

// 메모리 배리어
#ifdef LIBETUDE_COMPILER_MSVC
    #include <intrin.h>
    #define ET_MEMORY_BARRIER() _ReadWriteBarrier()
    #define ET_COMPILER_BARRIER() _ReadWriteBarrier()
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #define ET_MEMORY_BARRIER() __sync_synchronize()
    #define ET_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
    #define ET_MEMORY_BARRIER() ((void)0)
    #define ET_COMPILER_BARRIER() ((void)0)
#endif

// ============================================================================
// 성능 측정 매크로
// ============================================================================

/**
 * @brief 컴파일 타임에 성능 측정 코드를 포함/제외하는 매크로
 */

#if LIBETUDE_DEBUG && !defined(LIBETUDE_DISABLE_PROFILING)
    #define ET_PROFILE_ENABLED 1
    #define ET_PROFILE_SCOPE(name) ETProfileScope _prof_scope(name)
    #define ET_PROFILE_FUNCTION() ET_PROFILE_SCOPE(__FUNCTION__)
    #define ET_PROFILE_BEGIN(name) et_profile_begin(name)
    #define ET_PROFILE_END(name) et_profile_end(name)
#else
    #define ET_PROFILE_ENABLED 0
    #define ET_PROFILE_SCOPE(name) ((void)0)
    #define ET_PROFILE_FUNCTION() ((void)0)
    #define ET_PROFILE_BEGIN(name) ((void)0)
    #define ET_PROFILE_END(name) ((void)0)
#endif

// ============================================================================
// 컴파일 타임 어서션
// ============================================================================

/**
 * @brief 컴파일 타임에 조건을 검사하는 매크로
 */

#ifdef LIBETUDE_COMPILER_MSVC
    #define ET_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(LIBETUDE_COMPILER_GCC) || defined(LIBETUDE_COMPILER_CLANG)
    #if __STDC_VERSION__ >= 201112L
        #define ET_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
    #else
        #define ET_STATIC_ASSERT(cond, msg) \
            typedef char ET_CONCAT(static_assertion_failed_, __LINE__)[(cond) ? 1 : -1]
    #endif
#else
    #define ET_STATIC_ASSERT(cond, msg) \
        typedef char ET_CONCAT(static_assertion_failed_, __LINE__)[(cond) ? 1 : -1]
#endif

// ============================================================================
// 플랫폼별 최적화 설정 검증
// ============================================================================

// 컴파일 타임에 플랫폼 설정이 올바른지 검증
ET_STATIC_ASSERT(sizeof(void*) == sizeof(uintptr_t), "Pointer size mismatch");

#ifdef LIBETUDE_PLATFORM_WINDOWS
    ET_STATIC_ASSERT(sizeof(void*) >= 4, "Windows requires at least 32-bit pointers");
#endif

#if defined(LIBETUDE_HAVE_AVX2) && !defined(LIBETUDE_HAVE_AVX)
    #error "AVX2 requires AVX support"
#endif

#if defined(LIBETUDE_HAVE_SSE4_2) && !defined(LIBETUDE_HAVE_SSE4_1)
    #error "SSE4.2 requires SSE4.1 support"
#endif

// ============================================================================
// 유틸리티 함수 선언
// ============================================================================

/**
 * @brief 컴파일 타임 최적화 시스템을 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_optimization_initialize(void);

/**
 * @brief 현재 컴파일 타임 설정 정보를 가져옵니다
 * @param info 설정 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_compilation_info(struct ETCompilationInfo* info);

/**
 * @brief 컴파일 타임 최적화 시스템을 정리합니다
 */
void et_optimization_finalize(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_OPTIMIZATION_H