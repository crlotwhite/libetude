/**
 * @file windows_audio.c
 * @brief Windows 오디오 시스템 구현 (플랫폼 추상화 레이어)
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/platform/audio.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

// Windows 유틸리티 함수 선언
extern ETResult et_windows_error_to_common(int windows_error);
extern const char* et_get_windows_error_description(int windows_error);
extern void et_get_windows_system_error_message(DWORD error_code, char* buffer, size_t buffer_size);

// DirectSound 관련 전역 변수
static LPDIRECTSOUND8 g_direct_sound = NULL;
static bool g_directsound_initialized = false;

// WASAPI 관련 전역 변수
static IMMDeviceEnumerator* g_device_enumerator = NULL;
static bool g_wasapi_initialized = false;

// Windows 오디오 디바이스 구조체
typedef struct ETWindowsAudioDevice {
    ETAudioFormat format;
    ETAudioState state;
    ETAudioCallback callback;
    void* user_data;

    // DirectSound 관련
    LPDIRECTSOUNDBUFFER8 ds_buffer;
    DWORD ds_buffer_size;
    DWORD ds_write_pos;

    // WASAPI 관련
    IMMDevice* wasapi_device;
    IAudioClient* audio_client;
    IAudioRenderClient* render_client;
    IAudioCaptureClient* capture_client;
    HANDLE event_handle;

    // 스레드 관련
    HANDLE audio_thread;
    HANDLE stop_event;
    bool use_wasapi;
    bool is_input;
} ETWindowsAudioDevice;

// ============================================================================
// Windows 오류 코드 매핑
// ============================================================================

/**
 * @brief Windows 오디오 오류를 공통 오류 코드로 매핑
 */
static ETResult windows_audio_error_to_common(HRESULT hr) {
    return et_windows_error_to_common((int)hr);
}

/**
 * @brief MMRESULT 오류를 공통 오류 코드로 매핑
 */
static ETResult windows_mm_error_to_common(MMRESULT result) {
    return et_windows_error_to_common((int)result);
}

// ============================================================================
// Windows 오디오 시스템 초기화/정리
// ============================================================================

/**
 * @brief Windows 오디오 시스템 초기화
 */
static ETResult windows_audio_init(void) {
    if (g_directsound_initialized && g_wasapi_initialized) {
        return ET_SUCCESS;
    }

    // COM 초기화
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "COM 초기화 실패: 0x%08X", hr);
        return ET_ERROR_HARDWARE;
    }

    // DirectSound 초기화
    if (!g_directsound_initialized) {
        hr = DirectSoundCreate8(NULL, &g_direct_sound, NULL);
        if (SUCCEEDED(hr)) {
            HWND hwnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(g_direct_sound, hwnd, DSSCL_PRIORITY);
            if (SUCCEEDED(hr)) {
                g_directsound_initialized = true;
            }
        }
    }

    // WASAPI 디바이스 열거자 초기화
    if (!g_wasapi_initialized) {
        hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                             &IID_IMMDeviceEnumerator, (void**)&g_device_enumerator);
        if (SUCCEEDED(hr)) {
            g_wasapi_initialized = true;
        }
    }

    if (!g_directsound_initialized && !g_wasapi_initialized) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "Windows 오디오 시스템 초기화 실패");
        return ET_ERROR_HARDWARE;
    }

    return ET_SUCCESS;
}

/**
 * @brief Windows 오디오 시스템 정리
 */
static void windows_audio_finalize(void) {
    if (g_direct_sound) {
        IDirectSound8_Release(g_direct_sound);
        g_direct_sound = NULL;
    }
    g_directsound_initialized = false;

    if (g_device_enumerator) {
        IMMDeviceEnumerator_Release(g_device_enumerator);
        g_device_enumerator = NULL;
    }
    g_wasapi_initialized = false;

    CoUninitialize();
}

