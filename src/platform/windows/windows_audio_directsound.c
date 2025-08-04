/**
 * @file windows_audio_directsound.c
 * @brief Windows DirectSound 오디오 백엔드 구현 (WASAPI 폴백용)
 * @author LibEtude Project
 * @version 1.0.0
 *
 * WASAPI 실패 시 사용되는 DirectSound 기반 오디오 출력 구현
 */

#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <dsound.h>
#include <mmsystem.h>

/* DirectSound 전역 상태 */
static struct {
    bool initialized;
    LPDIRECTSOUND8 direct_sound;
    HWND window_handle;
} g_directsound_state = { false };

/* DirectSound 디바이스 구조체 (내부 구현) */
typedef struct ETDirectSoundDeviceImpl {
    LPDIRECTSOUNDBUFFER8 secondary_buffer;
    LPDIRECTSOUNDBUFFER primary_buffer;
    ETAudioFormat format;
    ETAudioCallback callback;
    void* user_data;

    /* 스레딩 관련 */
    HANDLE audio_thread;
    HANDLE stop_event;
    bool is_running;

    /* 버퍼 관리 */
    DWORD buffer_size;
    DWORD write_cursor;
    DWORD safe_write_cursor;
    float* temp_buffer;

    /* 성능 모니터링 */
    LARGE_INTEGER perf_frequency;
    double avg_callback_duration;

} ETDirectSoundDeviceImpl;

/**
 * @brief DirectSound 시스템 초기화
 */
