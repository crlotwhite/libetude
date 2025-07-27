/**
 * @file audio_io.c
 * @brief 오디오 I/O 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/audio_io.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
    #include <dsound.h>
#elif defined(__APPLE__)
    #include <AudioToolbox/AudioToolbox.h>
    #include <CoreAudio/CoreAudio.h>
#elif defined(__linux__)
    #include <alsa/asoundlib.h>
#endif

/**
 * @brief 오디오 디바이스 내부 구조체
 */
struct ETAudioDevice {
    ETAudioFormat format;           /**< 오디오 포맷 */
    ETAudioDeviceType type;         /**< 디바이스 타입 */
    ETAudioState state;             /**< 디바이스 상태 */
    ETAudioCallback callback;       /**< 콜백 함수 */
    void* user_data;                /**< 사용자 데이터 */

    // 플랫폼별 핸들
#ifdef _WIN32
    HWAVEOUT wave_out;              /**< Windows WaveOut 핸들 */
    HWAVEIN wave_in;                /**< Windows WaveIn 핸들 */
    WAVEHDR* wave_headers;          /**< Wave 헤더 배열 */
    int num_buffers;                /**< 버퍼 개수 */
#elif defined(__APPLE__)
    AudioQueueRef audio_queue;      /**< macOS AudioQueue */
    AudioQueueBufferRef* buffers;   /**< 오디오 버퍼 배열 */
    int num_buffers;                /**< 버퍼 개수 */
#elif defined(__linux__)
    snd_pcm_t* pcm_handle;          /**< ALSA PCM 핸들 */
    snd_pcm_hw_params_t* hw_params; /**< 하드웨어 파라미터 */
#endif

    ETAudioBuffer* ring_buffer;     /**< 링 버퍼 */
    bool is_running;                /**< 실행 상태 */
    uint32_t latency_ms;            /**< 지연시간 (밀리초) */
};

// 전역 변수
static bool g_audio_initialized = false;

/**
 * @brief 오디오 시스템 초기화
 */
static ETResult audio_system_init(void) {
    if (g_audio_initialized) {
        return ET_SUCCESS;
    }

#ifdef _WIN32
    // Windows 오디오 시스템 초기화
    if (waveOutGetNumDevs() == 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "No audio output devices found");
        return ET_ERROR_HARDWARE;
    }
#elif defined(__APPLE__)
    // macOS 오디오 시스템은 별도 초기화 불필요
#elif defined(__linux__)
    // ALSA 초기화는 디바이스 열 때 수행
#endif

    g_audio_initialized = true;
    return ET_SUCCESS;
}

/**
 * @brief 오디오 시스템 정리
 */
static void audio_system_cleanup(void) {
    if (!g_audio_initialized) {
        return;
    }

    // 플랫폼별 정리 작업
    g_audio_initialized = false;
}

ETAudioFormat et_audio_format_create(uint32_t sample_rate, uint16_t num_channels, uint32_t buffer_size) {
    ETAudioFormat format;
    format.sample_rate = sample_rate;
    format.bit_depth = 32; // float 사용
    format.num_channels = num_channels;
    format.frame_size = num_channels * sizeof(float);
    format.buffer_size = buffer_size;
    return format;
}

#ifdef _WIN32
/**
 * @brief Windows WaveOut 콜백 함수
 */
static void CALLBACK wave_out_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    ETAudioDevice* device = (ETAudioDevice*)dwInstance;

    if (uMsg == WOM_DONE && device->is_running) {
        WAVEHDR* header = (WAVEHDR*)dwParam1;

        // 콜백 호출
        if (device->callback) {
            device->callback((float*)header->lpData,
                           header->dwBufferLength / device->format.frame_size,
                           device->user_data);
        }

        // 버퍼 재사용
        waveOutWrite(hwo, header, sizeof(WAVEHDR));
    }
}
#endif

#ifdef __APPLE__
/**
 * @brief macOS AudioQueue 콜백 함수
 */
static void audio_queue_callback(void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    ETAudioDevice* device = (ETAudioDevice*)user_data;

    if (!device->is_running) {
        return;
    }

    // 콜백 호출
    if (device->callback) {
        device->callback((float*)buffer->mAudioData,
                        buffer->mAudioDataBytesCapacity / device->format.frame_size,
                        device->user_data);
    }

    // 버퍼 재사용
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}
#endif