/**
 * @brief DirectSound 버퍼 생성
 * @param format 오디오 포맷
 * @param buffer_size 버퍼 크기 (바이트)
 * @param secondary_buffer 생성된 보조 버퍼 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
static ETResult windows_create_directsound_buffer(const ETAudioFormat* format,
                                                DWORD buffer_size,
                                                LPDIRECTSOUNDBUFFER8* secondary_buffer) {
    if (!g_directsound_initialized || !g_direct_sound) {
        return ET_ERROR_HARDWARE;
    }

    // 버퍼 설명 구조체 설정
    DSBUFFERDESC buffer_desc;
    ZeroMemory(&buffer_desc, sizeof(buffer_desc));
    buffer_desc.dwSize = sizeof(buffer_desc);
    buffer_desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_GLOBALFOCUS;
    buffer_desc.dwBufferBytes = buffer_size;

    // Wave 포맷 설정
    WAVEFORMATEX wave_format;
    ZeroMemory(&wave_format, sizeof(wave_format));
    wave_format.wFormatTag = format->is_float ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wave_format.nChannels = format->num_channels;
    wave_format.nSamplesPerSec = format->sample_rate;
    wave_format.wBitsPerSample = format->bit_depth;
    wave_format.nBlockAlign = format->frame_size;
    wave_format.nAvgBytesPerSec = format->sample_rate * format->frame_size;
    wave_format.cbSize = 0;

    buffer_desc.lpwfxFormat = &wave_format;

    // 보조 버퍼 생성
    LPDIRECTSOUNDBUFFER temp_buffer;
    HRESULT hr = IDirectSound8_CreateSoundBuffer(g_direct_sound, &buffer_desc, &temp_buffer, NULL);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "DirectSound 버퍼 생성 실패: 0x%08X", hr);
        return windows_audio_error_to_common(hr);
    }

    // DirectSound8 인터페이스로 쿼리
    hr = IDirectSoundBuffer_QueryInterface(temp_buffer, &IID_IDirectSoundBuffer8, (void**)secondary_buffer);
    IDirectSoundBuffer_Release(temp_buffer);

    if (FAILED(hr)) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "DirectSound8 인터페이스 쿼리 실패: 0x%08X", hr);
        return windows_audio_error_to_common(hr);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 디바이스 열거 및 정보 조회
// ============================================================================

/**
 * @brief Windows 오디오 디바이스 열거
 */
