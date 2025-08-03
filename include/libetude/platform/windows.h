/* LibEtude Windows 플랫폼 헤더 파일 */
/* Copyright (c) 2025 LibEtude Project */

#ifndef LIBETUDE_PLATFORM_WINDOWS_H
#define LIBETUDE_PLATFORM_WINDOWS_H

#ifdef _WIN32

/* Windows 헤더 포함 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <dsound.h>
#include <mmsystem.h>
#include <intrin.h>  /* CPUID 지원 */

/* 필요한 전방 선언 */
#include "libetude/audio_io.h"
#include "libetude/platform/windows_security.h"

/* Windows 플랫폼 설정 구조체 */
typedef struct {
    /* 오디오 설정 */
    struct {
        bool prefer_wasapi;        /* WASAPI 우선 사용 */
        DWORD buffer_size_ms;      /* 오디오 버퍼 크기 (밀리초) */
        bool exclusive_mode;       /* 독점 모드 사용 여부 */
    } audio;

    /* 성능 설정 */
    struct {
        bool enable_large_pages;   /* Large Page 사용 여부 */
        bool enable_avx_optimization; /* AVX 최적화 활성화 */
        DWORD thread_pool_size;    /* 스레드 풀 크기 */
    } performance;

    /* 보안 설정 */
    struct {
        bool enforce_dep;          /* DEP 강제 적용 */
        bool require_aslr;         /* ASLR 요구 */
        bool check_uac;            /* UAC 권한 확인 */
        bool use_secure_allocator; /* 보안 메모리 할당자 사용 */
        ETUACLevel minimum_uac_level; /* 최소 UAC 권한 레벨 */
    } security;

    /* 개발 도구 설정 */
    struct {
        bool enable_etw_logging;   /* ETW 로깅 활성화 */
        bool generate_pdb;         /* PDB 파일 생성 */
        const char* log_file_path; /* 로그 파일 경로 */
    } development;
} ETWindowsPlatformConfig;

/* 이전 버전 호환성을 위한 별칭 */
typedef ETWindowsPlatformConfig ETWindowsConfig;

/* WASAPI 컨텍스트 */
typedef struct {
    IMMDeviceEnumerator* device_enumerator;
    IMMDevice* audio_device;
    IAudioClient* audio_client;
    IAudioRenderClient* render_client;
    HANDLE audio_event;
    bool is_exclusive_mode;
} ETWASAPIContext;

/* Windows 오디오 디바이스 정보 */
typedef struct {
    wchar_t device_id[256];
    wchar_t friendly_name[256];
    DWORD sample_rate;
    WORD channels;
    WORD bits_per_sample;
    bool is_default;
    bool supports_exclusive;
} ETWindowsAudioDevice;

/* Windows 특화 오류 코드 */
typedef enum {
    ET_WINDOWS_ERROR_WASAPI_INIT_FAILED = 0x1000,
    ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
    ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED,
    ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED,
    ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED,
    ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED
} ETWindowsErrorCode;

/* Windows CPU 기능 정보 */
typedef struct {
    bool has_sse41;
    bool has_avx;
    bool has_avx2;
    bool has_avx512;
} ETWindowsCPUFeatures;

/* Windows Thread Pool 래퍼 */
typedef struct {
    PTP_POOL thread_pool;
    PTP_CLEANUP_GROUP cleanup_group;
    TP_CALLBACK_ENVIRON callback_env;
} ETWindowsThreadPool;

