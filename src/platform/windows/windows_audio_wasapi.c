/**
 * @file windows_audio_wasapi.c
 * @brief Windows WASAPI 오디오 백엔드 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows Audio Session API (WASAPI)를 사용한 저지연 오디오 출력 구현
 * DirectSound 폴백 메커니즘 포함
 */

#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32

/* COM 인터페이스 포함 */
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <avrt.h> /* 저지연 스레드 지원 */

/* COM 인터페이스 함수 포인터 정의 */
#define COBJMACROS
#include <initguid.h>

/* WASAPI 전역 상태 */
static struct {
    bool initialized;
    IMMDeviceEnumerator* device_enumerator;
    bool com_initialized;
} g_wasapi_state = { false };

/* WASAPI 디바이스 구조체 확장 (내부 구현) */
struct ETWASAPIDevice {
    ETWASAPIContext wasapi;
    ETAudioFormat format;
    ETAudioCallback callback;
    void* user_data;

    /* 스레딩 관련 */
    HANDLE audio_thread;
    HANDLE stop_event;
    HANDLE buffer_event;
    bool is_running;

    /* 버퍼 관리 */
    UINT32 buffer_frame_count;
    UINT32 current_padding;
    float* temp_buffer;

    /* 성능 모니터링 */
    LARGE_INTEGER perf_frequency;
    LARGE_INTEGER last_callback_time;
    double avg_callback_duration;

    /* 오디오 세션 관리 */
    IAudioSessionControl* session_control;
    IAudioSessionControl2* session_control2;
    ISimpleAudioVolume* simple_volume;
    IAudioEndpointVolume* endpoint_volume;

    /* 볼륨 제어 */
    float current_volume;
    bool is_muted;
    bool volume_control_enabled;

    /* 세션 상태 */
    AudioSessionState session_state;
    GUID session_guid;
};

/**
 * @brief WASAPI 시스템 초기화
 */