static ETResult windows_enumerate_devices(ETAudioDeviceType type, ETAudioDeviceInfo* devices, int* count) {
    if (!devices || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETResult result = windows_audio_init();
    if (result != ET_SUCCESS) {
        return result;
    }

    int device_index = 0;
    int max_devices = *count;
    *count = 0;

    // WASAPI 디바이스 열거
    if (g_wasapi_initialized) {
        IMMDeviceCollection* device_collection = NULL;
        EDataFlow data_flow = (type == ET_AUDIO_DEVICE_OUTPUT) ? eRender : eCapture;

        HRESULT hr = IMMDeviceEnumerator_EnumAudioEndpoints(g_device_enumerator, data_flow,
                                                          DEVICE_STATE_ACTIVE, &device_collection);
        if (SUCCEEDED(hr)) {
            UINT wasapi_count = 0;
            IMMDeviceCollection_GetCount(device_collection, &wasapi_count);

            for (UINT i = 0; i < wasapi_count && device_index < max_devices; i++) {
                IMMDevice* device = NULL;
                hr = IMMDeviceCollection_Item(device_collection, i, &device);
                if (SUCCEEDED(hr)) {
                    IPropertyStore* props = NULL;
                    hr = IMMDevice_OpenPropertyStore(device, STGM_READ, &props);
                    if (SUCCEEDED(hr)) {
                        PROPVARIANT var_name;
                        PropVariantInit(&var_name);

                        hr = IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &var_name);
                        if (SUCCEEDED(hr) && var_name.vt == VT_LPWSTR) {
                            // 디바이스 정보 설정
                            ETAudioDeviceInfo* info = &devices[device_index];
                            WideCharToMultiByte(CP_UTF8, 0, var_name.pwszVal, -1,
                                              info->name, sizeof(info->name), NULL, NULL);

                            // 디바이스 ID 생성
                            snprintf(info->id, sizeof(info->id), "wasapi_%u", i);
                            info->type = type;
                            info->max_channels = (type == ET_AUDIO_DEVICE_OUTPUT) ? 8 : 2;

                            // 지원 샘플 레이트 설정 (기본값)
                            static uint32_t supported_rates[] = {44100, 48000, 96000, 192000};
                            info->supported_rates = supported_rates;
                            info->rate_count = 4;
                            info->is_default = (i == 0);
                            info->min_latency = 10;
                            info->max_latency = 100;

                            device_index++;
                        }

                        PropVariantClear(&var_name);
                        IPropertyStore_Release(props);
                    }
                    IMMDevice_Release(device);
                }
            }

            IMMDeviceCollection_Release(device_collection);
        }
    }

    // DirectSound 디바이스 열거 (출력만)
    if (type == ET_AUDIO_DEVICE_OUTPUT && g_directsound_initialized) {
        UINT ds_count = waveOutGetNumDevs();
        for (UINT i = 0; i < ds_count && device_index < max_devices; i++) {
            WAVEOUTCAPS caps;
            MMRESULT mmr = waveOutGetDevCaps(i, &caps, sizeof(caps));
            if (mmr == MMSYSERR_NOERROR) {
                ETAudioDeviceInfo* info = &devices[device_index];
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1,
                                  info->name, sizeof(info->name), NULL, NULL);

                snprintf(info->id, sizeof(info->id), "directsound_%u", i);
                info->type = type;
                info->max_channels = caps.wChannels;

                // 지원 샘플 레이트 설정
                static uint32_t supported_rates[] = {44100, 48000};
                info->supported_rates = supported_rates;
                info->rate_count = 2;
                info->is_default = (i == WAVE_MAPPER);
                info->min_latency = 20;
                info->max_latency = 200;

                device_index++;
            }
        }
    }

    *count = device_index;
    return ET_SUCCESS;
}

// ============================================================================
// 디바이스 관리 함수들
// ============================================================================

/**
 * @brief Windows 출력 디바이스 열기
 */
static ETResult windows_open_output_device(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device) {
    if (!format || !device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETResult result = windows_audio_init();
    if (result != ET_SUCCESS) {
        return result;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)calloc(1, sizeof(ETWindowsAudioDevice));
    if (!win_device) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 포맷 복사
    win_device->format = *format;
    win_device->state = ET_AUDIO_STATE_STOPPED;
    win_device->is_input = false;

    // WASAPI 우선 시도
    bool use_wasapi = true;
    if (device_name && strncmp(device_name, "directsound_", 12) == 0) {
        use_wasapi = false;
    }

    if (use_wasapi && g_wasapi_initialized) {
        // WASAPI 디바이스 열기
        IMMDevice* mm_device = NULL;
        HRESULT hr;

        if (!device_name || strcmp(device_name, "default") == 0) {
            hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g_device_enumerator, eRender, eConsole, &mm_device);
        } else {
            // 특정 디바이스 찾기 (간단한 구현)
            hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g_device_enumerator, eRender, eConsole, &mm_device);
        }

        if (SUCCEEDED(hr)) {
            hr = IMMDevice_Activate(mm_device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&win_device->audio_client);
            if (SUCCEEDED(hr)) {
                // 오디오 포맷 설정
                WAVEFORMATEX wave_format = {0};
                wave_format.wFormatTag = format->is_float ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
                wave_format.nChannels = format->num_channels;
                wave_format.nSamplesPerSec = format->sample_rate;
                wave_format.wBitsPerSample = format->bit_depth;
                wave_format.nBlockAlign = format->frame_size;
                wave_format.nAvgBytesPerSec = format->sample_rate * format->frame_size;

                REFERENCE_TIME buffer_duration = (REFERENCE_TIME)(format->buffer_size * 10000000.0 / format->sample_rate);
                hr = IAudioClient_Initialize(win_device->audio_client, AUDCLNT_SHAREMODE_SHARED, 0,
                                           buffer_duration, 0, &wave_format, NULL);

                if (SUCCEEDED(hr)) {
                    hr = IAudioClient_GetService(win_device->audio_client, &IID_IAudioRenderClient,
                                               (void**)&win_device->render_client);
                    if (SUCCEEDED(hr)) {
                        win_device->wasapi_device = mm_device;
                        win_device->use_wasapi = true;
                        *device = (ETAudioDevice*)win_device;
                        return ET_SUCCESS;
                    }
                }
            }
            IMMDevice_Release(mm_device);
        }
    }

    // DirectSound 폴백
    if (g_directsound_initialized) {
        result = windows_create_directsound_buffer(format, format->buffer_size * format->frame_size,
                                                 &win_device->ds_buffer);
        if (result == ET_SUCCESS) {
            win_device->ds_buffer_size = format->buffer_size * format->frame_size;
            win_device->ds_write_pos = 0;
            win_device->use_wasapi = false;
            *device = (ETAudioDevice*)win_device;
            return ET_SUCCESS;
        }
    }

    free(win_device);
    return ET_ERROR_HARDWARE;
}