#ifdef __cplusplus
extern "C" {
#endif

/* Windows 플랫폼 초기화/정리 함수 */
ETResult et_windows_init(const ETWindowsPlatformConfig* config);
void et_windows_finalize(void);

/* 기본 설정 생성 함수 */
ETWindowsPlatformConfig et_windows_create_default_config(void);

/* 플랫폼 상태 확인 함수 */
bool et_windows_is_initialized(void);
ETResult et_windows_get_platform_info(char* buffer, size_t buffer_size);

/* WASAPI 함수 */
ETResult et_audio_init_wasapi_with_fallback(ETAudioDevice* device);
ETResult et_audio_fallback_to_directsound(ETAudioDevice* device);

/* WASAPI 디바이스 열거 및 관리 */
ETResult et_windows_enumerate_audio_devices(ETWindowsAudioDevice** devices,
                                          uint32_t* device_count);
void et_windows_free_audio_devices(ETWindowsAudioDevice* devices);
ETResult et_windows_init_wasapi_device(const wchar_t* device_id,
                                     const ETAudioFormat* format,
                                     ETWASAPIContext* context);
void et_windows_cleanup_wasapi_context(ETWASAPIContext* context);
void et_windows_wasapi_cleanup(void);

/* WASAPI 렌더링 및 스트림 제어 (불투명 구조체) */
typedef struct ETWASAPIDevice ETWASAPIDevice;
ETResult et_wasapi_start_stream(ETWASAPIDevice* wasapi_device,
                               ETAudioCallback callback, void* user_data);
ETResult et_wasapi_stop_stream(ETWASAPIDevice* wasapi_device);
void et_wasapi_cleanup_device(ETWASAPIDevice* wasapi_device);

/* WASAPI 볼륨 제어 */
ETResult et_wasapi_set_volume(ETWASAPIDevice* wasapi_device, float volume);
ETResult et_wasapi_get_volume(ETWASAPIDevice* wasapi_device, float* volume);
ETResult et_wasapi_set_mute(ETWASAPIDevice* wasapi_device, bool mute);
ETResult et_wasapi_get_mute(ETWASAPIDevice* wasapi_device, bool* mute);

/* WASAPI 성능 모니터링 */
ETResult et_wasapi_get_performance_stats(ETWASAPIDevice* wasapi_device,
                                       double* avg_callback_duration,
                                       UINT32* current_padding,
                                       UINT32* buffer_frame_count);

/* DirectSound 폴백 함수 (내부 구조체는 불투명) */
typedef struct ETDirectSoundDevice ETDirectSoundDevice;
ETResult et_windows_start_directsound_device(ETDirectSoundDevice* ds_device);
ETResult et_windows_stop_directsound_device(ETDirectSoundDevice* ds_device);
void et_windows_cleanup_directsound_device(ETDirectSoundDevice* ds_device);
void et_windows_directsound_cleanup(void);

/* DirectSound 상태 확인 및 복구 */
ETResult et_windows_check_directsound_device_status(ETDirectSoundDevice* device);
ETResult et_windows_get_directsound_performance_stats(ETDirectSoundDevice* device,
                                                    double* avg_callback_duration,
                                                    DWORD* current_write_cursor,
                                                    DWORD* buffer_size);

/* 통합 오디오 폴백 관리 */
ETResult et_windows_init_audio_with_fallback(ETAudioDevice* device,
                                           const ETAudioFormat* format);
ETResult et_windows_check_audio_backend_status(ETAudioDevice* device);
ETResult et_windows_attempt_audio_recovery(ETAudioDevice* device);
ETResult et_windows_get_fallback_manager_info(char* buffer, size_t buffer_size);
void et_windows_set_auto_recovery_enabled(bool enabled);
void et_windows_cleanup_fallback_manager(void);

/* 보안 기능 */
bool et_windows_check_dep_compatibility(void);
void* et_windows_alloc_aslr_compatible(size_t size);
bool et_windows_check_uac_permissions(void);

/* ETW 지원 */
ETResult et_windows_register_etw_provider(void);
void et_windows_log_performance_event(const char* event_name, double duration_ms);
void et_windows_log_error_event(ETErrorCode error_code, const char* description);

/* CPU 기능 감지 */
ETWindowsCPUFeatures et_windows_detect_cpu_features(void);

/* SIMD 최적화 커널 */
void et_windows_simd_matrix_multiply_avx2(const float* a, const float* b,
                                        float* c, int m, int n, int k);

/* Thread Pool 관리 */
ETResult et_windows_threadpool_init(ETWindowsThreadPool* pool,
                                  DWORD min_threads, DWORD max_threads);

/* Large Page 메모리 할당 */
void* et_windows_alloc_large_pages(size_t size);
bool et_windows_enable_large_page_privilege(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* LIBETUDE_PLATFORM_WINDOWS_H */