ETAudioDevice* et_audio_open_output_device(const char* device_name, const ETAudioFormat* format) {
    if (audio_system_init() != ET_SUCCESS) {
        return NULL;
    }

    ETAudioDevice* device = (ETAudioDevice*)malloc(sizeof(ETAudioDevice));
    if (!device) {
        et_set_error(ET_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "Failed to allocate audio device");
        return NULL;
    }

    memset(device, 0, sizeof(ETAudioDevice));
    device->format = *format;
    device->type = ET_AUDIO_DEVICE_OUTPUT;
    device->state = ET_AUDIO_STATE_STOPPED;
    device->latency_ms = 20; // 기본 지연시간 20ms

    // 링 버퍼 생성
    device->ring_buffer = et_audio_buffer_create(format->buffer_size * 4, format->num_channels);
    if (!device->ring_buffer) {
        free(device);
        return NULL;
    }

#ifdef _WIN32
    // Windows WaveOut 초기화
    WAVEFORMATEX wave_format;
    wave_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wave_format.nChannels = format->num_channels;
    wave_format.nSamplesPerSec = format->sample_rate;
    wave_format.wBitsPerSample = 32;
    wave_format.nBlockAlign = format->frame_size;
    wave_format.nAvgBytesPerSec = format->sample_rate * format->frame_size;
    wave_format.cbSize = 0;

    MMRESULT result = waveOutOpen(&device->wave_out, WAVE_MAPPER, &wave_format,
                                  (DWORD_PTR)wave_out_callback, (DWORD_PTR)device,
                                  CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to open Windows audio device");
        et_audio_buffer_destroy(device->ring_buffer);
        free(device);
        return NULL;
    }

    // 버퍼 할당
    device->num_buffers = 4;
    device->wave_headers = (WAVEHDR*)malloc(sizeof(WAVEHDR) * device->num_buffers);

    for (int i = 0; i < device->num_buffers; i++) {
        device->wave_headers[i].lpData = (LPSTR)malloc(format->buffer_size * format->frame_size);
        device->wave_headers[i].dwBufferLength = format->buffer_size * format->frame_size;
        device->wave_headers[i].dwFlags = 0;

        waveOutPrepareHeader(device->wave_out, &device->wave_headers[i], sizeof(WAVEHDR));
    }

#elif defined(__APPLE__)
    // macOS AudioQueue 초기화
    AudioStreamBasicDescription audio_format;
    audio_format.mSampleRate = format->sample_rate;
    audio_format.mFormatID = kAudioFormatLinearPCM;
    audio_format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    audio_format.mBytesPerPacket = format->frame_size;
    audio_format.mFramesPerPacket = 1;
    audio_format.mBytesPerFrame = format->frame_size;
    audio_format.mChannelsPerFrame = format->num_channels;
    audio_format.mBitsPerChannel = 32;

    OSStatus status = AudioQueueNewOutput(&audio_format, audio_queue_callback, device,
                                         NULL, NULL, 0, &device->audio_queue);

    if (status != noErr) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to create macOS AudioQueue");
        et_audio_buffer_destroy(device->ring_buffer);
        free(device);
        return NULL;
    }

    // 버퍼 할당
    device->num_buffers = 3;
    device->buffers = (AudioQueueBufferRef*)malloc(sizeof(AudioQueueBufferRef) * device->num_buffers);

    for (int i = 0; i < device->num_buffers; i++) {
        AudioQueueAllocateBuffer(device->audio_queue,
                               format->buffer_size * format->frame_size,
                               &device->buffers[i]);
        device->buffers[i]->mAudioDataByteSize = format->buffer_size * format->frame_size;
    }