/**
 * @brief Windows 입력 디바이스 열기
 */
static ETResult windows_open_input_device(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device) {
    if (!format || !device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETResult result = windows_audio_init();
    if (result != ET_SUCCESS) {
        return result;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)calloc(1, sizeof(ETWindowsAudioDevice));
    if (!win_device) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    win_device->format = *format;
    win_device->state = ET_AUDIO_STATE_STOPPED;
    win_device->is_input = true;

    // WASAPI 입력 디바이스 열기
    if (g_wasapi_initialized) {
        IMMDevice* mm_device = NULL;
        HRESULT hr;

        if (!device_name || strcmp(device_name, "default") == 0) {
            hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g_device_enumerator, eCapture, eConsole, &mm_device);
        } else {
            hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g_device_enumerator, eCapture, eConsole, &mm_device);
        }

        if (SUCCEEDED(hr)) {
            hr = IMMDevice_Activate(mm_device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&win_device->audio_client);
            if (SUCCEEDED(hr)) {
                WAVEFORMATEX wave_format = {0};
                wave_format.wFormatTag = format->is_float ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
                wave_format.nChannels = format->num_channels;
                wave_format.nSamplesPerSec = format->sample_rate;
                wave_format.wBitsPerSample = format->bit_depth;
                wave_format.nBlockAlign = format->frame_size;
                wave_format.nAvgBytesPerSec = format->sample_rate * format->frame_size;

                REFERENCE_TIME buffer_duration = (REFERENCE_TIME)(format->buffer_size * 10000000.0 / format->sample_rate);
                hr = IAudioClient_Initialize(win_device->audio_client, AUDCLNT_SHAREMODE_SHARED, 0,
                                           buffer_duration, 0, &wave_format, NULL);

                if (SUCCEEDED(hr)) {
                    hr = IAudioClient_GetService(win_device->audio_client, &IID_IAudioCaptureClient,
                                               (void**)&win_device->capture_client);
                    if (SUCCEEDED(hr)) {
                        win_device->wasapi_device = mm_device;
                        win_device->use_wasapi = true;
                        *device = (ETAudioDevice*)win_device;
                        return ET_SUCCESS;
                    }
                }
            }
            IMMDevice_Release(mm_device);
        }
    }

    free(win_device);
    return ET_ERROR_HARDWARE;
}

/**
 * @brief Windows 오디오 디바이스 닫기
 */
