/* LibEtude Windows 플랫폼 구현 */
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
    HMODULE kernel32_handle;
    HMODULE ntdll_handle;
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
    config.security.use_secure_allocator = true;
    config.security.minimum_uac_level = ET_UAC_LEVEL_USER;

    /* 개발 도구 기본 설정 */
    config.development.enable_etw_logging = false;
    config.development.generate_pdb = false;
    config.development.log_file_path = NULL;

    return config;
}

/* Windows 버전 확인 함수 */
static bool _check_windows_version(void) {
    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    /* Windows 10 이상 확인 (Build 10240+) */
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        if (osvi.dwMajorVersion >= 10) {
            return true;
        }
        /* Windows 8.1 이상도 지원 */
        if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 3) {
            return true;
        }
    }

    return false;
}

/* 필수 DLL 로드 함수 */
static ETResult _load_required_dlls(void) {
    /* kernel32.dll 로드 (이미 로드되어 있음) */
    g_windows_platform.kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (!g_windows_platform.kernel32_handle) {
        return ET_ERROR_PLATFORM_INIT_FAILED;
    }

    /* ntdll.dll 로드 (Large Page 지원용) */
    g_windows_platform.ntdll_handle = GetModuleHandle(L"ntdll.dll");
    if (!g_windows_platform.ntdll_handle) {
        /* ntdll.dll이 없으면 일부 기능만 제한 */
        g_windows_platform.config.performance.enable_large_pages = false;
    }

    return ET_SUCCESS;
}

/* Windows 플랫폼 초기화 */
ETResult et_windows_init(const ETWindowsPlatformConfig* config) {
    if (g_windows_platform.initialized) {
        return ET_ERROR_ALREADY_INITIALIZED;
    }

    /* 오류 처리 시스템 초기화 */
    ETResult error_result = et_windows_error_init();
    if (error_result != ET_SUCCESS) {
        return error_result;
    }

    /* 기본 폴백 콜백 등록 */
    et_windows_register_default_fallbacks();

    /* Windows 버전 확인 */
    if (!_check_windows_version()) {
        ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_UNSUPPORTED_WINDOWS_VERSION, 0, S_OK,
                               "Unsupported Windows version detected");
        return ET_ERROR_UNSUPPORTED_PLATFORM;
    }

    /* 설정 복사 */
    if (config) {
        g_windows_platform.config = *config;
    } else {
        g_windows_platform.config = et_windows_create_default_config();
    }

    /* 필수 DLL 로드 */
    ETResult result = _load_required_dlls();
    if (result != ET_SUCCESS) {
        ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_REQUIRED_DLL_NOT_FOUND, GetLastError(), S_OK,
                               "Failed to load required Windows DLLs");
        return result;
    }

    /* COM 초기화 (WASAPI용) */
    if (g_windows_platform.config.audio.prefer_wasapi) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            /* COM 초기화 실패 시 DirectSound로 폴백 */
            ET_WINDOWS_REPORT_HRESULT_ERROR(ET_WINDOWS_ERROR_COM_INIT_FAILED, hr,
                                          "COM initialization failed, falling back to DirectSound");
            g_windows_platform.config.audio.prefer_wasapi = false;
        }
    }

    /* 보안 기능 확인 */
    if (g_windows_platform.config.security.enforce_dep) {
        if (!et_windows_check_dep_compatibility()) {
            ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_DEP_NOT_SUPPORTED, 0, S_OK,
                                   "DEP compatibility check failed");
            return ET_ERROR_SECURITY_CHECK_FAILED;
        }
    }

    /* ASLR 호환성 확인 */
    if (g_windows_platform.config.security.require_aslr) {
        if (!et_windows_check_aslr_compatibility()) {
            ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_ASLR_NOT_SUPPORTED, 0, S_OK,
                                   "ASLR compatibility check failed");
            return ET_ERROR_SECURITY_CHECK_FAILED;
        }
    }

    /* UAC 권한 확인 */
    if (g_windows_platform.config.security.check_uac) {
        if (!et_windows_check_uac_permissions()) {
            /* UAC 권한이 부족하면 일부 기능 제한 */
            ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_UAC_INSUFFICIENT_PRIVILEGES, 0, S_OK,
                                   "Insufficient UAC privileges, disabling some features");
            g_windows_platform.config.performance.enable_large_pages = false;
            g_windows_platform.config.security.use_secure_allocator = false;
        }
    }

    /* ETW 프로바이더 등록 */
    if (g_windows_platform.config.development.enable_etw_logging) {
        result = et_windows_register_etw_provider();
        if (result != ET_SUCCESS) {
            /* ETW 등록 실패는 치명적이지 않음 */
            ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED, 0, S_OK,
                                   "ETW provider registration failed, disabling ETW logging");
            g_windows_platform.config.development.enable_etw_logging = false;
        }
    }

    /* 스레드 풀 크기 자동 설정 */
    if (g_windows_platform.config.performance.thread_pool_size == 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        g_windows_platform.config.performance.thread_pool_size = sysinfo.dwNumberOfProcessors;
    }

    g_windows_platform.initialized = true;

    return ET_SUCCESS;
}