#elif defined(__linux__)
    // ALSA 초기화
    int err = snd_pcm_open(&device->pcm_handle, device_name ? device_name : "default",
                          SND_PCM_STREAM_PLAYBACK, 0);

    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to open ALSA device");
        et_audio_buffer_destroy(device->ring_buffer);
        free(device);
        return NULL;
    }

    // 하드웨어 파라미터 설정
    snd_pcm_hw_params_alloca(&device->hw_params);
    snd_pcm_hw_params_any(device->pcm_handle, device->hw_params);
    snd_pcm_hw_params_set_access(device->pcm_handle, device->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(device->pcm_handle, device->hw_params, SND_PCM_FORMAT_FLOAT);
    snd_pcm_hw_params_set_channels(device->pcm_handle, device->hw_params, format->num_channels);
    snd_pcm_hw_params_set_rate(device->pcm_handle, device->hw_params, format->sample_rate, 0);

    err = snd_pcm_hw_params(device->pcm_handle, device->hw_params);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to set ALSA parameters");
        snd_pcm_close(device->pcm_handle);
        et_audio_buffer_destroy(device->ring_buffer);
        free(device);
        return NULL;
    }
#endif

    return device;
}

ETAudioDevice* et_audio_open_input_device(const char* device_name, const ETAudioFormat* format) {
    // 입력 디바이스는 출력 디바이스와 유사하게 구현
    // 여기서는 기본 구조만 제공
    ETAudioDevice* device = (ETAudioDevice*)malloc(sizeof(ETAudioDevice));
    if (!device) {
        return NULL;
    }

    memset(device, 0, sizeof(ETAudioDevice));
    device->format = *format;
    device->type = ET_AUDIO_DEVICE_INPUT;
    device->state = ET_AUDIO_STATE_STOPPED;

    // 플랫폼별 입력 디바이스 초기화는 향후 구현

    return device;
}

void et_audio_close_device(ETAudioDevice* device) {
    if (!device) {
        return;
    }

    // 스트림 정지
    et_audio_stop(device);

#ifdef _WIN32
    if (device->wave_out) {
        waveOutReset(device->wave_out);

        // 버퍼 정리
        for (int i = 0; i < device->num_buffers; i++) {
            waveOutUnprepareHeader(device->wave_out, &device->wave_headers[i], sizeof(WAVEHDR));
            free(device->wave_headers[i].lpData);
        }
        free(device->wave_headers);

        waveOutClose(device->wave_out);
    }

#elif defined(__APPLE__)
    if (device->audio_queue) {
        AudioQueueStop(device->audio_queue, true);

        // 버퍼 정리
        for (int i = 0; i < device->num_buffers; i++) {
            AudioQueueFreeBuffer(device->audio_queue, device->buffers[i]);
        }
        free(device->buffers);

        AudioQueueDispose(device->audio_queue, true);
    }

#elif defined(__linux__)
    if (device->pcm_handle) {
        snd_pcm_close(device->pcm_handle);
    }
#endif

    // 링 버퍼 정리
    if (device->ring_buffer) {
        et_audio_buffer_destroy(device->ring_buffer);
    }

    free(device);
}

ETResult et_audio_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data) {
    if (!device) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Device is NULL");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    device->callback = callback;
    device->user_data = user_data;

    return ET_SUCCESS;
}

ETResult et_audio_start(ETAudioDevice* device) {
    if (!device) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Device is NULL");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (device->state == ET_AUDIO_STATE_RUNNING) {
        return ET_SUCCESS;
    }

    device->is_running = true;
    device->state = ET_AUDIO_STATE_RUNNING;

#ifdef _WIN32
    // Windows WaveOut 시작
    for (int i = 0; i < device->num_buffers; i++) {
        // 초기 버퍼 채우기
        if (device->callback) {
            device->callback((float*)device->wave_headers[i].lpData,
                           device->wave_headers[i].dwBufferLength / device->format.frame_size,
                           device->user_data);
        }
        waveOutWrite(device->wave_out, &device->wave_headers[i], sizeof(WAVEHDR));
    }

#elif defined(__APPLE__)
    // macOS AudioQueue 시작
    for (int i = 0; i < device->num_buffers; i++) {
        // 초기 버퍼 채우기
        if (device->callback) {
            device->callback((float*)device->buffers[i]->mAudioData,
                           device->buffers[i]->mAudioDataBytesCapacity / device->format.frame_size,
                           device->user_data);
        }
        AudioQueueEnqueueBuffer(device->audio_queue, device->buffers[i], 0, NULL);
    }

    OSStatus status = AudioQueueStart(device->audio_queue, NULL);
    if (status != noErr) {
        device->is_running = false;
        device->state = ET_AUDIO_STATE_STOPPED;
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to start AudioQueue");
        return ET_ERROR_HARDWARE;
    }