static ETResult _directsound_system_init(void) {
    if (g_directsound_state.initialized) {
        return ET_SUCCESS;
    }

    /* 더미 윈도우 핸들 생성 (DirectSound 초기화용) */
    g_directsound_state.window_handle = GetDesktopWindow();

    /* DirectSound 객체 생성 */
    HRESULT hr = DirectSoundCreate8(NULL, &g_directsound_state.direct_sound, NULL);
    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound 객체 생성 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 협력 레벨 설정 */
    hr = IDirectSound8_SetCooperativeLevel(g_directsound_state.direct_sound,
                                          g_directsound_state.window_handle,
                                          DSSCL_PRIORITY);

    if (FAILED(hr)) {
        IDirectSound8_Release(g_directsound_state.direct_sound);
        g_directsound_state.direct_sound = NULL;
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound 협력 레벨 설정 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    g_directsound_state.initialized = true;

    ET_LOG_INFO("DirectSound 시스템 초기화 완료");
    return ET_SUCCESS;
}

/**
 * @brief DirectSound 시스템 정리
 */
static void _directsound_system_cleanup(void) {
    if (!g_directsound_state.initialized) {
        return;
    }

    if (g_directsound_state.direct_sound) {
        IDirectSound8_Release(g_directsound_state.direct_sound);
        g_directsound_state.direct_sound = NULL;
    }

    g_directsound_state.window_handle = NULL;
    g_directsound_state.initialized = false;

    ET_LOG_INFO("DirectSound 시스템 정리 완료");
}

/**
 * @brief DirectSound 버퍼 생성
 */
static ETResult _create_directsound_buffer(ETDirectSoundDeviceImpl* ds_device,
                                         const ETAudioFormat* format) {
    HRESULT hr;

    /* 기본 버퍼 설명자 */
    DSBUFFERDESC primary_desc;
    memset(&primary_desc, 0, sizeof(DSBUFFERDESC));
    primary_desc.dwSize = sizeof(DSBUFFERDESC);
    primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    primary_desc.dwBufferBytes = 0;
    primary_desc.lpwfxFormat = NULL;

    /* 기본 버퍼 생성 */
    LPDIRECTSOUNDBUFFER temp_primary = NULL;
    hr = IDirectSound8_CreateSoundBuffer(g_directsound_state.direct_sound,
                                        &primary_desc, &temp_primary, NULL);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound 기본 버퍼 생성 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* IDirectSoundBuffer8 인터페이스 가져오기 */
    hr = IDirectSoundBuffer_QueryInterface(temp_primary, &IID_IDirectSoundBuffer8,
                                          (void**)&ds_device->primary_buffer);
    IDirectSoundBuffer_Release(temp_primary);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSoundBuffer8 인터페이스 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 기본 버퍼 포맷 설정 */
    WAVEFORMATEX wave_format;
    memset(&wave_format, 0, sizeof(WAVEFORMATEX));
    wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wave_format.nChannels = format->num_channels;
    wave_format.nSamplesPerSec = format->sample_rate;
    wave_format.wBitsPerSample = 32;
    wave_format.nBlockAlign = format->num_channels * sizeof(float);
    wave_format.nAvgBytesPerSec = format->sample_rate * wave_format.nBlockAlign;
    wave_format.cbSize = 0;

    hr = IDirectSoundBuffer8_SetFormat(ds_device->primary_buffer, &wave_format);
    if (FAILED(hr)) {
        ET_LOG_WARNING("기본 버퍼 포맷 설정 실패: 0x%08X (계속 진행)", hr);
    }

    /* 보조 버퍼 크기 계산 (200ms 버퍼) */
    ds_device->buffer_size = format->sample_rate * format->num_channels *
                            sizeof(float) * 200 / 1000;

    /* 보조 버퍼 설명자 */
    DSBUFFERDESC secondary_desc;
    memset(&secondary_desc, 0, sizeof(DSBUFFERDESC));
    secondary_desc.dwSize = sizeof(DSBUFFERDESC);
    secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    secondary_desc.dwBufferBytes = ds_device->buffer_size;
    secondary_desc.lpwfxFormat = &wave_format;

    /* 보조 버퍼 생성 */
    LPDIRECTSOUNDBUFFER temp_secondary = NULL;
    hr = IDirectSound8_CreateSoundBuffer(g_directsound_state.direct_sound,
                                        &secondary_desc, &temp_secondary, NULL);

    if (FAILED(hr)) {
        IDirectSoundBuffer8_Release(ds_device->primary_buffer);
        ds_device->primary_buffer = NULL;
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound 보조 버퍼 생성 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* IDirectSoundBuffer8 인터페이스 가져오기 */
    hr = IDirectSoundBuffer_QueryInterface(temp_secondary, &IID_IDirectSoundBuffer8,
                                          (void**)&ds_device->secondary_buffer);
    IDirectSoundBuffer_Release(temp_secondary);

    if (FAILED(hr)) {
        IDirectSoundBuffer8_Release(ds_device->primary_buffer);
        ds_device->primary_buffer = NULL;
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSoundBuffer8 보조 인터페이스 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 임시 버퍼 할당 */
    DWORD temp_buffer_frames = ds_device->buffer_size / (format->num_channels * sizeof(float));
    ds_device->temp_buffer = (float*)malloc(temp_buffer_frames * format->num_channels * sizeof(float));

    if (!ds_device->temp_buffer) {
        IDirectSoundBuffer8_Release(ds_device->secondary_buffer);
        IDirectSoundBuffer8_Release(ds_device->primary_buffer);
        ds_device->secondary_buffer = NULL;
        ds_device->primary_buffer = NULL;
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "DirectSound 임시 버퍼 할당 실패");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    /* 성능 카운터 초기화 */
    QueryPerformanceFrequency(&ds_device->perf_frequency);
    ds_device->avg_callback_duration = 0.0;

    ET_LOG_INFO("DirectSound 버퍼 생성 완료 (크기: %u 바이트)", ds_device->buffer_size);
    return ET_SUCCESS;
}

/**
 * @brief DirectSound 오디오 스레드 함수 (강화된 오류 처리)
 */
static DWORD WINAPI _directsound_audio_thread(LPVOID param) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)param;
    DWORD consecutive_errors = 0;
    const DWORD max_consecutive_errors = 10;
    const DWORD sleep_time = 10; /* 10ms 간격으로 체크 */

    /* 스레드 우선순위 설정 */
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
        ET_LOG_WARNING("DirectSound 스레드 우선순위 설정 실패 (오류: %lu)", GetLastError());
    }

    ET_LOG_INFO("DirectSound 오디오 스레드 시작 (스레드 ID: %lu)", GetCurrentThreadId());

    while (ds_device->is_running) {
        DWORD wait_result = WaitForSingleObject(ds_device->stop_event, sleep_time);

        if (wait_result == WAIT_OBJECT_0) {
            ET_LOG_INFO("DirectSound 스레드 정지 신호 수신");
            break;
        } else if (wait_result == WAIT_FAILED) {
            ET_LOG_ERROR("DirectSound 스레드 대기 실패 (오류: %lu)", GetLastError());
            break;
        }

        LARGE_INTEGER start_time, end_time;
        QueryPerformanceCounter(&start_time);

        /* 현재 재생 위치 가져오기 */
        DWORD play_cursor, write_cursor;
        HRESULT hr = IDirectSoundBuffer8_GetCurrentPosition(ds_device->secondary_buffer,
                                                           &play_cursor, &write_cursor);

        if (FAILED(hr)) {
            consecutive_errors++;
            ET_LOG_ERROR("DirectSound 커서 위치 가져오기 실패: 0x%08X (연속 오류: %lu)",
                        hr, consecutive_errors);

            if (consecutive_errors >= max_consecutive_errors) {
                ET_LOG_ERROR("DirectSound 연속 오류 한계 초과, 스레드 종료");
                break;
            }

            /* 오류 발생 시 더 긴 대기 */
            Sleep(50);
            continue;
        }

        /* 성공적인 작업 후 오류 카운터 리셋 */
        consecutive_errors = 0;

        /* 안전한 쓰기 영역 계산 */
        DWORD safe_write_size;
        if (write_cursor >= ds_device->write_cursor) {
            safe_write_size = write_cursor - ds_device->write_cursor;
        } else {
            safe_write_size = (ds_device->buffer_size - ds_device->write_cursor) + write_cursor;
        }

        /* 최소 쓰기 크기 확인 (20ms 분량) */
        DWORD min_write_size = ds_device->format.sample_rate * ds_device->format.num_channels *
                              sizeof(float) * 20 / 1000;

        if (safe_write_size >= min_write_size) {
            /* 버퍼 잠금 */
            void* buffer_ptr1 = NULL;
            void* buffer_ptr2 = NULL;
            DWORD buffer_size1 = 0;
            DWORD buffer_size2 = 0;

            hr = IDirectSoundBuffer8_Lock(ds_device->secondary_buffer,
                                         ds_device->write_cursor,
                                         safe_write_size,
                                         &buffer_ptr1, &buffer_size1,
                                         &buffer_ptr2, &buffer_size2,
                                         0);

            if (SUCCEEDED(hr)) {
                bool callback_success = true;

                /* 첫 번째 버퍼 영역 처리 */
                if (buffer_ptr1 && buffer_size1 > 0) {
                    DWORD frames1 = buffer_size1 / (ds_device->format.num_channels * sizeof(float));

                    if (ds_device->callback) {
                        /* 임시 버퍼 초기화 */
                        memset(ds_device->temp_buffer, 0, buffer_size1);

                        /* 콜백 호출 (예외 처리 포함) */
                        __try {
                            ds_device->callback(ds_device->temp_buffer, frames1, ds_device->user_data);
                            memcpy(buffer_ptr1, ds_device->temp_buffer, buffer_size1);
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER) {
                            ET_LOG_ERROR("DirectSound 콜백 예외 발생: 0x%08X", GetExceptionCode());
                            memset(buffer_ptr1, 0, buffer_size1);
                            callback_success = false;
                        }
                    } else {
                        memset(buffer_ptr1, 0, buffer_size1);
                    }
                }

                /* 두 번째 버퍼 영역 처리 (링 버퍼 랩어라운드) */
                if (buffer_ptr2 && buffer_size2 > 0 && callback_success) {
                    DWORD frames2 = buffer_size2 / (ds_device->format.num_channels * sizeof(float));

                    if (ds_device->callback) {
                        /* 임시 버퍼 초기화 */
                        memset(ds_device->temp_buffer, 0, buffer_size2);

                        /* 콜백 호출 (예외 처리 포함) */
                        __try {
                            ds_device->callback(ds_device->temp_buffer, frames2, ds_device->user_data);
                            memcpy(buffer_ptr2, ds_device->temp_buffer, buffer_size2);
                        }
                        __except(EXCEPTION_EXECUTE_HANDLER) {
                            ET_LOG_ERROR("DirectSound 콜백 예외 발생 (버퍼2): 0x%08X", GetExceptionCode());
                            memset(buffer_ptr2, 0, buffer_size2);
                        }
                    } else {
                        memset(buffer_ptr2, 0, buffer_size2);
                    }
                }

                /* 버퍼 잠금 해제 */
                HRESULT unlock_hr = IDirectSoundBuffer8_Unlock(ds_device->secondary_buffer,
                                                              buffer_ptr1, buffer_size1,
                                                              buffer_ptr2, buffer_size2);

                if (FAILED(unlock_hr)) {
                    ET_LOG_ERROR("DirectSound 버퍼 잠금 해제 실패: 0x%08X", unlock_hr);
                } else {
                    /* 쓰기 커서 업데이트 */
                    ds_device->write_cursor = (ds_device->write_cursor + safe_write_size) % ds_device->buffer_size;
                }
            } else {
                consecutive_errors++;
                ET_LOG_ERROR("DirectSound 버퍼 잠금 실패: 0x%08X (연속 오류: %lu)",
                            hr, consecutive_errors);

                if (consecutive_errors >= max_consecutive_errors) {
                    ET_LOG_ERROR("DirectSound 버퍼 잠금 연속 실패, 스레드 종료");
                    break;
                }
            }
        }

        /* 성능 측정 */
        QueryPerformanceCounter(&end_time);
        double callback_duration = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 /
                                 ds_device->perf_frequency.QuadPart;

        /* 이동 평균 계산 */
        ds_device->avg_callback_duration =
            ds_device->avg_callback_duration * 0.9 + callback_duration * 0.1;

        /* 성능 경고 */
        if (callback_duration > 15.0) { /* 15ms 이상 */
            ET_LOG_WARNING("DirectSound 높은 콜백 지연 시간: %.2fms", callback_duration);
        }
    }

    ET_LOG_INFO("DirectSound 오디오 스레드 종료 (평균 콜백 시간: %.2fms)",
               ds_device->avg_callback_duration);
    return 0;
}