/* Windows 플랫폼 정리 */
void et_windows_finalize(void) {
    if (!g_windows_platform.initialized) {
        return;
    }

    /* COM 정리 */
    if (g_windows_platform.config.audio.prefer_wasapi) {
        CoUninitialize();
    }

    /* 핸들 정리 */
    if (g_windows_platform.kernel32_handle) {
        /* kernel32.dll은 시스템 DLL이므로 해제하지 않음 */
        g_windows_platform.kernel32_handle = NULL;
    }

    if (g_windows_platform.ntdll_handle) {
        /* ntdll.dll도 시스템 DLL이므로 해제하지 않음 */
        g_windows_platform.ntdll_handle = NULL;
    }

    /* 오류 처리 시스템 정리 */
    et_windows_error_finalize();

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

    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
        return ET_ERROR_PLATFORM_INFO_UNAVAILABLE;
    }

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    int written = snprintf(buffer, buffer_size,
        "Windows Platform Information:\n"
        "- OS Version: %lu.%lu.%lu\n"
        "- Processor Architecture: %s\n"
        "- Number of Processors: %lu\n"
        "- WASAPI Enabled: %s\n"
        "- Large Pages Enabled: %s\n"
        "- ETW Logging Enabled: %s\n",
        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber,
        (sysinfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "x64" :
        (sysinfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) ? "x86" : "Unknown",
        sysinfo.dwNumberOfProcessors,
        g_windows_platform.config.audio.prefer_wasapi ? "Yes" : "No",
        g_windows_platform.config.performance.enable_large_pages ? "Yes" : "No",
        g_windows_platform.config.development.enable_etw_logging ? "Yes" : "No"
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    return ET_SUCCESS;
}

/* Windows 보안 기능 구현 */

/* DEP 호환성 확인 */
bool et_windows_check_dep_compatibility(void) {
    /* GetProcessDEPPolicy 함수를 사용하여 DEP 상태 확인 */
    typedef BOOL (WINAPI *GetProcessDEPPolicyFunc)(HANDLE, LPDWORD, PBOOL);

    HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
    if (!kernel32) {
        return false;
    }

    GetProcessDEPPolicyFunc GetProcessDEPPolicy =
        (GetProcessDEPPolicyFunc)GetProcAddress(kernel32, "GetProcessDEPPolicy");

    if (!GetProcessDEPPolicy) {
        /* Windows XP SP3 이하에서는 DEP 지원 안함 */
        return false;
    }

    DWORD flags = 0;
    BOOL permanent = FALSE;

    if (GetProcessDEPPolicy(GetCurrentProcess(), &flags, &permanent)) {
        /* DEP가 활성화되어 있으면 호환성 확인 */
        return (flags & PROCESS_DEP_ENABLE) != 0;
    }

    return false;
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
    /* 현재 프로세스의 토큰을 열어서 권한 확인 */
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(TOKEN_ELEVATION);

    bool is_elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, size, &size)) {
        is_elevated = elevation.TokenIsElevated != 0;
    }

    CloseHandle(token);
    return is_elevated;
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
        int edx = cpuinfo[3];

        features.has_sse41 = (ecx & (1 << 19)) != 0;  /* SSE4.1 */
        features.has_avx = (ecx & (1 << 28)) != 0;    /* AVX */

        /* EDX 레지스터에서 기본 SSE 지원 확인 */
        bool has_sse = (edx & (1 << 25)) != 0;        /* SSE */
        bool has_sse2 = (edx & (1 << 26)) != 0;       /* SSE2 */

        if (has_sse && has_sse2) {
            /* 기본 SSE 지원이 있어야 상위 기능도 의미가 있음 */
        }
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
    /* 실제 ETW 구현은 복잡하므로 여기서는 기본 스텁만 제공 */
    /* 향후 별도 파일에서 완전한 ETW 구현 예정 */
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