#elif defined(__linux__)
    // ALSA 시작
    snd_pcm_prepare(device->pcm_handle);
#endif

    return ET_SUCCESS;
}

ETResult et_audio_stop(ETAudioDevice* device) {
    if (!device) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Device is NULL");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    device->is_running = false;
    device->state = ET_AUDIO_STATE_STOPPED;

#ifdef _WIN32
    if (device->wave_out) {
        waveOutReset(device->wave_out);
    }

#elif defined(__APPLE__)
    if (device->audio_queue) {
        AudioQueueStop(device->audio_queue, true);
    }

#elif defined(__linux__)
    if (device->pcm_handle) {
        snd_pcm_drop(device->pcm_handle);
    }
#endif

    return ET_SUCCESS;
}

ETResult et_audio_pause(ETAudioDevice* device) {
    if (!device) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Device is NULL");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    device->state = ET_AUDIO_STATE_PAUSED;

#ifdef _WIN32
    if (device->wave_out) {
        waveOutPause(device->wave_out);
    }

#elif defined(__APPLE__)
    if (device->audio_queue) {
        AudioQueuePause(device->audio_queue);
    }

#elif defined(__linux__)
    if (device->pcm_handle) {
        snd_pcm_pause(device->pcm_handle, 1);
    }
#endif

    return ET_SUCCESS;
}

ETAudioState et_audio_get_state(const ETAudioDevice* device) {
    if (!device) {
        return ET_AUDIO_STATE_STOPPED;
    }

    return device->state;
}

uint32_t et_audio_get_latency(const ETAudioDevice* device) {
    if (!device) {
        return 0;
    }

    return device->latency_ms;
}

// ============================================================================
// 오디오 버퍼 관리 시스템
// ============================================================================

ETAudioBuffer* et_audio_buffer_create(uint32_t size, uint16_t num_channels) {
    if (size == 0 || num_channels == 0) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Invalid buffer parameters");
        return NULL;
    }

    ETAudioBuffer* buffer = (ETAudioBuffer*)malloc(sizeof(ETAudioBuffer));
    if (!buffer) {
        et_set_error(ET_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "Failed to allocate audio buffer");
        return NULL;
    }

    // 채널 수를 고려한 실제 버퍼 크기 계산
    uint32_t total_size = size * num_channels;
    buffer->data = (float*)malloc(total_size * sizeof(float));
    if (!buffer->data) {
        free(buffer);
        et_set_error(ET_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "Failed to allocate buffer data");
        return NULL;
    }

    buffer->size = size;
    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->available = 0;
    buffer->is_full = false;

    // 버퍼 초기화
    memset(buffer->data, 0, total_size * sizeof(float));

    return buffer;
}

void et_audio_buffer_destroy(ETAudioBuffer* buffer) {
    if (!buffer) {
        return;
    }

    if (buffer->data) {
        free(buffer->data);
    }

    free(buffer);
}

uint32_t et_audio_buffer_write(ETAudioBuffer* buffer, const float* data, uint32_t num_frames) {
    if (!buffer || !data || num_frames == 0) {
        return 0;
    }

    if (buffer->is_full) {
        return 0; // 버퍼가 가득 참
    }

    uint32_t frames_to_write = num_frames;
    uint32_t available_space = buffer->size - buffer->available;

    if (frames_to_write > available_space) {
        frames_to_write = available_space;
    }

    uint32_t frames_written = 0;

    // 링 버퍼 쓰기 (두 부분으로 나뉠 수 있음)
    while (frames_written < frames_to_write) {
        uint32_t chunk_size = frames_to_write - frames_written;
        uint32_t space_to_end = buffer->size - buffer->write_pos;

        if (chunk_size > space_to_end) {
            chunk_size = space_to_end;
        }

        memcpy(&buffer->data[buffer->write_pos], &data[frames_written],
               chunk_size * sizeof(float));

        buffer->write_pos = (buffer->write_pos + chunk_size) % buffer->size;
        frames_written += chunk_size;
    }

    buffer->available += frames_written;

    if (buffer->available == buffer->size) {
        buffer->is_full = true;
    }

    return frames_written;
}