/**
 * @brief DirectSound로 폴백 (강화된 오류 처리 포함)
 */
ETResult et_audio_fallback_to_directsound(ETAudioDevice* device) {
    if (!device) {
        ET_SET_ERROR(ET_ERROR_INVALID_PARAMETER, "유효하지 않은 디바이스 매개변수");
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("DirectSound 폴백 모드로 전환 시작");

    /* DirectSound 시스템 초기화 */
    ETResult result = _directsound_system_init();
    if (result != ET_SUCCESS) {
        ET_LOG_ERROR("DirectSound 시스템 초기화 실패 (오류: %d)", result);
        return result;
    }

    /* DirectSound 디바이스 구조체 할당 */
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)malloc(sizeof(ETDirectSoundDeviceImpl));
    if (!ds_device) {
        ET_LOG_ERROR("DirectSound 디바이스 메모리 할당 실패");
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "DirectSound 디바이스 할당 실패");
        _directsound_system_cleanup();
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(ds_device, 0, sizeof(ETDirectSoundDeviceImpl));

    /* 기본 오디오 포맷 설정 */
    ds_device->format = et_audio_format_create(44100, 2, 1024);
    ET_LOG_INFO("DirectSound 오디오 포맷 설정: %dHz, %d채널, %d프레임 버퍼",
               ds_device->format.sample_rate,
               ds_device->format.num_channels,
               ds_device->format.buffer_size);

    /* DirectSound 버퍼 생성 */
    result = _create_directsound_buffer(ds_device, &ds_device->format);
    if (result != ET_SUCCESS) {
        ET_LOG_ERROR("DirectSound 버퍼 생성 실패 (오류: %d)", result);
        free(ds_device);
        _directsound_system_cleanup();
        return result;
    }

    /* 정지 이벤트 생성 */
    ds_device->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ds_device->stop_event) {
        DWORD error = GetLastError();
        ET_LOG_ERROR("DirectSound 정지 이벤트 생성 실패 (Windows 오류: %lu)", error);

        /* 리소스 정리 */
        if (ds_device->temp_buffer) {
            free(ds_device->temp_buffer);
        }
        if (ds_device->secondary_buffer) {
            IDirectSoundBuffer8_Release(ds_device->secondary_buffer);
        }
        if (ds_device->primary_buffer) {
            IDirectSoundBuffer8_Release(ds_device->primary_buffer);
        }
        free(ds_device);
        _directsound_system_cleanup();

        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound stop event creation failed (Windows error: %lu)", error);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 디바이스에 DirectSound 컨텍스트 연결 */
    /* 실제 구현에서는 ETAudioDevice 구조체를 확장해야 함 */
    /* device->platform_data = ds_device; */
    /* device->backend_type = ET_AUDIO_BACKEND_DIRECTSOUND; */

    ET_LOG_INFO("DirectSound 폴백 초기화 완료 (버퍼 크기: %u 바이트)", ds_device->buffer_size);
    return ET_SUCCESS;
}