/* Large Page 메모리 할당 구현 */

/* Large Page 권한 활성화 */
bool et_windows_enable_large_page_privilege(void) {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }

    bool result = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL) != 0;
    CloseHandle(token);

    return result && GetLastError() == ERROR_SUCCESS;
}

/* Large Page 메모리 할당 */
void* et_windows_alloc_large_pages(size_t size) {
    if (!g_windows_platform.config.performance.enable_large_pages) {
        /* Large Page가 비활성화되어 있으면 일반 할당 */
        return et_windows_alloc_aslr_compatible(size);
    }

    /* Large Page 크기 조회 */
    SIZE_T large_page_size = GetLargePageMinimum();
    if (large_page_size == 0) {
        /* Large Page를 지원하지 않으면 일반 할당 */
        return et_windows_alloc_aslr_compatible(size);
    }

    /* 크기를 Large Page 경계로 정렬 */
    size_t aligned_size = ((size + large_page_size - 1) / large_page_size) * large_page_size;

    /* Large Page 메모리 할당 시도 */
    void* ptr = VirtualAlloc(NULL, aligned_size,
                           MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                           PAGE_READWRITE);

    if (!ptr) {
        /* Large Page 할당 실패 시 일반 메모리로 폴백 */
        ptr = et_windows_alloc_aslr_compatible(size);
    }

    return ptr;
}

/* Thread Pool 관리 구현 (기본 스텁) */

/* Windows Thread Pool 초기화 */
ETResult et_windows_threadpool_init(ETWindowsThreadPool* pool,
                                  DWORD min_threads, DWORD max_threads) {
    if (!pool) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    /* Thread Pool 생성 */
    pool->thread_pool = CreateThreadpool(NULL);
    if (!pool->thread_pool) {
        return ET_ERROR_THREAD;
    }

    /* Cleanup Group 생성 */
    pool->cleanup_group = CreateThreadpoolCleanupGroup();
    if (!pool->cleanup_group) {
        CloseThreadpool(pool->thread_pool);
        return ET_ERROR_THREAD;
    }

    /* Callback Environment 초기화 */
    InitializeThreadpoolEnvironment(&pool->callback_env);
    SetThreadpoolCallbackPool(&pool->callback_env, pool->thread_pool);
    SetThreadpoolCallbackCleanupGroup(&pool->callback_env, pool->cleanup_group, NULL);

    /* Thread Pool 크기 설정 */
    SetThreadpoolThreadMinimum(pool->thread_pool, min_threads);
    SetThreadpoolThreadMaximum(pool->thread_pool, max_threads);

    return ET_SUCCESS;
}

/* SIMD 최적화 커널 구현 (기본 스텁) */

/* AVX2 최적화된 행렬 곱셈 */
void et_windows_simd_matrix_multiply_avx2(const float* a, const float* b,
                                        float* c, int m, int n, int k) {
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) {
        return;
    }

    /* CPU가 AVX2를 지원하는지 확인 */
    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx2) {
        /* AVX2를 지원하지 않으면 기본 구현 사용 */
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                float sum = 0.0f;
                for (int l = 0; l < k; l++) {
                    sum += a[i * k + l] * b[l * n + j];
                }
                c[i * n + j] = sum;
            }
        }
        return;
    }

    /* 실제 AVX2 최적화 구현은 별도 파일에서 구현 예정 */
    /* 여기서는 기본 구현만 제공 */
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