static ETResult _wasapi_system_init(void) {
    if (g_wasapi_state.initialized) {
        return ET_SUCCESS;
    }

    /* COM 초기화 */
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "COM 초기화 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }
    g_wasapi_state.com_initialized = (hr != RPC_E_CHANGED_MODE);

    /* 디바이스 열거자 생성 */
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator,
                         (void**)&g_wasapi_state.device_enumerator);

    if (FAILED(hr)) {
        if (g_wasapi_state.com_initialized) {
            CoUninitialize();
        }
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "WASAPI 디바이스 열거자 생성 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    g_wasapi_state.initialized = true;

    ET_LOG_INFO("WASAPI system initialization completed");
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 시스템 정리
 */
static void _wasapi_system_cleanup(void) {
    if (!g_wasapi_state.initialized) {
        return;
    }

    if (g_wasapi_state.device_enumerator) {
        IMMDeviceEnumerator_Release(g_wasapi_state.device_enumerator);
        g_wasapi_state.device_enumerator = NULL;
    }

    if (g_wasapi_state.com_initialized) {
        CoUninitialize();
        g_wasapi_state.com_initialized = false;
    }

    g_wasapi_state.initialized = false;

    ET_LOG_INFO("WASAPI system cleanup completed");
}

/**
 * @brief 기본 오디오 디바이스 가져오기
 */
static ETResult _get_default_audio_device(IMMDevice** device) {
    if (!g_wasapi_state.device_enumerator || !device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        g_wasapi_state.device_enumerator,
        eRender,  /* 출력 디바이스 */
        eConsole, /* 콘솔 역할 */
        device
    );

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "기본 오디오 디바이스 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    return ET_SUCCESS;
}

/**
 * @brief 오디오 포맷을 WAVEFORMATEX로 변환
 */
static void _convert_to_waveformat(const ETAudioFormat* et_format,
                                  WAVEFORMATEXTENSIBLE* wave_format) {
    memset(wave_format, 0, sizeof(WAVEFORMATEXTENSIBLE));

    wave_format->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave_format->Format.nChannels = et_format->num_channels;
    wave_format->Format.nSamplesPerSec = et_format->sample_rate;
    wave_format->Format.wBitsPerSample = 32; /* float32 사용 */
    wave_format->Format.nBlockAlign = et_format->num_channels * sizeof(float);
    wave_format->Format.nAvgBytesPerSec =
        et_format->sample_rate * wave_format->Format.nBlockAlign;
    wave_format->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    wave_format->Samples.wValidBitsPerSample = 32;
    wave_format->dwChannelMask = (et_format->num_channels == 1) ?
        SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    wave_format->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}

/**
 * @brief 오디오 세션 이벤트 핸들러
 */
typedef struct {
    IAudioSessionEventsVtbl* lpVtbl;
    LONG ref_count;
    ETWASAPIDevice* wasapi_device;
} ETAudioSessionEvents;

/* 오디오 세션 이벤트 핸들러 구현 */
static HRESULT STDMETHODCALLTYPE _session_events_query_interface(
    IAudioSessionEvents* this, REFIID riid, void** ppvObject) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAudioSessionEvents)) {
        *ppvObject = this;
        IAudioSessionEvents_AddRef(this);
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE _session_events_add_ref(IAudioSessionEvents* this) {
    ETAudioSessionEvents* events = (ETAudioSessionEvents*)this;
    return InterlockedIncrement(&events->ref_count);
}

static ULONG STDMETHODCALLTYPE _session_events_release(IAudioSessionEvents* this) {
    ETAudioSessionEvents* events = (ETAudioSessionEvents*)this;
    ULONG ref = InterlockedDecrement(&events->ref_count);
    if (ref == 0) {
        free(events);
    }
    return ref;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_display_name_changed(
    IAudioSessionEvents* this, LPCWSTR NewDisplayName, LPCGUID EventContext) {
    ET_LOG_INFO("Audio session display name changed: %ls", NewDisplayName);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_icon_path_changed(
    IAudioSessionEvents* this, LPCWSTR NewIconPath, LPCGUID EventContext) {
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_simple_volume_changed(
    IAudioSessionEvents* this, float NewVolume, BOOL NewMute, LPCGUID EventContext) {
    ETAudioSessionEvents* events = (ETAudioSessionEvents*)this;
    if (events->wasapi_device) {
        events->wasapi_device->current_volume = NewVolume;
        events->wasapi_device->is_muted = NewMute ? true : false;
        ET_LOG_INFO("Volume changed: %.2f%%, Mute: %s",
                   NewVolume * 100.0f, NewMute ? "Yes" : "No");
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_channel_volume_changed(
    IAudioSessionEvents* this, DWORD ChannelCount, float NewChannelVolumeArray[],
    DWORD ChangedChannel, LPCGUID EventContext) {
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_grouping_param_changed(
    IAudioSessionEvents* this, LPCGUID NewGroupingParam, LPCGUID EventContext) {
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_state_changed(
    IAudioSessionEvents* this, AudioSessionState NewState) {
    ETAudioSessionEvents* events = (ETAudioSessionEvents*)this;
    if (events->wasapi_device) {
        events->wasapi_device->session_state = NewState;
        const char* state_name = "알 수 없음";
        switch (NewState) {
            case AudioSessionStateInactive: state_name = "Inactive"; break;
            case AudioSessionStateActive: state_name = "Active"; break;
            case AudioSessionStateExpired: state_name = "Expired"; break;
        }
        ET_LOG_INFO("Audio session state changed: %s", state_name);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE _session_events_on_session_disconnected(
    IAudioSessionEvents* this, AudioSessionDisconnectReason DisconnectReason) {
    const char* reason_name = "알 수 없음";
    switch (DisconnectReason) {
        case DisconnectReasonDeviceRemoval: reason_name = "Device Removal"; break;
        case DisconnectReasonServerShutdown: reason_name = "Server Shutdown"; break;
        case DisconnectReasonFormatChanged: reason_name = "Format Changed"; break;
        case DisconnectReasonSessionLogoff: reason_name = "Session Logoff"; break;
        case DisconnectReasonSessionDisconnected: reason_name = "Session Disconnected"; break;
        case DisconnectReasonExclusiveModeOverride: reason_name = "Exclusive Mode Override"; break;
    }
    ET_LOG_WARNING("Audio session disconnected: %s", reason_name);
    return S_OK;
}

static IAudioSessionEventsVtbl session_events_vtbl = {
    _session_events_query_interface,
    _session_events_add_ref,
    _session_events_release,
    _session_events_on_display_name_changed,
    _session_events_on_icon_path_changed,
    _session_events_on_simple_volume_changed,
    _session_events_on_channel_volume_changed,
    _session_events_on_grouping_param_changed,
    _session_events_on_state_changed,
    _session_events_on_session_disconnected
};

/**
 * @brief 오디오 세션 관리 초기화
 */
static ETResult _initialize_audio_session(ETWASAPIDevice* wasapi_device) {
    HRESULT hr;

    /* 오디오 세션 관리자 가져오기 */
    IAudioSessionManager* session_manager = NULL;
    hr = IMMDevice_Activate(wasapi_device->wasapi.audio_device,
                           &IID_IAudioSessionManager, CLSCTX_ALL, NULL,
                           (void**)&session_manager);

    if (FAILED(hr)) {
        ET_LOG_ERROR("Failed to get audio session manager: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 세션 제어 가져오기 */
    hr = IAudioSessionManager_GetAudioSessionControl(
        session_manager, NULL, 0, &wasapi_device->session_control);

    if (FAILED(hr)) {
        IAudioSessionManager_Release(session_manager);
        ET_LOG_ERROR("Failed to get audio session control: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 확장된 세션 제어 인터페이스 가져오기 */
    hr = IAudioSessionControl_QueryInterface(wasapi_device->session_control,
                                            &IID_IAudioSessionControl2,
                                            (void**)&wasapi_device->session_control2);

    if (SUCCEEDED(hr)) {
        /* 세션 GUID 가져오기 */
        LPWSTR session_instance_id = NULL;
        if (SUCCEEDED(IAudioSessionControl2_GetSessionInstanceIdentifier(
            wasapi_device->session_control2, &session_instance_id))) {
            ET_LOG_INFO("Audio session ID: %ls", session_instance_id);
            CoTaskMemFree(session_instance_id);
        }
    }

    /* 간단한 볼륨 제어 가져오기 */
    hr = IAudioSessionManager_GetSimpleAudioVolume(
        session_manager, NULL, 0, &wasapi_device->simple_volume);

    if (FAILED(hr)) {
        ET_LOG_WARNING("Failed to get simple volume control: 0x%08X", hr);
    } else {
        /* Get current volume state */
        float volume;
        BOOL mute;
        if (SUCCEEDED(ISimpleAudioVolume_GetMasterVolume(wasapi_device->simple_volume, &volume)) &&
            SUCCEEDED(ISimpleAudioVolume_GetMute(wasapi_device->simple_volume, &mute))) {
            wasapi_device->current_volume = volume;
            wasapi_device->is_muted = mute ? true : false;
            wasapi_device->volume_control_enabled = true;
            ET_LOG_INFO("Current volume: %.2f%%, Mute: %s",
                       volume * 100.0f, mute ? "Yes" : "No");
        }
    }

    /* 엔드포인트 볼륨 제어 가져오기 */
    hr = IMMDevice_Activate(wasapi_device->wasapi.audio_device,
                           &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                           (void**)&wasapi_device->endpoint_volume);

    if (FAILED(hr)) {
        ET_LOG_WARNING("Failed to get endpoint volume control: 0x%08X", hr);
    }

    /* 세션 이벤트 핸들러 등록 */
    ETAudioSessionEvents* session_events = (ETAudioSessionEvents*)malloc(sizeof(ETAudioSessionEvents));
    if (session_events) {
        session_events->lpVtbl = &session_events_vtbl;
        session_events->ref_count = 1;
        session_events->wasapi_device = wasapi_device;

        hr = IAudioSessionControl_RegisterAudioSessionNotification(
            wasapi_device->session_control, (IAudioSessionEvents*)session_events);

        if (FAILED(hr)) {
            ET_LOG_WARNING("Failed to register session event handler: 0x%08X", hr);
            free(session_events);
        } else {
            ET_LOG_INFO("Audio session event handler registration completed");
        }
    }

    /* 세션 표시 이름 설정 */
    hr = IAudioSessionControl_SetDisplayName(wasapi_device->session_control,
                                            L"LibEtude Audio Engine", NULL);
    if (FAILED(hr)) {
        ET_LOG_WARNING("Failed to set session display name: 0x%08X", hr);
    }

    IAudioSessionManager_Release(session_manager);

    ET_LOG_INFO("Audio session management initialization completed");
    return ET_SUCCESS;
}

/**
 * @brief 오디오 클라이언트 초기화
 */
static ETResult _initialize_audio_client(ETWASAPIDevice* wasapi_device,
                                       IMMDevice* device,
                                       const ETAudioFormat* format) {
    HRESULT hr;

    /* 오디오 클라이언트 활성화 */
    hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL,
                           (void**)&wasapi_device->wasapi.audio_client);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 클라이언트 활성화 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 포맷 설정 */
    WAVEFORMATEXTENSIBLE wave_format;
    _convert_to_waveformat(format, &wave_format);

    /* 공유 모드로 초기화 시도 */
    REFERENCE_TIME buffer_duration = format->buffer_size * 10000000LL / format->sample_rate;

    hr = IAudioClient_Initialize(
        wasapi_device->wasapi.audio_client,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        buffer_duration,
        0, /* 공유 모드에서는 0 */
        (WAVEFORMATEX*)&wave_format,
        NULL
    );

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 클라이언트 초기화 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 버퍼 크기 가져오기 */
    hr = IAudioClient_GetBufferSize(wasapi_device->wasapi.audio_client,
                                   &wasapi_device->buffer_frame_count);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "버퍼 크기 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 이벤트 핸들 생성 */
    wasapi_device->wasapi.audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wasapi_device->wasapi.audio_event) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 이벤트 생성 실패");
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 이벤트 핸들 설정 */
    hr = IAudioClient_SetEventHandle(wasapi_device->wasapi.audio_client,
                                    wasapi_device->wasapi.audio_event);

    if (FAILED(hr)) {
        CloseHandle(wasapi_device->wasapi.audio_event);
        wasapi_device->wasapi.audio_event = NULL;
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "이벤트 핸들 설정 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 렌더 클라이언트 가져오기 */
    hr = IAudioClient_GetService(wasapi_device->wasapi.audio_client,
                                &IID_IAudioRenderClient,
                                (void**)&wasapi_device->wasapi.render_client);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "렌더 클라이언트 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 임시 버퍼 할당 */
    wasapi_device->temp_buffer = (float*)malloc(
        wasapi_device->buffer_frame_count * format->num_channels * sizeof(float));

    if (!wasapi_device->temp_buffer) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "임시 버퍼 할당 실패");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    /* 성능 카운터 초기화 */
    QueryPerformanceFrequency(&wasapi_device->perf_frequency);
    wasapi_device->avg_callback_duration = 0.0;

    /* 오디오 세션 관리 초기화 */
    ETResult session_result = _initialize_audio_session(wasapi_device);
    if (session_result != ET_SUCCESS) {
        ET_LOG_WARNING("Audio session management initialization failed, using basic functionality only");
    }

    ET_LOG_INFO("WASAPI audio client initialization completed (buffer size: %u frames)",
               wasapi_device->buffer_frame_count);

    return ET_SUCCESS;
}

/**
 * @brief 저지연 오디오 렌더링 루프 구현
 */
static ETResult _wasapi_render_audio_data(ETWASAPIDevice* wasapi_device) {
    HRESULT hr;
    LARGE_INTEGER start_time, end_time;
    QueryPerformanceCounter(&start_time);

    /* 현재 패딩 확인 */
    hr = IAudioClient_GetCurrentPadding(
        wasapi_device->wasapi.audio_client,
        &wasapi_device->current_padding);

    if (FAILED(hr)) {
        ET_LOG_ERROR("패딩 정보 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 사용 가능한 프레임 수 계산 */
    UINT32 available_frames = wasapi_device->buffer_frame_count -
                             wasapi_device->current_padding;

    /* 최소 프레임 수 확인 (언더런 방지) */
    UINT32 min_frames = wasapi_device->buffer_frame_count / 4;
    if (available_frames < min_frames) {
        return ET_SUCCESS; /* 충분한 버퍼가 없으면 대기 */
    }

    /* 버퍼 가져오기 */
    BYTE* buffer_data;
    hr = IAudioRenderClient_GetBuffer(
        wasapi_device->wasapi.render_client,
        available_frames,
        &buffer_data);

    if (FAILED(hr)) {
        ET_LOG_ERROR("버퍼 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 데이터 생성 */
    if (wasapi_device->callback) {
        /* 임시 버퍼 초기화 */
        memset(wasapi_device->temp_buffer, 0,
              available_frames * wasapi_device->format.num_channels * sizeof(float));

        /* 콜백 호출하여 오디오 데이터 생성 */
        wasapi_device->callback(wasapi_device->temp_buffer,
                              available_frames,
                              wasapi_device->user_data);

        /* 데이터 복사 (float32 -> float32) */
        memcpy(buffer_data, wasapi_device->temp_buffer,
              available_frames * wasapi_device->format.num_channels * sizeof(float));
    } else {
        /* 콜백이 없으면 무음 출력 */
        memset(buffer_data, 0,
              available_frames * wasapi_device->format.num_channels * sizeof(float));
    }

    /* 버퍼 해제 */
    hr = IAudioRenderClient_ReleaseBuffer(
        wasapi_device->wasapi.render_client,
        available_frames,
        0);

    if (FAILED(hr)) {
        ET_LOG_ERROR("버퍼 해제 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 성능 측정 및 통계 업데이트 */
    QueryPerformanceCounter(&end_time);
    double callback_duration = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 /
                             wasapi_device->perf_frequency.QuadPart;

    /* 이동 평균 계산 (저지연 모니터링) */
    wasapi_device->avg_callback_duration =
        wasapi_device->avg_callback_duration * 0.95 + callback_duration * 0.05;

    wasapi_device->last_callback_time = end_time;

    /* 지연 시간 경고 */
    if (callback_duration > 10.0) { /* 10ms 이상 */
        ET_LOG_WARNING("높은 오디오 콜백 지연 시간: %.2fms", callback_duration);
    }

    return ET_SUCCESS;
}

/**
 * @brief WASAPI 오디오 스레드 함수 (저지연 최적화)
 */
static DWORD WINAPI _wasapi_audio_thread(LPVOID param) {
    ETWASAPIDevice* wasapi_device = (ETWASAPIDevice*)param;
    HANDLE events[2] = { wasapi_device->stop_event, wasapi_device->wasapi.audio_event };

    /* 스레드 우선순위를 최고로 설정 (저지연을 위해) */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    /* 스레드 스케줄링 클래스 설정 */
    DWORD task_index = 0;
    HANDLE task_handle = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &task_index);
    if (!task_handle) {
        ET_LOG_WARNING("Pro Audio 스레드 특성 설정 실패, 기본 우선순위 사용");
    }

    ET_LOG_INFO("WASAPI low-latency audio thread started (buffer: %u frames)",
               wasapi_device->buffer_frame_count);

    /* 초기 버퍼 채우기 (언더런 방지) */
    for (int i = 0; i < 2; i++) {
        if (_wasapi_render_audio_data(wasapi_device) != ET_SUCCESS) {
            ET_LOG_ERROR("초기 버퍼 채우기 실패");
            break;
        }
    }

    /* 메인 렌더링 루프 */
    while (wasapi_device->is_running) {
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, 100); /* 100ms 타임아웃 */

        if (wait_result == WAIT_OBJECT_0) {
            /* 정지 이벤트 */
            break;
        } else if (wait_result == WAIT_OBJECT_0 + 1) {
            /* 오디오 이벤트 - 버퍼가 준비됨 */
            if (_wasapi_render_audio_data(wasapi_device) != ET_SUCCESS) {
                ET_LOG_ERROR("오디오 렌더링 실패, 스레드 종료");
                break;
            }
        } else if (wait_result == WAIT_TIMEOUT) {
            /* 타임아웃 - 강제로 버퍼 상태 확인 (언더런 복구) */
            UINT32 current_padding;
            HRESULT hr = IAudioClient_GetCurrentPadding(
                wasapi_device->wasapi.audio_client, &current_padding);

            if (SUCCEEDED(hr) && current_padding < wasapi_device->buffer_frame_count / 2) {
                ET_LOG_WARNING("오디오 버퍼 언더런 감지, 복구 시도");
                _wasapi_render_audio_data(wasapi_device);
            }
        } else {
            /* 오류 발생 */
            ET_LOG_ERROR("WaitForMultipleObjects 실패: %lu", GetLastError());
            break;
        }
    }

    /* 스레드 특성 해제 */
    if (task_handle) {
        AvRevertMmThreadCharacteristics(task_handle);
    }

    ET_LOG_INFO("WASAPI audio thread terminated (average callback time: %.2fms)",
               wasapi_device->avg_callback_duration);
    return 0;
}

/**
 * @brief WASAPI 디바이스 초기화 (DirectSound 폴백 포함)
 */
ETResult et_audio_init_wasapi_with_fallback(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("WASAPI 초기화 시도 중...");

    /* WASAPI 시스템 초기화 */
    ETResult result = _wasapi_system_init();
    if (result != ET_SUCCESS) {
        ET_LOG_WARNING("WASAPI 시스템 초기화 실패 (오류: %d), DirectSound로 폴백", result);
        return et_audio_fallback_to_directsound(device);
    }

    /* WASAPI 디바이스 구조체 할당 */
    ETWASAPIDevice* wasapi_device = (ETWASAPIDevice*)malloc(sizeof(ETWASAPIDevice));
    if (!wasapi_device) {
        ET_LOG_ERROR("WASAPI 디바이스 메모리 할당 실패, DirectSound로 폴백");
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "WASAPI 디바이스 할당 실패");
        return et_audio_fallback_to_directsound(device);
    }

    memset(wasapi_device, 0, sizeof(ETWASAPIDevice));

    /* 기본 디바이스 가져오기 */
    result = _get_default_audio_device(&wasapi_device->wasapi.audio_device);
    if (result != ET_SUCCESS) {
        free(wasapi_device);
        ET_LOG_WARNING("기본 오디오 디바이스 가져오기 실패 (오류: %d), DirectSound로 폴백", result);
        return et_audio_fallback_to_directsound(device);
    }

    /* 오디오 포맷 설정 (임시로 기본값 사용) */
    wasapi_device->format = et_audio_format_create(44100, 2, 1024);

    /* 오디오 클라이언트 초기화 */
    result = _initialize_audio_client(wasapi_device,
                                    wasapi_device->wasapi.audio_device,
                                    &wasapi_device->format);

    if (result != ET_SUCCESS) {
        ET_LOG_WARNING("WASAPI 오디오 클라이언트 초기화 실패 (오류: %d), DirectSound로 폴백", result);

        /* WASAPI 리소스 정리 */
        if (wasapi_device->wasapi.audio_device) {
            IMMDevice_Release(wasapi_device->wasapi.audio_device);
        }
        free(wasapi_device);

        return et_audio_fallback_to_directsound(device);
    }

    /* 이벤트 생성 */
    wasapi_device->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!wasapi_device->stop_event) {
        ET_LOG_WARNING("WASAPI 정지 이벤트 생성 실패, DirectSound로 폴백");

        /* WASAPI 리소스 정리 */
        if (wasapi_device->temp_buffer) {
            free(wasapi_device->temp_buffer);
        }
        if (wasapi_device->wasapi.render_client) {
            IAudioRenderClient_Release(wasapi_device->wasapi.render_client);
        }
        if (wasapi_device->wasapi.audio_client) {
            IAudioClient_Release(wasapi_device->wasapi.audio_client);
        }
        if (wasapi_device->wasapi.audio_event) {
            CloseHandle(wasapi_device->wasapi.audio_event);
        }
        if (wasapi_device->wasapi.audio_device) {
            IMMDevice_Release(wasapi_device->wasapi.audio_device);
        }
        free(wasapi_device);

        return et_audio_fallback_to_directsound(device);
    }

    /* 디바이스에 WASAPI 컨텍스트 연결 */
    /* In actual implementation, ETAudioDevice structure needs to be extended */

    ET_LOG_INFO("WASAPI 디바이스 초기화 성공");
    return ET_SUCCESS;
}

/**
 * @brief 사용 가능한 오디오 디바이스 열거
 */
ETResult et_windows_enumerate_audio_devices(ETWindowsAudioDevice** devices,
                                          uint32_t* device_count) {
    if (!devices || !device_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *devices = NULL;
    *device_count = 0;

    /* WASAPI 시스템 초기화 */
    ETResult result = _wasapi_system_init();
    if (result != ET_SUCCESS) {
        return result;
    }

    /* 디바이스 컬렉션 가져오기 */
    IMMDeviceCollection* device_collection = NULL;
    HRESULT hr = IMMDeviceEnumerator_EnumAudioEndpoints(
        g_wasapi_state.device_enumerator,
        eRender,  /* 출력 디바이스만 */
        DEVICE_STATE_ACTIVE,
        &device_collection);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 디바이스 열거 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 디바이스 개수 가져오기 */
    UINT count = 0;
    hr = IMMDeviceCollection_GetCount(device_collection, &count);
    if (FAILED(hr) || count == 0) {
        IMMDeviceCollection_Release(device_collection);
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "디바이스 개수 가져오기 실패 또는 디바이스 없음");
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 디바이스 정보 배열 할당 */
    ETWindowsAudioDevice* device_array = (ETWindowsAudioDevice*)malloc(
        count * sizeof(ETWindowsAudioDevice));

    if (!device_array) {
        IMMDeviceCollection_Release(device_collection);
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "디바이스 배열 할당 실패");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    /* 기본 디바이스 ID 가져오기 */
    IMMDevice* default_device = NULL;
    LPWSTR default_device_id = NULL;

    if (SUCCEEDED(_get_default_audio_device(&default_device))) {
        IMMDevice_GetId(default_device, &default_device_id);
        IMMDevice_Release(default_device);
    }

    /* 각 디바이스 정보 수집 */
    uint32_t valid_devices = 0;

    for (UINT i = 0; i < count; i++) {
        IMMDevice* device = NULL;
        hr = IMMDeviceCollection_Item(device_collection, i, &device);

        if (FAILED(hr)) {
            continue;
        }

        ETWindowsAudioDevice* dev_info = &device_array[valid_devices];
        memset(dev_info, 0, sizeof(ETWindowsAudioDevice));

        /* 디바이스 ID 가져오기 */
        LPWSTR device_id = NULL;
        if (SUCCEEDED(IMMDevice_GetId(device, &device_id))) {
            wcsncpy_s(dev_info->device_id, 256, device_id, _TRUNCATE);

            /* 기본 디바이스인지 확인 */
            if (default_device_id && wcscmp(device_id, default_device_id) == 0) {
                dev_info->is_default = true;
            }

            CoTaskMemFree(device_id);
        }

        /* 디바이스 속성 가져오기 */
        IPropertyStore* prop_store = NULL;
        if (SUCCEEDED(IMMDevice_OpenPropertyStore(device, STGM_READ, &prop_store))) {
            PROPVARIANT prop_variant;
            PropVariantInit(&prop_variant);

            /* 친화적 이름 가져오기 */
            if (SUCCEEDED(IPropertyStore_GetValue(prop_store, &PKEY_Device_FriendlyName, &prop_variant))) {
                if (prop_variant.vt == VT_LPWSTR) {
                    wcsncpy_s(dev_info->friendly_name, 256, prop_variant.pwszVal, _TRUNCATE);
                }
                PropVariantClear(&prop_variant);
            }

            IPropertyStore_Release(prop_store);
        }

        /* 오디오 클라이언트로 포맷 정보 가져오기 */
        IAudioClient* audio_client = NULL;
        if (SUCCEEDED(IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL,
                                        (void**)&audio_client))) {

            WAVEFORMATEX* mix_format = NULL;
            if (SUCCEEDED(IAudioClient_GetMixFormat(audio_client, &mix_format))) {
                dev_info->sample_rate = mix_format->nSamplesPerSec;
                dev_info->channels = mix_format->nChannels;
                dev_info->bits_per_sample = mix_format->wBitsPerSample;

                /* 독점 모드 지원 확인 */
                HRESULT exclusive_hr = IAudioClient_IsFormatSupported(
                    audio_client, AUDCLNT_SHAREMODE_EXCLUSIVE, mix_format, NULL);
                dev_info->supports_exclusive = SUCCEEDED(exclusive_hr);

                CoTaskMemFree(mix_format);
            }

            IAudioClient_Release(audio_client);
        }

        IMMDevice_Release(device);
        valid_devices++;
    }

    /* 정리 */
    if (default_device_id) {
        CoTaskMemFree(default_device_id);
    }
    IMMDeviceCollection_Release(device_collection);

    if (valid_devices == 0) {
        free(device_array);
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED, "유효한 오디오 디바이스 없음");
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 배열 크기 조정 */
    if (valid_devices < count) {
        ETWindowsAudioDevice* resized_array = (ETWindowsAudioDevice*)realloc(
            device_array, valid_devices * sizeof(ETWindowsAudioDevice));

        if (resized_array) {
            device_array = resized_array;
        }
    }

    *devices = device_array;
    *device_count = valid_devices;

    ET_LOG_INFO("Audio device enumeration completed: %u devices found", valid_devices);
    return ET_SUCCESS;
}

/**
 * @brief 오디오 디바이스 목록 해제
 */
void et_windows_free_audio_devices(ETWindowsAudioDevice* devices) {
    if (devices) {
        free(devices);
    }
}

/**
 * @brief 특정 디바이스로 WASAPI 초기화
 */
ETResult et_windows_init_wasapi_device(const wchar_t* device_id,
                                     const ETAudioFormat* format,
                                     ETWASAPIContext* context) {
    if (!device_id || !format || !context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(context, 0, sizeof(ETWASAPIContext));

    /* WASAPI 시스템 초기화 */
    ETResult result = _wasapi_system_init();
    if (result != ET_SUCCESS) {
        return result;
    }

    /* 특정 디바이스 가져오기 */
    HRESULT hr = IMMDeviceEnumerator_GetDevice(
        g_wasapi_state.device_enumerator,
        device_id,
        &context->audio_device);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "지정된 오디오 디바이스 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 클라이언트 활성화 */
    hr = IMMDevice_Activate(context->audio_device, &IID_IAudioClient, CLSCTX_ALL, NULL,
                           (void**)&context->audio_client);

    if (FAILED(hr)) {
        IMMDevice_Release(context->audio_device);
        context->audio_device = NULL;
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 클라이언트 활성화 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 포맷 설정 */
    WAVEFORMATEXTENSIBLE wave_format;
    _convert_to_waveformat(format, &wave_format);

    /* 포맷 지원 확인 */
    WAVEFORMATEX* closest_match = NULL;
    hr = IAudioClient_IsFormatSupported(context->audio_client,
                                       AUDCLNT_SHAREMODE_SHARED,
                                       (WAVEFORMATEX*)&wave_format,
                                       &closest_match);

    if (hr == S_FALSE && closest_match) {
        ET_LOG_WARNING("요청한 포맷이 정확히 지원되지 않음, 가장 가까운 포맷 사용");
        CoTaskMemFree(closest_match);
    } else if (FAILED(hr)) {
        IAudioClient_Release(context->audio_client);
        IMMDevice_Release(context->audio_device);
        memset(context, 0, sizeof(ETWASAPIContext));
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "오디오 포맷 지원 확인 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    ET_LOG_INFO("WASAPI device initialization completed");
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 컨텍스트 정리
 */
void et_windows_cleanup_wasapi_context(ETWASAPIContext* context) {
    if (!context) {
        return;
    }

    if (context->render_client) {
        IAudioRenderClient_Release(context->render_client);
        context->render_client = NULL;
    }

    if (context->audio_client) {
        IAudioClient_Release(context->audio_client);
        context->audio_client = NULL;
    }

    if (context->audio_event) {
        CloseHandle(context->audio_event);
        context->audio_event = NULL;
    }

    if (context->audio_device) {
        IMMDevice_Release(context->audio_device);
        context->audio_device = NULL;
    }

    context->is_exclusive_mode = false;
}

/**
 * @brief WASAPI 디바이스 볼륨 설정
 */
ETResult et_wasapi_set_volume(ETWASAPIDevice* wasapi_device, float volume) {
    if (!wasapi_device || volume < 0.0f || volume > 1.0f) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!wasapi_device->volume_control_enabled || !wasapi_device->simple_volume) {
        ET_LOG_WARNING("볼륨 제어가 비활성화되어 있음");
        return ET_ERROR_NOT_SUPPORTED;
    }

    HRESULT hr = ISimpleAudioVolume_SetMasterVolume(
        wasapi_device->simple_volume, volume, NULL);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "볼륨 설정 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    wasapi_device->current_volume = volume;
    ET_LOG_INFO("Volume set: %.2f%%", volume * 100.0f);

    return ET_SUCCESS;
}

/**
 * @brief WASAPI 디바이스 볼륨 가져오기
 */
ETResult et_wasapi_get_volume(ETWASAPIDevice* wasapi_device, float* volume) {
    if (!wasapi_device || !volume) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!wasapi_device->volume_control_enabled || !wasapi_device->simple_volume) {
        *volume = wasapi_device->current_volume;
        return ET_SUCCESS;
    }

    HRESULT hr = ISimpleAudioVolume_GetMasterVolume(wasapi_device->simple_volume, volume);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "볼륨 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    wasapi_device->current_volume = *volume;
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 디바이스 음소거 설정
 */
ETResult et_wasapi_set_mute(ETWASAPIDevice* wasapi_device, bool mute) {
    if (!wasapi_device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!wasapi_device->volume_control_enabled || !wasapi_device->simple_volume) {
        ET_LOG_WARNING("볼륨 제어가 비활성화되어 있음");
        return ET_ERROR_NOT_SUPPORTED;
    }

    HRESULT hr = ISimpleAudioVolume_SetMute(
        wasapi_device->simple_volume, mute ? TRUE : FALSE, NULL);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "음소거 설정 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    wasapi_device->is_muted = mute;
    ET_LOG_INFO("Mute setting: %s", mute ? "Yes" : "No");

    return ET_SUCCESS;
}

/**
 * @brief WASAPI 디바이스 음소거 상태 가져오기
 */
ETResult et_wasapi_get_mute(ETWASAPIDevice* wasapi_device, bool* mute) {
    if (!wasapi_device || !mute) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!wasapi_device->volume_control_enabled || !wasapi_device->simple_volume) {
        *mute = wasapi_device->is_muted;
        return ET_SUCCESS;
    }

    BOOL is_muted;
    HRESULT hr = ISimpleAudioVolume_GetMute(wasapi_device->simple_volume, &is_muted);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "음소거 상태 가져오기 실패: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    *mute = is_muted ? true : false;
    wasapi_device->is_muted = *mute;
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 오디오 스트림 시작
 */
ETResult et_wasapi_start_stream(ETWASAPIDevice* wasapi_device,
                               ETAudioCallback callback, void* user_data) {
    if (!wasapi_device || !callback) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (wasapi_device->is_running) {
        ET_LOG_WARNING("Audio stream is already running");
        return ET_SUCCESS;
    }

    /* 콜백 설정 */
    wasapi_device->callback = callback;
    wasapi_device->user_data = user_data;

    /* 오디오 클라이언트 시작 */
    HRESULT hr = IAudioClient_Start(wasapi_device->wasapi.audio_client);
    if (FAILED(hr)) {
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "Audio client start failed: 0x%08X", hr);
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    /* 오디오 스레드 시작 */
    wasapi_device->is_running = true;
    wasapi_device->audio_thread = CreateThread(
        NULL, 0, _wasapi_audio_thread, wasapi_device, 0, NULL);

    if (!wasapi_device->audio_thread) {
        wasapi_device->is_running = false;
        IAudioClient_Stop(wasapi_device->wasapi.audio_client);
        ET_SET_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                    "Audio thread creation failed");
        return ET_WINDOWS_ERROR_WASAPI_INIT_FAILED;
    }

    ET_LOG_INFO("WASAPI audio stream started");
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 오디오 스트림 정지
 */
ETResult et_wasapi_stop_stream(ETWASAPIDevice* wasapi_device) {
    if (!wasapi_device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!wasapi_device->is_running) {
        return ET_SUCCESS;
    }

    /* 스레드 정지 신호 */
    wasapi_device->is_running = false;
    if (wasapi_device->stop_event) {
        SetEvent(wasapi_device->stop_event);
    }

    /* 스레드 종료 대기 */
    if (wasapi_device->audio_thread) {
        WaitForSingleObject(wasapi_device->audio_thread, 5000); /* 5초 대기 */
        CloseHandle(wasapi_device->audio_thread);
        wasapi_device->audio_thread = NULL;
    }

    /* 오디오 클라이언트 정지 */
    if (wasapi_device->wasapi.audio_client) {
        IAudioClient_Stop(wasapi_device->wasapi.audio_client);
    }

    ET_LOG_INFO("WASAPI audio stream stopped");
    return ET_SUCCESS;
}

/**
 * @brief WASAPI 디바이스 정리
 */
void et_wasapi_cleanup_device(ETWASAPIDevice* wasapi_device) {
    if (!wasapi_device) {
        return;
    }

    /* 스트림 정지 */
    et_wasapi_stop_stream(wasapi_device);

    /* 세션 관리 정리 */
    if (wasapi_device->session_control) {
        /* Event handler release is complex, skipped (needs implementation) */
        IAudioSessionControl_Release(wasapi_device->session_control);
        wasapi_device->session_control = NULL;
    }

    if (wasapi_device->session_control2) {
        IAudioSessionControl2_Release(wasapi_device->session_control2);
        wasapi_device->session_control2 = NULL;
    }

    if (wasapi_device->simple_volume) {
        ISimpleAudioVolume_Release(wasapi_device->simple_volume);
        wasapi_device->simple_volume = NULL;
    }

    if (wasapi_device->endpoint_volume) {
        IAudioEndpointVolume_Release(wasapi_device->endpoint_volume);
        wasapi_device->endpoint_volume = NULL;
    }

    /* WASAPI 컨텍스트 정리 */
    et_windows_cleanup_wasapi_context(&wasapi_device->wasapi);

    /* 이벤트 핸들 정리 */
    if (wasapi_device->stop_event) {
        CloseHandle(wasapi_device->stop_event);
        wasapi_device->stop_event = NULL;
    }

    /* 임시 버퍼 해제 */
    if (wasapi_device->temp_buffer) {
        free(wasapi_device->temp_buffer);
        wasapi_device->temp_buffer = NULL;
    }

    /* 구조체 해제 */
    free(wasapi_device);

    ET_LOG_INFO("WASAPI device cleanup completed");
}

/**
 * @brief 오디오 성능 통계 가져오기
 */
ETResult et_wasapi_get_performance_stats(ETWASAPIDevice* wasapi_device,
                                       double* avg_callback_duration,
                                       UINT32* current_padding,
                                       UINT32* buffer_frame_count) {
    if (!wasapi_device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (avg_callback_duration) {
        *avg_callback_duration = wasapi_device->avg_callback_duration;
    }

    if (current_padding) {
        *current_padding = wasapi_device->current_padding;
    }

    if (buffer_frame_count) {
        *buffer_frame_count = wasapi_device->buffer_frame_count;
    }

    return ET_SUCCESS;
}

/**
 * @brief 모듈 정리 함수
 */
void et_windows_wasapi_cleanup(void) {
    _wasapi_system_cleanup();
}

#endif /* _WIN32 */