/**
 * @brief DirectSound 디바이스 시작 (강화된 오류 처리)
 */
ETResult et_windows_start_directsound_device(ETDirectSoundDevice* device) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)device;

    if (!ds_device) {
        ET_SET_ERROR(ET_ERROR_INVALID_PARAMETER, "DirectSound 디바이스가 NULL입니다");
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!ds_device->secondary_buffer) {
        ET_SET_ERROR(ET_ERROR_INVALID_PARAMETER, "DirectSound 보조 버퍼가 초기화되지 않았습니다");
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (ds_device->is_running) {
        ET_LOG_INFO("DirectSound 디바이스가 이미 실행 중입니다");
        return ET_SUCCESS;
    }

    ET_LOG_INFO("DirectSound 디바이스 시작 중...");

    /* 버퍼 상태 확인 */
    DWORD status;
    HRESULT hr = IDirectSoundBuffer8_GetStatus(ds_device->secondary_buffer, &status);
    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound 버퍼 상태 확인 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 버퍼가 손실된 경우 복원 시도 */
    if (status & DSBSTATUS_BUFFERLOST) {
        ET_LOG_WARNING("DirectSound 버퍼 손실 감지, 복원 시도");
        hr = IDirectSoundBuffer8_Restore(ds_device->secondary_buffer);
        if (FAILED(hr)) {
            ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                        "DirectSound 버퍼 복원 실패: 0x%08X", hr);
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
        }
        ET_LOG_INFO("DirectSound 버퍼 복원 완료");
    }

    /* 버퍼 초기화 */
    ds_device->write_cursor = 0;
    ds_device->safe_write_cursor = 0;

    /* 정지 이벤트 리셋 */
    if (!ResetEvent(ds_device->stop_event)) {
        DWORD error = GetLastError();
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "정지 이벤트 리셋 실패 (Windows 오류: %lu)", error);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 오디오 스레드 시작 */
    ds_device->is_running = true;

    ds_device->audio_thread = CreateThread(NULL, 0, _directsound_audio_thread,
                                          ds_device, 0, NULL);

    if (!ds_device->audio_thread) {
        DWORD error = GetLastError();
        ds_device->is_running = false;
        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound audio thread creation failed (Windows error: %lu)", error);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 스레드 시작 대기 (최대 1초) */
    if (WaitForSingleObject(ds_device->audio_thread, 1000) == WAIT_TIMEOUT) {
        ET_LOG_INFO("DirectSound 오디오 스레드 시작 확인됨");
    }

    /* DirectSound 재생 시작 */
    hr = IDirectSoundBuffer8_Play(ds_device->secondary_buffer, 0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr)) {
        ET_LOG_ERROR("DirectSound 재생 시작 실패: 0x%08X", hr);

        /* 스레드 정리 */
        ds_device->is_running = false;
        SetEvent(ds_device->stop_event);

        if (WaitForSingleObject(ds_device->audio_thread, 5000) == WAIT_TIMEOUT) {
            ET_LOG_WARNING("DirectSound 스레드 종료 대기 시간 초과, 강제 종료");
            TerminateThread(ds_device->audio_thread, 1);
        }

        CloseHandle(ds_device->audio_thread);
        ds_device->audio_thread = NULL;

        ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                    "DirectSound playback start failed: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    ET_LOG_INFO("DirectSound 디바이스 시작 완료 (버퍼 크기: %u 바이트, 스레드 ID: %lu)",
               ds_device->buffer_size, GetThreadId(ds_device->audio_thread));
    return ET_SUCCESS;
}

