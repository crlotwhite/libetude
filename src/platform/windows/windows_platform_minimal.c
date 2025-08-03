/* LibEtude Windows 플랫폼 최소 구현 */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows.h"
#include "libetude/error.h"
#include "libetude/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows 플랫폼 전역 상태 */
static struct {
    bool initialized;
    ETWindowsPlatformConfig config;
} g_windows_platform = { false };

/* 기본 설정 생성 함수 */
ETWindowsPlatformConfig et_windows_create_default_config(void) {
    ETWindowsPlatformConfig config = { 0 };

    /* 오디오 기본 설정 */
    config.audio.prefer_wasapi = true;
    config.audio.buffer_size_ms = 20;  /* 20ms 버퍼 (저지연) */
    config.audio.exclusive_mode = false;

    /* 성능 기본 설정 */
    config.performance.enable_large_pages = true;
    config.performance.enable_avx_optimization = true;
    config.performance.thread_pool_size = 0;  /* 자동 감지 */

    /* 보안 기본 설정 */
    config.security.enforce_dep = true;
    config.security.require_aslr = true;
    config.security.check_uac = true;

    /* 개발 도구 기본 설정 */
    config.development.enable_etw_logging = false;
    config.development.generate_pdb = false;
    config.development.log_file_path = NULL;

    return config;
}

/* Windows 플랫폼 초기화 */
ETResult et_windows_init(const ETWindowsPlatformConfig* config) {
    if (g_windows_platform.initialized) {
        return ET_ERROR_ALREADY_INITIALIZED;
    }

    /* 설정 복사 */
    if (config) {
        g_windows_platform.config = *config;
    } else {
        g_windows_platform.config = et_windows_create_default_config();
    }

    g_windows_platform.initialized = true;

    return ET_SUCCESS;
}

/* Windows 플랫폼 정리 */
void et_windows_finalize(void) {
    if (!g_windows_platform.initialized) {
        return;
    }

    /* 상태 초기화 */
    memset(&g_windows_platform, 0, sizeof(g_windows_platform));
}

/* 플랫폼 초기화 상태 확인 */
bool et_windows_is_initialized(void) {
    return g_windows_platform.initialized;
}

/* 플랫폼 정보 조회 */
ETResult et_windows_get_platform_info(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_platform.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    int written = snprintf(buffer, buffer_size,
        "Windows Platform Information:\n"
        "- WASAPI Enabled: %s\n"
        "- Large Pages Enabled: %s\n"
        "- ETW Logging Enabled: %s\n",
        g_windows_platform.config.audio.prefer_wasapi ? "Yes" : "No",
        g_windows_platform.config.performance.enable_large_pages ? "Yes" : "No",
        g_windows_platform.config.development.enable_etw_logging ? "Yes" : "No"
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    return ET_SUCCESS;
}

/* DEP 호환성 확인 */
bool et_windows_check_dep_compatibility(void) {
    /* 간단한 구현 - 항상 true 반환 */
    return true;
}

/* ASLR 호환 메모리 할당 */
void* et_windows_alloc_aslr_compatible(size_t size) {
    /* VirtualAlloc을 사용하여 ASLR 호환 메모리 할당 */
    void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!ptr) {
        /* VirtualAlloc 실패 시 일반 malloc으로 폴백 */
        ptr = malloc(size);
    }

    return ptr;
}

/* UAC 권한 확인 */
bool et_windows_check_uac_permissions(void) {
    /* 간단한 구현 - 항상 false 반환 (일반 사용자) */
    return false;
}

/* CPU 기능 감지 구현 */

/* CPUID 명령어 실행 함수 */
static void _cpuid(int info[4], int function_id) {
#ifdef _MSC_VER
    __cpuid(info, function_id);
#elif defined(__GNUC__)
    __asm__ volatile (
        "cpuid"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "a" (function_id)
    );
#else
    /* 컴파일러가 CPUID를 지원하지 않으면 모든 기능 비활성화 */
    info[0] = info[1] = info[2] = info[3] = 0;
#endif
}

