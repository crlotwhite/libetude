/**
 * @file windows_audio.c
 * @brief Windows 오디오 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>

// DirectSound 관련 전역 변수
static LPDIRECTSOUND8 g_direct_sound = NULL;
static bool g_directsound_initialized = false;

/**
 * @brief Windows 오디오 시스템 초기화
 */
void windows_audio_init(void) {
    if (g_directsound_initialized) {
        return;
    }

    // DirectSound 초기화
    HRESULT hr = DirectSoundCreate8(NULL, &g_direct_sound, NULL);
    if (SUCCEEDED(hr)) {
        // 협력 레벨 설정
        HWND hwnd = GetDesktopWindow();
        hr = IDirectSound8_SetCooperativeLevel(g_direct_sound, hwnd, DSSCL_PRIORITY);

        if (SUCCEEDED(hr)) {
            g_directsound_initialized = true;
        }
    }

    if (!g_directsound_initialized) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to initialize DirectSound");
    }
}

/**
 * @brief Windows 오디오 시스템 정리
 */
void windows_audio_finalize(void) {
    if (g_direct_sound) {
        IDirectSound8_Release(g_direct_sound);
        g_direct_sound = NULL;
    }
    g_directsound_initialized = false;
}

/**
 * @brief WaveOut 에러 코드를 문자열로 변환
 * @param error WaveOut 에러 코드
 * @return 에러 문자열
 */
static const char* windows_wave_error_string(MMRESULT error) {
    switch (error) {
        case MMSYSERR_NOERROR: return "No error";
        case MMSYSERR_ERROR: return "Unspecified error";
        case MMSYSERR_BADDEVICEID: return "Bad device ID";
        case MMSYSERR_NOTENABLED: return "Driver not enabled";
        case MMSYSERR_ALLOCATED: return "Device already allocated";
        case MMSYSERR_INVALHANDLE: return "Invalid handle";
        case MMSYSERR_NODRIVER: return "No driver";
        case MMSYSERR_NOMEM: return "Out of memory";
        case WAVERR_BADFORMAT: return "Unsupported wave format";
        case WAVERR_STILLPLAYING: return "Still playing";
        case WAVERR_UNPREPARED: return "Header not prepared";
        default: return "Unknown error";
    }
}

/**
 * @brief DirectSound 버퍼 생성
 * @param format 오디오 포맷
 * @param buffer_size 버퍼 크기 (바이트)
 * @param secondary_buffer 생성된 보조 버퍼 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult windows_create_directsound_buffer(const ETAudioFormat* format,
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
    wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wave_format.nChannels = format->num_channels;
    wave_format.nSamplesPerSec = format->sample_rate;
    wave_format.wBitsPerSample = 32;
    wave_format.nBlockAlign = format->frame_size;
    wave_format.nAvgBytesPerSec = format->sample_rate * format->frame_size;
    wave_format.cbSize = 0;

    buffer_desc.lpwfxFormat = &wave_format;

    // 보조 버퍼 생성
    LPDIRECTSOUNDBUFFER temp_buffer;
    HRESULT hr = IDirectSound8_CreateSoundBuffer(g_direct_sound, &buffer_desc, &temp_buffer, NULL);

    if (FAILED(hr)) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to create DirectSound buffer");
        return ET_ERROR_HARDWARE;
    }

    // DirectSound8 인터페이스로 쿼리
    hr = IDirectSoundBuffer_QueryInterface(temp_buffer, &IID_IDirectSoundBuffer8, (void**)secondary_buffer);
    IDirectSoundBuffer_Release(temp_buffer);

    if (FAILED(hr)) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to query DirectSound8 interface");
        return ET_ERROR_HARDWARE;
    }

    return ET_SUCCESS;
}

/**
 * @brief WASAPI 디바이스 열거
 * @param device_type 디바이스 타입 (출력/입력)
 * @param device_count 디바이스 개수 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult windows_enumerate_wasapi_devices(ETAudioDeviceType device_type, UINT* device_count) {
    HRESULT hr;
    IMMDeviceEnumerator* device_enumerator = NULL;
    IMMDeviceCollection* device_collection = NULL;

    // COM 초기화
    hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return ET_ERROR_HARDWARE;
    }

    // 디바이스 열거자 생성
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator, (void**)&device_enumerator);

    if (FAILED(hr)) {
        CoUninitialize();
        return ET_ERROR_HARDWARE;
    }

    // 디바이스 컬렉션 가져오기
    EDataFlow data_flow = (device_type == ET_AUDIO_DEVICE_OUTPUT) ? eRender : eCapture;
    hr = IMMDeviceEnumerator_EnumAudioEndpoints(device_enumerator, data_flow,
                                              DEVICE_STATE_ACTIVE, &device_collection);

    if (SUCCEEDED(hr)) {
        IMMDeviceCollection_GetCount(device_collection, device_count);
        IMMDeviceCollection_Release(device_collection);
    }

    IMMDeviceEnumerator_Release(device_enumerator);
    CoUninitialize();

    return SUCCEEDED(hr) ? ET_SUCCESS : ET_ERROR_HARDWARE;
}

/**
 * @brief Windows 오디오 디바이스 정보 조회
 * @param device_id 디바이스 ID (-1이면 기본 디바이스)
 * @param device_type 디바이스 타입
 * @param device_name 디바이스 이름 (출력, 최대 256자)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult windows_get_device_info(int device_id, ETAudioDeviceType device_type,
                               char* device_name, size_t name_size) {
    if (device_type == ET_AUDIO_DEVICE_OUTPUT) {
        WAVEOUTCAPS caps;
        MMRESULT result;

        if (device_id == -1) {
            result = waveOutGetDevCaps(WAVE_MAPPER, &caps, sizeof(caps));
        } else {
            result = waveOutGetDevCaps(device_id, &caps, sizeof(caps));
        }

        if (result == MMSYSERR_NOERROR) {
            // 유니코드를 UTF-8로 변환 (간단한 ASCII 변환)
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, device_name, (int)name_size, NULL, NULL);
            return ET_SUCCESS;
        }
    } else {
        WAVEINCAPS caps;
        MMRESULT result;

        if (device_id == -1) {
            result = waveInGetDevCaps(WAVE_MAPPER, &caps, sizeof(caps));
        } else {
            result = waveInGetDevCaps(device_id, &caps, sizeof(caps));
        }

        if (result == MMSYSERR_NOERROR) {
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, device_name, (int)name_size, NULL, NULL);
            return ET_SUCCESS;
        }
    }

    return ET_ERROR_HARDWARE;
}

#else
// Windows가 아닌 플랫폼에서는 빈 구현
void windows_audio_init(void) {}
void windows_audio_finalize(void) {}
#endif