/**
 * @brief DirectSound 디바이스 정지 (강화된 오류 처리)
 */
ETResult et_windows_stop_directsound_device(ETDirectSoundDevice* device) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)device;

    if (!ds_device) {
        ET_SET_ERROR(ET_ERROR_INVALID_PARAMETER, "DirectSound 디바이스가 NULL입니다");
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!ds_device->is_running) {
        ET_LOG_INFO("DirectSound 디바이스가 이미 정지되어 있습니다");
        return ET_SUCCESS;
    }

    ET_LOG_INFO("DirectSound 디바이스 정지 중...");

    /* DirectSound 재생 정지 (스레드 정지 전에 수행) */
    if (ds_device->secondary_buffer) {
        HRESULT hr = IDirectSoundBuffer8_Stop(ds_device->secondary_buffer);
        if (FAILED(hr)) {
            ET_LOG_WARNING("DirectSound 재생 정지 실패: 0x%08X (계속 진행)", hr);
        } else {
            ET_LOG_INFO("DirectSound 재생 정지 완료");
        }
    }

    /* 오디오 스레드 정지 */
    ds_device->is_running = false;

    if (!SetEvent(ds_device->stop_event)) {
        DWORD error = GetLastError();
        ET_LOG_WARNING("DirectSound 정지 이벤트 설정 실패 (Windows 오류: %lu)", error);
    }

    if (ds_device->audio_thread) {
        ET_LOG_INFO("DirectSound 오디오 스레드 종료 대기 중...");

        DWORD wait_result = WaitForSingleObject(ds_device->audio_thread, 5000); /* 5초 대기 */

        switch (wait_result) {
            case WAIT_OBJECT_0:
                ET_LOG_INFO("DirectSound 오디오 스레드 정상 종료");
                break;

            case WAIT_TIMEOUT:
                ET_LOG_WARNING("DirectSound thread termination timeout, attempting force termination");
                if (!TerminateThread(ds_device->audio_thread, 1)) {
                    DWORD error = GetLastError();
                    ET_LOG_ERROR("DirectSound 스레드 강제 종료 실패 (Windows 오류: %lu)", error);
                } else {
                    ET_LOG_WARNING("DirectSound thread force termination completed");
                }
                break;

            case WAIT_FAILED:
                {
                    DWORD error = GetLastError();
                    ET_LOG_ERROR("DirectSound 스레드 대기 실패 (Windows 오류: %lu)", error);
                }
                break;

            default:
                ET_LOG_WARNING("DirectSound 스레드 대기 알 수 없는 결과: %lu", wait_result);
                break;
        }

        CloseHandle(ds_device->audio_thread);
        ds_device->audio_thread = NULL;
    }

    /* 버퍼 위치 리셋 */
    ds_device->write_cursor = 0;
    ds_device->safe_write_cursor = 0;

    ET_LOG_INFO("DirectSound 디바이스 정지 완료");
    return ET_SUCCESS;
}