static void windows_close_device(ETAudioDevice* device) {
    if (!device) {
        return;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)device;

    // 스트림 정지
    if (win_device->state == ET_AUDIO_STATE_RUNNING) {
        if (win_device->stop_event) {
            SetEvent(win_device->stop_event);
        }
        if (win_device->audio_thread) {
            WaitForSingleObject(win_device->audio_thread, 1000);
            CloseHandle(win_device->audio_thread);
        }
    }

    // WASAPI 리소스 정리
    if (win_device->use_wasapi) {
        if (win_device->render_client) {
            IAudioRenderClient_Release(win_device->render_client);
        }
        if (win_device->capture_client) {
            IAudioCaptureClient_Release(win_device->capture_client);
        }
        if (win_device->audio_client) {
            IAudioClient_Release(win_device->audio_client);
        }
        if (win_device->wasapi_device) {
            IMMDevice_Release(win_device->wasapi_device);
        }
    }

    // DirectSound 리소스 정리
    if (win_device->ds_buffer) {
        IDirectSoundBuffer8_Release(win_device->ds_buffer);
    }

    // 이벤트 핸들 정리
    if (win_device->event_handle) {
        CloseHandle(win_device->event_handle);
    }
    if (win_device->stop_event) {
        CloseHandle(win_device->stop_event);
    }

    free(win_device);
}

// ============================================================================
// 스트림 제어 함수들
// ============================================================================

/**
 * @brief 오디오 스트림 시작
 */
static ETResult windows_start_stream(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)device;

    if (win_device->state == ET_AUDIO_STATE_RUNNING) {
        return ET_SUCCESS;
    }

    if (win_device->use_wasapi && win_device->audio_client) {
        HRESULT hr = IAudioClient_Start(win_device->audio_client);
        if (FAILED(hr)) {
            return windows_audio_error_to_common(hr);
        }
    } else if (win_device->ds_buffer) {
        HRESULT hr = IDirectSoundBuffer8_Play(win_device->ds_buffer, 0, 0, DSBPLAY_LOOPING);
        if (FAILED(hr)) {
            return windows_audio_error_to_common(hr);
        }
    }

    win_device->state = ET_AUDIO_STATE_RUNNING;
    return ET_SUCCESS;
}

/**
 * @brief 오디오 스트림 정지
 */
static ETResult windows_stop_stream(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)device;

    if (win_device->state == ET_AUDIO_STATE_STOPPED) {
        return ET_SUCCESS;
    }

    if (win_device->use_wasapi && win_device->audio_client) {
        HRESULT hr = IAudioClient_Stop(win_device->audio_client);
        if (FAILED(hr)) {
            return windows_audio_error_to_common(hr);
        }
    } else if (win_device->ds_buffer) {
        HRESULT hr = IDirectSoundBuffer8_Stop(win_device->ds_buffer);
        if (FAILED(hr)) {
            return windows_audio_error_to_common(hr);
        }
    }

    win_device->state = ET_AUDIO_STATE_STOPPED;
    return ET_SUCCESS;
}

/**
 * @brief 오디오 스트림 일시정지
 */
static ETResult windows_pause_stream(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)device;

    if (win_device->state != ET_AUDIO_STATE_RUNNING) {
        return ET_ERROR_INVALID_STATE;
    }

    // WASAPI는 일시정지를 직접 지원하지 않으므로 정지로 처리
    ETResult result = windows_stop_stream(device);
    if (result == ET_SUCCESS) {
        win_device->state = ET_AUDIO_STATE_PAUSED;
    }

    return result;
}

// ============================================================================
// 콜백 및 상태 관리
// ============================================================================

/**
 * @brief 오디오 콜백 설정
 */