/* Windows CPU 기능 감지 */
ETWindowsCPUFeatures et_windows_detect_cpu_features(void) {
    ETWindowsCPUFeatures features = { 0 };
    int cpuinfo[4] = { 0 };

    /* CPUID 기본 정보 조회 */
    _cpuid(cpuinfo, 0);
    int max_function_id = cpuinfo[0];

    if (max_function_id >= 1) {
        /* 기본 기능 플래그 조회 */
        _cpuid(cpuinfo, 1);

        /* ECX 레지스터의 기능 플래그 */
        int ecx = cpuinfo[2];

        features.has_sse41 = (ecx & (1 << 19)) != 0;  /* SSE4.1 */
        features.has_avx = (ecx & (1 << 28)) != 0;    /* AVX */
    }

    if (max_function_id >= 7) {
        /* 확장 기능 플래그 조회 */
        _cpuid(cpuinfo, 7);

        /* EBX 레지스터의 확장 기능 플래그 */
        int ebx = cpuinfo[1];

        features.has_avx2 = (ebx & (1 << 5)) != 0;    /* AVX2 */
        features.has_avx512 = (ebx & (1 << 16)) != 0; /* AVX-512F */
    }

    return features;
}

/* ETW 지원 구현 (기본 스텁) */

/* ETW 프로바이더 등록 */
ETResult et_windows_register_etw_provider(void) {
    /* 기본 스텁 - 항상 성공 */
    return ET_SUCCESS;
}

/* 성능 이벤트 로깅 */
void et_windows_log_performance_event(const char* event_name, double duration_ms) {
    if (!event_name) return;

    /* 기본적으로 디버그 출력으로 로깅 */
    if (g_windows_platform.config.development.enable_etw_logging) {
        char message[256];
        snprintf(message, sizeof(message),
                "Performance Event: %s took %.2f ms", event_name, duration_ms);

        OutputDebugStringA(message);
    }
}

/* 오류 이벤트 로깅 */
void et_windows_log_error_event(ETErrorCode error_code, const char* description) {
    if (!description) return;

    /* 기본적으로 디버그 출력으로 로깅 */
    if (g_windows_platform.config.development.enable_etw_logging) {
        char message[256];
        snprintf(message, sizeof(message),
                "Error Event: Code %d - %s", error_code, description);

        OutputDebugStringA(message);
    }
}

/* Large Page 권한 활성화 */
bool et_windows_enable_large_page_privilege(void) {
    /* 간단한 구현 - 항상 false 반환 */
    return false;
}

/* Large Page 메모리 할당 */
void* et_windows_alloc_large_pages(size_t size) {
    /* Large Page가 비활성화되어 있으면 일반 할당 */
    return et_windows_alloc_aslr_compatible(size);
}

/* Thread Pool 관리 구현 (기본 스텁) */

/* Windows Thread Pool 초기화 */
ETResult et_windows_threadpool_init(ETWindowsThreadPool* pool,
                                  DWORD min_threads, DWORD max_threads) {
    if (!pool) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    /* 기본 스텁 - 항상 성공 */
    return ET_SUCCESS;
}

/* SIMD 최적화 커널 구현 (기본 스텁) */

/* AVX2 최적화된 행렬 곱셈 */
void et_windows_simd_matrix_multiply_avx2(const float* a, const float* b,
                                        float* c, int m, int n, int k) {
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) {
        return;
    }

    /* 기본 구현 사용 */
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

/* 오디오 함수 스텁 */
ETResult et_audio_init_wasapi_with_fallback(ETAudioDevice* device) {
    (void)device;  /* 미사용 매개변수 경고 방지 */
    return ET_ERROR_NOT_IMPLEMENTED;
}

ETResult et_audio_fallback_to_directsound(ETAudioDevice* device) {
    (void)device;  /* 미사용 매개변수 경고 방지 */
    return ET_ERROR_NOT_IMPLEMENTED;
}