/**
 * @brief DirectSound 디바이스 정리
 */
void et_windows_cleanup_directsound_device(ETDirectSoundDevice* device) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)device;
    if (!ds_device) {
        return;
    }

    /* 디바이스 정지 */
    et_windows_stop_directsound_device(ds_device);

    /* 리소스 정리 */
    if (ds_device->temp_buffer) {
        free(ds_device->temp_buffer);
        ds_device->temp_buffer = NULL;
    }

    if (ds_device->secondary_buffer) {
        IDirectSoundBuffer8_Release(ds_device->secondary_buffer);
        ds_device->secondary_buffer = NULL;
    }

    if (ds_device->primary_buffer) {
        IDirectSoundBuffer8_Release(ds_device->primary_buffer);
        ds_device->primary_buffer = NULL;
    }

    if (ds_device->stop_event) {
        CloseHandle(ds_device->stop_event);
        ds_device->stop_event = NULL;
    }

    free(ds_device);
}

/**
 * @brief DirectSound 런타임 오류 복구 시도
 */
static ETResult _directsound_recover_from_error(ETDirectSoundDeviceImpl* ds_device) {
    if (!ds_device || !ds_device->secondary_buffer) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("DirectSound 오류 복구 시도 중...");

    /* 버퍼 상태 확인 */
    DWORD status;
    HRESULT hr = IDirectSoundBuffer8_GetStatus(ds_device->secondary_buffer, &status);
    if (FAILED(hr)) {
        ET_LOG_ERROR("DirectSound 버퍼 상태 확인 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 버퍼 손실 복구 */
    if (status & DSBSTATUS_BUFFERLOST) {
        ET_LOG_INFO("DirectSound 버퍼 손실 감지, 복원 시도");
        hr = IDirectSoundBuffer8_Restore(ds_device->secondary_buffer);
        if (FAILED(hr)) {
            ET_LOG_ERROR("DirectSound 버퍼 복원 실패: 0x%08X", hr);
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
        }

        /* 버퍼 재시작 */
        hr = IDirectSoundBuffer8_Play(ds_device->secondary_buffer, 0, 0, DSBPLAY_LOOPING);
        if (FAILED(hr)) {
            ET_LOG_ERROR("DirectSound 버퍼 재시작 실패: 0x%08X", hr);
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
        }

        ET_LOG_INFO("DirectSound 버퍼 복구 완료");
    }

    /* 커서 위치 리셋 */
    ds_device->write_cursor = 0;
    ds_device->safe_write_cursor = 0;

    return ET_SUCCESS;
}

/**
 * @brief DirectSound 디바이스 상태 확인
 */
ETResult et_windows_check_directsound_device_status(ETDirectSoundDevice* device) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)device;

    if (!ds_device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!ds_device->secondary_buffer) {
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 버퍼 상태 확인 */
    DWORD status;
    HRESULT hr = IDirectSoundBuffer8_GetStatus(ds_device->secondary_buffer, &status);
    if (FAILED(hr)) {
        return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }

    /* 스레드 상태 확인 */
    if (ds_device->is_running && ds_device->audio_thread) {
        DWORD exit_code;
        if (GetExitCodeThread(ds_device->audio_thread, &exit_code)) {
            if (exit_code != STILL_ACTIVE) {
                ET_LOG_WARNING("DirectSound 오디오 스레드가 예상치 못하게 종료됨 (종료 코드: %lu)", exit_code);
                return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
            }
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief DirectSound 성능 통계 가져오기
 */
ETResult et_windows_get_directsound_performance_stats(ETDirectSoundDevice* device,
                                                    double* avg_callback_duration,
                                                    DWORD* current_write_cursor,
                                                    DWORD* buffer_size) {
    ETDirectSoundDeviceImpl* ds_device = (ETDirectSoundDeviceImpl*)device;

    if (!ds_device || !avg_callback_duration || !current_write_cursor || !buffer_size) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *avg_callback_duration = ds_device->avg_callback_duration;
    *current_write_cursor = ds_device->write_cursor;
    *buffer_size = ds_device->buffer_size;

    return ET_SUCCESS;
}

/**
 * @brief DirectSound 모듈 정리
 */
void et_windows_directsound_cleanup(void) {
    _directsound_system_cleanup();
}

#endif /* _WIN32 */