uint32_t et_audio_buffer_read(ETAudioBuffer* buffer, float* data, uint32_t num_frames) {
    if (!buffer || !data || num_frames == 0) {
        return 0;
    }

    if (buffer->available == 0) {
        return 0; // 읽을 데이터가 없음
    }

    uint32_t frames_to_read = num_frames;

    if (frames_to_read > buffer->available) {
        frames_to_read = buffer->available;
    }

    uint32_t frames_read = 0;

    // 링 버퍼 읽기 (두 부분으로 나뉠 수 있음)
    while (frames_read < frames_to_read) {
        uint32_t chunk_size = frames_to_read - frames_read;
        uint32_t data_to_end = buffer->size - buffer->read_pos;

        if (chunk_size > data_to_end) {
            chunk_size = data_to_end;
        }

        memcpy(&data[frames_read], &buffer->data[buffer->read_pos],
               chunk_size * sizeof(float));

        buffer->read_pos = (buffer->read_pos + chunk_size) % buffer->size;
        frames_read += chunk_size;
    }

    buffer->available -= frames_read;
    buffer->is_full = false;

    return frames_read;
}

void et_audio_buffer_reset(ETAudioBuffer* buffer) {
    if (!buffer) {
        return;
    }

    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->available = 0;
    buffer->is_full = false;

    // 버퍼 데이터 초기화 (선택적)
    if (buffer->data) {
        memset(buffer->data, 0, buffer->size * sizeof(float));
    }
}

uint32_t et_audio_buffer_available_space(const ETAudioBuffer* buffer) {
    if (!buffer) {
        return 0;
    }

    return buffer->size - buffer->available;
}

uint32_t et_audio_buffer_available_data(const ETAudioBuffer* buffer) {
    if (!buffer) {
        return 0;
    }

    return buffer->available;
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * @brief 오디오 샘플 클리핑 (범위: -1.0 ~ 1.0)
 * @param sample 입력 샘플
 * @return 클리핑된 샘플
 */
static inline float audio_clip_sample(float sample) {
    if (sample > 1.0f) return 1.0f;
    if (sample < -1.0f) return -1.0f;
    return sample;
}

/**
 * @brief 오디오 버퍼 클리핑
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 */
void et_audio_clip_buffer(float* buffer, uint32_t num_samples) {
    if (!buffer) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] = audio_clip_sample(buffer[i]);
    }
}

/**
 * @brief 오디오 버퍼 볼륨 조절
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @param volume 볼륨 (0.0 ~ 1.0)
 */
void et_audio_apply_volume(float* buffer, uint32_t num_samples, float volume) {
    if (!buffer || volume < 0.0f) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] *= volume;
    }
}

/**
 * @brief 오디오 버퍼 믹싱
 * @param dest 대상 버퍼
 * @param src 소스 버퍼
 * @param num_samples 샘플 수
 * @param mix_ratio 믹싱 비율 (0.0 ~ 1.0)
 */
void et_audio_mix_buffers(float* dest, const float* src, uint32_t num_samples, float mix_ratio) {
    if (!dest || !src || mix_ratio < 0.0f || mix_ratio > 1.0f) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        dest[i] = dest[i] * (1.0f - mix_ratio) + src[i] * mix_ratio;
    }
}

/**
 * @brief 오디오 버퍼 페이드 인/아웃
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @param fade_in true면 페이드 인, false면 페이드 아웃
 */
void et_audio_fade_buffer(float* buffer, uint32_t num_samples, bool fade_in) {
    if (!buffer || num_samples == 0) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        float factor;
        if (fade_in) {
            factor = (float)i / (float)(num_samples - 1);
        } else {
            factor = (float)(num_samples - 1 - i) / (float)(num_samples - 1);
        }
        buffer[i] *= factor;
    }
}

// 모듈 정리 함수 (프로그램 종료 시 호출)
void et_audio_cleanup(void) {
    audio_system_cleanup();
}