static ETResult windows_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data) {
    if (!device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsAudioDevice* win_device = (ETWindowsAudioDevice*)device;
    win_device->callback = callback;
    win_device->user_data = user_data;

    return ET_SUCCESS;
}

/**
 * @brief 디바이스 지연시간 조회
 */
static uint32_t windows_get_latency(const ETAudioDevice* device) {
    if (!device) {
        return 0;
    }

    const ETWindowsAudioDevice* win_device = (const ETWindowsAudioDevice*)device;

    if (win_device->use_wasapi && win_device->audio_client) {
        REFERENCE_TIME latency = 0;
        HRESULT hr = IAudioClient_GetStreamLatency(win_device->audio_client, &latency);
        if (SUCCEEDED(hr)) {
            return (uint32_t)(latency / 10000); // 100ns 단위를 ms로 변환
        }
    }

    // DirectSound 기본 지연시간
    return 50;
}

/**
 * @brief 디바이스 상태 조회
 */
static ETAudioState windows_get_state(const ETAudioDevice* device) {
    if (!device) {
        return ET_AUDIO_STATE_ERROR;
    }

    const ETWindowsAudioDevice* win_device = (const ETWindowsAudioDevice*)device;
    return win_device->state;
}

/**
 * @brief 포맷 지원 여부 확인
 */
static bool windows_is_format_supported(const char* device_name, const ETAudioFormat* format) {
    if (!format) {
        return false;
    }

    // 기본적인 포맷 검증
    if (format->sample_rate < 8000 || format->sample_rate > 192000) {
        return false;
    }

    if (format->num_channels < 1 || format->num_channels > 8) {
        return false;
    }

    if (format->bit_depth != 16 && format->bit_depth != 24 && format->bit_depth != 32) {
        return false;
    }

    return true;
}

/**
 * @brief 지원되는 포맷 목록 조회
 */
static ETResult windows_get_supported_formats(const char* device_name, ETAudioFormat* formats, int* count) {
    if (!formats || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기본 지원 포맷들
    static const ETAudioFormat supported_formats[] = {
        {44100, 16, 2, 4, 1024, false},    // CD 품질
        {48000, 16, 2, 4, 1024, false},    // DAT 품질
        {48000, 24, 2, 6, 1024, false},    // 고품질
        {96000, 24, 2, 6, 1024, false},    // 고해상도
        {44100, 32, 2, 8, 1024, true},     // Float32
        {48000, 32, 2, 8, 1024, true}      // Float32 고품질
    };

    int max_formats = *count;
    int supported_count = sizeof(supported_formats) / sizeof(supported_formats[0]);

    if (max_formats > supported_count) {
        max_formats = supported_count;
    }

    for (int i = 0; i < max_formats; i++) {
        formats[i] = supported_formats[i];
    }

    *count = max_formats;
    return ET_SUCCESS;
}

// ============================================================================
// Windows 오디오 인터페이스 구조체
// ============================================================================

static ETAudioInterface windows_audio_interface = {
    .open_output_device = windows_open_output_device,
    .open_input_device = windows_open_input_device,
    .close_device = windows_close_device,
    .start_stream = windows_start_stream,
    .stop_stream = windows_stop_stream,
    .pause_stream = windows_pause_stream,
    .set_callback = windows_set_callback,
    .enumerate_devices = windows_enumerate_devices,
    .get_latency = windows_get_latency,
    .get_state = windows_get_state,
    .is_format_supported = windows_is_format_supported,
    .get_supported_formats = windows_get_supported_formats,
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief Windows 오디오 인터페이스 가져오기
 */
ETAudioInterface* et_get_windows_audio_interface(void) {
    return &windows_audio_interface;
}

/**
 * @brief Windows 오디오 시스템 초기화 (공개 함수)
 */
ETResult et_windows_audio_initialize(void) {
    return windows_audio_init();
}

/**
 * @brief Windows 오디오 시스템 정리 (공개 함수)
 */
void et_windows_audio_cleanup(void) {
    windows_audio_finalize();
}

#else
// Windows가 아닌 플랫폼에서는 빈 구현
ETAudioInterface* et_get_windows_audio_interface(void) { return NULL; }
ETResult et_windows_audio_initialize(void) { return ET_ERROR_NOT_IMPLEMENTED; }
void et_windows_audio_cleanup(void) {}
#endif