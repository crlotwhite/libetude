/**
 * @file macos_audio.c
 * @brief macOS CoreAudio 구현체 - 플랫폼 추상화 인터페이스 통합
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/platform/audio.h"
#include "libetude/platform/common.h"
#include "libetude/platform/macos_compat.h"
#include "libetude/error.h"
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__

// macOS 호환성 설정 적용
LIBETUDE_SUPPRESS_BLOCK_WARNINGS

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <pthread.h>

// ============================================================================
// macOS 오디오 디바이스 구조체
// ============================================================================

/**
 * @brief macOS 오디오 디바이스 구조체
 */
typedef struct ETMacOSAudioDevice {
    // 공통 필드
    ETAudioFormat format;           /**< 오디오 포맷 */
    ETAudioDeviceType type;         /**< 디바이스 타입 */
    ETAudioState state;             /**< 디바이스 상태 */
    ETAudioCallback callback;       /**< 오디오 콜백 */
    void* user_data;                /**< 사용자 데이터 */

    // macOS 특화 필드
    AudioDeviceID device_id;        /**< CoreAudio 디바이스 ID */
    AudioUnit audio_unit;           /**< AudioUnit 인스턴스 */
    AudioStreamBasicDescription stream_format; /**< 스트림 포맷 */

    // 버퍼 관리
    ETAudioBuffer* ring_buffer;     /**< 링 버퍼 */
    float* temp_buffer;             /**< 임시 버퍼 */
    uint32_t temp_buffer_size;      /**< 임시 버퍼 크기 */

    // 동기화
    pthread_mutex_t mutex;          /**< 뮤텍스 */
    bool is_initialized;            /**< 초기화 여부 */
} ETMacOSAudioDevice;

// ============================================================================
// macOS 오류 코드 매핑
// ============================================================================

/**
 * @brief macOS OSStatus를 공통 오류 코드로 매핑
 */
static ETResult macos_map_osstatus_to_result(OSStatus status) {
    switch (status) {
        case noErr:
            return ET_SUCCESS;
        case kAudioHardwareNotRunningError:
            return ET_ERROR_HARDWARE;
        case kAudioHardwareUnknownPropertyError:
            return ET_ERROR_UNSUPPORTED;
        case kAudioDeviceUnsupportedFormatError:
            return ET_ERROR_INVALID_ARGUMENT;
        case kAudioHardwareIllegalOperationError:
            return ET_ERROR_INVALID_STATE;
        case kAudioHardwareBadDeviceError:
            return ET_ERROR_NOT_FOUND;

        default:
            return ET_ERROR_UNKNOWN;
    }
}

/**
 * @brief OSStatus 오류 메시지 생성
 */
static const char* macos_get_osstatus_message(OSStatus status) {
    switch (status) {
        case noErr:
            return "성공";
        case kAudioHardwareNotRunningError:
            return "오디오 하드웨어가 실행되지 않음";
        case kAudioHardwareUnknownPropertyError:
            return "알 수 없는 오디오 속성";
        case kAudioDeviceUnsupportedFormatError:
            return "지원되지 않는 오디오 포맷";
        case kAudioHardwareIllegalOperationError:
            return "잘못된 오디오 작업";
        case kAudioHardwareBadDeviceError:
            return "잘못된 오디오 디바이스";
        default:
            return "알 수 없는 CoreAudio 오류";
    }
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * @brief ETAudioFormat을 AudioStreamBasicDescription으로 변환
 */
static void et_format_to_asbd(const ETAudioFormat* format, AudioStreamBasicDescription* asbd) {
    memset(asbd, 0, sizeof(AudioStreamBasicDescription));

    asbd->mSampleRate = format->sample_rate;
    asbd->mFormatID = kAudioFormatLinearPCM;
    asbd->mChannelsPerFrame = format->num_channels;

    if (format->is_float) {
        asbd->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        asbd->mBitsPerChannel = 32;
        asbd->mBytesPerFrame = format->num_channels * sizeof(float);
    } else {
        asbd->mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        asbd->mBitsPerChannel = format->bit_depth;
        asbd->mBytesPerFrame = format->num_channels * (format->bit_depth / 8);
    }

    asbd->mFramesPerPacket = 1;
    asbd->mBytesPerPacket = asbd->mBytesPerFrame;
}

/**
 * @brief AudioStreamBasicDescription을 ETAudioFormat으로 변환
 */
static void asbd_to_et_format(const AudioStreamBasicDescription* asbd, ETAudioFormat* format) {
    memset(format, 0, sizeof(ETAudioFormat));

    format->sample_rate = (uint32_t)asbd->mSampleRate;
    format->num_channels = (uint16_t)asbd->mChannelsPerFrame;
    format->is_float = (asbd->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
    format->bit_depth = (uint16_t)asbd->mBitsPerChannel;
    format->frame_size = asbd->mBytesPerFrame;
    format->buffer_size = 512; // 기본값
}

/**
 * @brief 기본 출력 디바이스 ID 조회
 */
static AudioDeviceID macos_get_default_output_device(void) {
    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &property_address,
                                               0, NULL,
                                               &size, &device_id);

    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "기본 출력 디바이스 조회 실패: %s",
                     macos_get_osstatus_message(status));
        return kAudioObjectUnknown;
    }

    return device_id;
}

/**
 * @brief 기본 입력 디바이스 ID 조회
 */
static AudioDeviceID macos_get_default_input_device(void) {
    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &property_address,
                                               0, NULL,
                                               &size, &device_id);

    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "기본 입력 디바이스 조회 실패: %s",
                     macos_get_osstatus_message(status));
        return kAudioObjectUnknown;
    }

    return device_id;
}

/**
 * @brief 디바이스 이름으로 디바이스 ID 찾기
 */
static AudioDeviceID macos_find_device_by_name(const char* device_name, ETAudioDeviceType type) {
    if (!device_name) {
        return (type == ET_AUDIO_DEVICE_OUTPUT) ?
               macos_get_default_output_device() :
               macos_get_default_input_device();
    }

    // 디바이스 목록 조회
    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                   &property_address,
                                                   0, NULL, &data_size);
    if (status != noErr) {
        return kAudioObjectUnknown;
    }

    UInt32 device_count = data_size / sizeof(AudioDeviceID);
    AudioDeviceID* devices = (AudioDeviceID*)malloc(data_size);
    if (!devices) {
        return kAudioObjectUnknown;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &property_address,
                                      0, NULL, &data_size, devices);
    if (status != noErr) {
        free(devices);
        return kAudioObjectUnknown;
    }

    // 각 디바이스의 이름 확인
    for (UInt32 i = 0; i < device_count; i++) {
        CFStringRef device_name_ref = NULL;
        UInt32 size = sizeof(device_name_ref);

        AudioObjectPropertyAddress name_address = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        status = AudioObjectGetPropertyData(devices[i], &name_address,
                                          0, NULL, &size, &device_name_ref);
        if (status == noErr && device_name_ref) {
            char name_buffer[256];
            if (CFStringGetCString(device_name_ref, name_buffer, sizeof(name_buffer),
                                 kCFStringEncodingUTF8)) {
                if (strcmp(name_buffer, device_name) == 0) {
                    CFRelease(device_name_ref);
                    AudioDeviceID found_device = devices[i];
                    free(devices);
                    return found_device;
                }
            }
            CFRelease(device_name_ref);
        }
    }

    free(devices);
    return kAudioObjectUnknown;
}

// ============================================================================
// AudioUnit 콜백 함수들
// ============================================================================

/**
 * @brief 출력용 AudioUnit 렌더 콜백
 */
static OSStatus macos_output_render_callback(void* inRefCon,
                                           AudioUnitRenderActionFlags* ioActionFlags,
                                           const AudioTimeStamp* inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList* ioData) {
    ETMacOSAudioDevice* device = (ETMacOSAudioDevice*)inRefCon;

    if (!device || !device->callback || device->state != ET_AUDIO_STATE_RUNNING) {
        // 무음 출력
        for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
            memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
        }
        return noErr;
    }

    pthread_mutex_lock(&device->mutex);

    // 콜백 호출
    float* buffer = (float*)ioData->mBuffers[0].mData;
    device->callback(buffer, inNumberFrames, device->user_data);

    pthread_mutex_unlock(&device->mutex);

    return noErr;
}

/**
 * @brief 입력용 AudioUnit 입력 콜백
 */
static OSStatus macos_input_callback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData) {
    ETMacOSAudioDevice* device = (ETMacOSAudioDevice*)inRefCon;

    if (!device || !device->callback || device->state != ET_AUDIO_STATE_RUNNING) {
        return noErr;
    }

    // 입력 데이터 렌더링
    AudioBufferList buffer_list;
    buffer_list.mNumberBuffers = 1;
    buffer_list.mBuffers[0].mNumberChannels = device->format.num_channels;
    buffer_list.mBuffers[0].mDataByteSize = inNumberFrames * device->format.frame_size;
    buffer_list.mBuffers[0].mData = device->temp_buffer;

    OSStatus status = AudioUnitRender(device->audio_unit, ioActionFlags, inTimeStamp,
                                    inBusNumber, inNumberFrames, &buffer_list);
    if (status != noErr) {
        return status;
    }

    pthread_mutex_lock(&device->mutex);

    // 콜백 호출
    device->callback(device->temp_buffer, inNumberFrames, device->user_data);

    pthread_mutex_unlock(&device->mutex);

    return noErr;
}

// ============================================================================
// 디바이스 생성 및 관리 함수들
// ============================================================================

/**
 * @brief macOS 오디오 디바이스 생성
 */
static ETMacOSAudioDevice* macos_create_audio_device(const char* device_name,
                                                    const ETAudioFormat* format,
                                                    ETAudioDeviceType type) {
    ETMacOSAudioDevice* device = (ETMacOSAudioDevice*)calloc(1, sizeof(ETMacOSAudioDevice));
    if (!device) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "오디오 디바이스 메모리 할당 실패");
        return NULL;
    }

    // 기본 필드 초기화
    device->format = *format;
    device->type = type;
    device->state = ET_AUDIO_STATE_STOPPED;
    device->callback = NULL;
    device->user_data = NULL;
    device->device_id = kAudioObjectUnknown;
    device->audio_unit = NULL;
    device->is_initialized = false;

    // 뮤텍스 초기화
    if (pthread_mutex_init(&device->mutex, NULL) != 0) {
        free(device);
        ET_SET_ERROR(ET_ERROR_RUNTIME, "뮤텍스 초기화 실패");
        return NULL;
    }

    // 디바이스 ID 찾기
    device->device_id = macos_find_device_by_name(device_name, type);
    if (device->device_id == kAudioObjectUnknown) {
        pthread_mutex_destroy(&device->mutex);
        free(device);
        ET_SET_ERROR(ET_ERROR_NOT_FOUND, "오디오 디바이스를 찾을 수 없음: %s",
                     device_name ? device_name : "기본 디바이스");
        return NULL;
    }

    // AudioStreamBasicDescription 설정
    et_format_to_asbd(format, &device->stream_format);

    // 링 버퍼 생성
    device->ring_buffer = et_audio_buffer_create(format->buffer_size * 4,
                                                format->num_channels, true);
    if (!device->ring_buffer) {
        pthread_mutex_destroy(&device->mutex);
        free(device);
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "링 버퍼 생성 실패");
        return NULL;
    }

    // 임시 버퍼 생성 (입력용)
    if (type == ET_AUDIO_DEVICE_INPUT) {
        device->temp_buffer_size = format->buffer_size * format->num_channels;
        device->temp_buffer = (float*)malloc(device->temp_buffer_size * sizeof(float));
        if (!device->temp_buffer) {
            et_audio_buffer_destroy(device->ring_buffer);
            pthread_mutex_destroy(&device->mutex);
            free(device);
            ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "임시 버퍼 생성 실패");
            return NULL;
        }
    }

    return device;
}

/**
 * @brief macOS 오디오 디바이스 해제
 */
static void macos_destroy_audio_device(ETMacOSAudioDevice* device) {
    if (!device) {
        return;
    }

    // AudioUnit 정리
    if (device->audio_unit) {
        AudioUnitUninitialize(device->audio_unit);
        AudioComponentInstanceDispose(device->audio_unit);
    }

    // 버퍼 해제
    if (device->ring_buffer) {
        et_audio_buffer_destroy(device->ring_buffer);
    }
    if (device->temp_buffer) {
        free(device->temp_buffer);
    }

    // 뮤텍스 해제
    pthread_mutex_destroy(&device->mutex);

    free(device);
}

/**
 * @brief AudioUnit 초기화
 */
static ETResult macos_initialize_audio_unit(ETMacOSAudioDevice* device) {
    OSStatus status;
    AudioComponent component;
    AudioComponentDescription desc;

    // AudioComponent 설정
    memset(&desc, 0, sizeof(desc));
    if (device->type == ET_AUDIO_DEVICE_OUTPUT) {
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    } else {
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
    }
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    // AudioComponent 찾기
    component = AudioComponentFindNext(NULL, &desc);
    if (!component) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "AudioComponent를 찾을 수 없음");
        return ET_ERROR_HARDWARE;
    }

    // AudioUnit 인스턴스 생성
    status = AudioComponentInstanceNew(component, &device->audio_unit);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "AudioUnit 생성 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    // 입력 디바이스의 경우 입력 활성화
    if (device->type == ET_AUDIO_DEVICE_INPUT) {
        UInt32 enable_input = 1;
        status = AudioUnitSetProperty(device->audio_unit,
                                    kAudioOutputUnitProperty_EnableIO,
                                    kAudioUnitScope_Input, 1,
                                    &enable_input, sizeof(enable_input));
        if (status != noErr) {
            ET_SET_ERROR(ET_ERROR_HARDWARE, "입력 활성화 실패: %s",
                         macos_get_osstatus_message(status));
            return macos_map_osstatus_to_result(status);
        }

        // 출력 비활성화
        UInt32 disable_output = 0;
        status = AudioUnitSetProperty(device->audio_unit,
                                    kAudioOutputUnitProperty_EnableIO,
                                    kAudioUnitScope_Output, 0,
                                    &disable_output, sizeof(disable_output));
        if (status != noErr) {
            ET_SET_ERROR(ET_ERROR_HARDWARE, "출력 비활성화 실패: %s",
                         macos_get_osstatus_message(status));
            return macos_map_osstatus_to_result(status);
        }
    }

    // 디바이스 설정
    status = AudioUnitSetProperty(device->audio_unit,
                                kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global, 0,
                                &device->device_id, sizeof(device->device_id));
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "디바이스 설정 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    // 스트림 포맷 설정
    UInt32 scope = (device->type == ET_AUDIO_DEVICE_OUTPUT) ?
                   kAudioUnitScope_Input : kAudioUnitScope_Output;
    UInt32 element = (device->type == ET_AUDIO_DEVICE_OUTPUT) ? 0 : 1;

    status = AudioUnitSetProperty(device->audio_unit,
                                kAudioUnitProperty_StreamFormat,
                                scope, element,
                                &device->stream_format, sizeof(device->stream_format));
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "스트림 포맷 설정 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    // 콜백 설정
    AURenderCallbackStruct callback_struct;
    callback_struct.inputProcRefCon = device;

    if (device->type == ET_AUDIO_DEVICE_OUTPUT) {
        callback_struct.inputProc = macos_output_render_callback;
        status = AudioUnitSetProperty(device->audio_unit,
                                    kAudioUnitProperty_SetRenderCallback,
                                    kAudioUnitScope_Input, 0,
                                    &callback_struct, sizeof(callback_struct));
    } else {
        callback_struct.inputProc = macos_input_callback;
        status = AudioUnitSetProperty(device->audio_unit,
                                    kAudioOutputUnitProperty_SetInputCallback,
                                    kAudioUnitScope_Global, 0,
                                    &callback_struct, sizeof(callback_struct));
    }

    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "콜백 설정 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    // AudioUnit 초기화
    status = AudioUnitInitialize(device->audio_unit);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "AudioUnit 초기화 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    device->is_initialized = true;
    return ET_SUCCESS;
}

// ============================================================================
// 플랫폼 추상화 인터페이스 구현
// ============================================================================

/**
 * @brief 출력 디바이스 열기
 */
static ETResult macos_open_output_device(const char* device_name,
                                       const ETAudioFormat* format,
                                       ETAudioDevice** device) {
    if (!format || !device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "잘못된 인수");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!et_audio_format_validate(format)) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "잘못된 오디오 포맷");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = macos_create_audio_device(device_name, format,
                                                               ET_AUDIO_DEVICE_OUTPUT);
    if (!macos_device) {
        return ET_ERROR_NOT_FOUND; // 오류는 이미 설정됨
    }

    ETResult result = macos_initialize_audio_unit(macos_device);
    if (result != ET_SUCCESS) {
        macos_destroy_audio_device(macos_device);
        return result;
    }

    *device = (ETAudioDevice*)macos_device;
    return ET_SUCCESS;
}

/**
 * @brief 입력 디바이스 열기
 */
static ETResult macos_open_input_device(const char* device_name,
                                      const ETAudioFormat* format,
                                      ETAudioDevice** device) {
    if (!format || !device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "잘못된 인수");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!et_audio_format_validate(format)) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "잘못된 오디오 포맷");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = macos_create_audio_device(device_name, format,
                                                               ET_AUDIO_DEVICE_INPUT);
    if (!macos_device) {
        return ET_ERROR_NOT_FOUND; // 오류는 이미 설정됨
    }

    ETResult result = macos_initialize_audio_unit(macos_device);
    if (result != ET_SUCCESS) {
        macos_destroy_audio_device(macos_device);
        return result;
    }

    *device = (ETAudioDevice*)macos_device;
    return ET_SUCCESS;
}

/**
 * @brief 디바이스 닫기
 */
static void macos_close_device(ETAudioDevice* device) {
    if (!device) {
        return;
    }

    ETMacOSAudioDevice* macos_device = (ETMacOSAudioDevice*)device;

    // 스트림 정지
    if (macos_device->state == ET_AUDIO_STATE_RUNNING) {
        AudioOutputUnitStop(macos_device->audio_unit);
    }

    macos_destroy_audio_device(macos_device);
}

/**
 * @brief 스트림 시작
 */
static ETResult macos_start_stream(ETAudioDevice* device) {
    if (!device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "디바이스가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = (ETMacOSAudioDevice*)device;

    if (!macos_device->is_initialized) {
        ET_SET_ERROR(ET_ERROR_INVALID_STATE, "디바이스가 초기화되지 않음");
        return ET_ERROR_INVALID_STATE;
    }

    if (macos_device->state == ET_AUDIO_STATE_RUNNING) {
        return ET_SUCCESS; // 이미 실행 중
    }

    OSStatus status = AudioOutputUnitStart(macos_device->audio_unit);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "스트림 시작 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    macos_device->state = ET_AUDIO_STATE_RUNNING;
    return ET_SUCCESS;
}

/**
 * @brief 스트림 정지
 */
static ETResult macos_stop_stream(ETAudioDevice* device) {
    if (!device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "디바이스가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = (ETMacOSAudioDevice*)device;

    if (macos_device->state == ET_AUDIO_STATE_STOPPED) {
        return ET_SUCCESS; // 이미 정지됨
    }

    OSStatus status = AudioOutputUnitStop(macos_device->audio_unit);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "스트림 정지 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    macos_device->state = ET_AUDIO_STATE_STOPPED;
    return ET_SUCCESS;
}

/**
 * @brief 스트림 일시정지
 */
static ETResult macos_pause_stream(ETAudioDevice* device) {
    if (!device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "디바이스가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = (ETMacOSAudioDevice*)device;

    if (macos_device->state != ET_AUDIO_STATE_RUNNING) {
        ET_SET_ERROR(ET_ERROR_INVALID_STATE, "디바이스가 실행 중이 아님");
        return ET_ERROR_INVALID_STATE;
    }

    // macOS에서는 일시정지를 위해 정지 후 상태만 변경
    OSStatus status = AudioOutputUnitStop(macos_device->audio_unit);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "스트림 일시정지 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    macos_device->state = ET_AUDIO_STATE_PAUSED;
    return ET_SUCCESS;
}

/**
 * @brief 콜백 설정
 */
static ETResult macos_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data) {
    if (!device) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "디바이스가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSAudioDevice* macos_device = (ETMacOSAudioDevice*)device;

    pthread_mutex_lock(&macos_device->mutex);
    macos_device->callback = callback;
    macos_device->user_data = user_data;
    pthread_mutex_unlock(&macos_device->mutex);

    return ET_SUCCESS;
}

/**
 * @brief 디바이스 상태 조회
 */
static ETAudioState macos_get_state(const ETAudioDevice* device) {
    if (!device) {
        return ET_AUDIO_STATE_ERROR;
    }

    const ETMacOSAudioDevice* macos_device = (const ETMacOSAudioDevice*)device;
    return macos_device->state;
}

/**
 * @brief 지연시간 조회
 */
static uint32_t macos_get_latency(const ETAudioDevice* device) {
    if (!device) {
        return 0;
    }

    const ETMacOSAudioDevice* macos_device = (const ETMacOSAudioDevice*)device;

    // 디바이스 지연시간 조회
    UInt32 latency = 0;
    UInt32 size = sizeof(latency);

    AudioObjectPropertyAddress property_address = {
        kAudioDevicePropertyLatency,
        (macos_device->type == ET_AUDIO_DEVICE_INPUT) ?
            kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(macos_device->device_id,
                                               &property_address,
                                               0, NULL, &size, &latency);
    if (status != noErr) {
        return 0;
    }

    // 프레임을 밀리초로 변환
    return (uint32_t)((double)latency * 1000.0 / macos_device->format.sample_rate);
}

/**
 * @brief 디바이스 열거
 */
static ETResult macos_enumerate_devices(ETAudioDeviceType type, ETAudioDeviceInfo* devices, int* count) {
    if (!count) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "count가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 디바이스 목록 조회
    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                   &property_address,
                                                   0, NULL, &data_size);
    if (status != noErr) {
        ET_SET_ERROR(ET_ERROR_HARDWARE, "디바이스 목록 크기 조회 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    UInt32 device_count = data_size / sizeof(AudioDeviceID);
    AudioDeviceID* device_ids = (AudioDeviceID*)malloc(data_size);
    if (!device_ids) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "디바이스 ID 배열 메모리 할당 실패");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                      &property_address,
                                      0, NULL, &data_size, device_ids);
    if (status != noErr) {
        free(device_ids);
        ET_SET_ERROR(ET_ERROR_HARDWARE, "디바이스 목록 조회 실패: %s",
                     macos_get_osstatus_message(status));
        return macos_map_osstatus_to_result(status);
    }

    int found_count = 0;
    AudioDeviceID default_output = macos_get_default_output_device();
    AudioDeviceID default_input = macos_get_default_input_device();

    // 각 디바이스 정보 수집
    for (UInt32 i = 0; i < device_count && found_count < *count; i++) {
        AudioDeviceID device_id = device_ids[i];

        // 디바이스 타입 확인
        bool has_input = false, has_output = false;

        // 입력 스트림 확인
        AudioObjectPropertyAddress stream_address = {
            kAudioDevicePropertyStreams,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMain
        };

        UInt32 stream_size = 0;
        status = AudioObjectGetPropertyDataSize(device_id, &stream_address, 0, NULL, &stream_size);
        if (status == noErr && stream_size > 0) {
            has_input = true;
        }

        // 출력 스트림 확인
        stream_address.mScope = kAudioDevicePropertyScopeOutput;
        status = AudioObjectGetPropertyDataSize(device_id, &stream_address, 0, NULL, &stream_size);
        if (status == noErr && stream_size > 0) {
            has_output = true;
        }

        // 요청된 타입과 일치하는지 확인
        bool matches_type = false;
        ETAudioDeviceType device_type;

        if (type == ET_AUDIO_DEVICE_OUTPUT && has_output) {
            matches_type = true;
            device_type = ET_AUDIO_DEVICE_OUTPUT;
        } else if (type == ET_AUDIO_DEVICE_INPUT && has_input) {
            matches_type = true;
            device_type = ET_AUDIO_DEVICE_INPUT;
        } else if (type == ET_AUDIO_DEVICE_DUPLEX && has_input && has_output) {
            matches_type = true;
            device_type = ET_AUDIO_DEVICE_DUPLEX;
        }

        if (!matches_type) {
            continue;
        }

        // 디바이스 정보가 요청된 경우에만 채움
        if (devices && found_count < *count) {
            ETAudioDeviceInfo* info = &devices[found_count];
            memset(info, 0, sizeof(ETAudioDeviceInfo));

            // 디바이스 이름 조회
            CFStringRef device_name_ref = NULL;
            UInt32 name_size = sizeof(device_name_ref);

            AudioObjectPropertyAddress name_address = {
                kAudioDevicePropertyDeviceNameCFString,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };

            status = AudioObjectGetPropertyData(device_id, &name_address,
                                              0, NULL, &name_size, &device_name_ref);
            if (status == noErr && device_name_ref) {
                CFStringGetCString(device_name_ref, info->name, sizeof(info->name),
                                 kCFStringEncodingUTF8);
                CFRelease(device_name_ref);
            } else {
                snprintf(info->name, sizeof(info->name), "Unknown Device %u", device_id);
            }

            // 디바이스 ID를 문자열로 변환
            snprintf(info->id, sizeof(info->id), "%u", device_id);

            info->type = device_type;
            info->is_default = (device_id == default_output) || (device_id == default_input);

            // 최대 채널 수 조회
            AudioObjectPropertyAddress channel_address = {
                kAudioDevicePropertyStreamConfiguration,
                (type == ET_AUDIO_DEVICE_INPUT) ?
                    kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMain
            };

            UInt32 config_size = 0;
            status = AudioObjectGetPropertyDataSize(device_id, &channel_address, 0, NULL, &config_size);
            if (status == noErr && config_size > 0) {
                AudioBufferList* buffer_list = (AudioBufferList*)malloc(config_size);
                if (buffer_list) {
                    status = AudioObjectGetPropertyData(device_id, &channel_address,
                                                      0, NULL, &config_size, buffer_list);
                    if (status == noErr) {
                        UInt32 total_channels = 0;
                        for (UInt32 j = 0; j < buffer_list->mNumberBuffers; j++) {
                            total_channels += buffer_list->mBuffers[j].mNumberChannels;
                        }
                        info->max_channels = total_channels;
                    }
                    free(buffer_list);
                }
            }

            if (info->max_channels == 0) {
                info->max_channels = 2; // 기본값
            }

            // 지연시간 정보 (간단히 추정)
            info->min_latency = 5;   // 5ms
            info->max_latency = 100; // 100ms

            // 지원 샘플 레이트 (기본값들)
            static uint32_t default_rates[] = {44100, 48000, 88200, 96000};
            info->supported_rates = (uint32_t*)malloc(sizeof(default_rates));
            if (info->supported_rates) {
                memcpy(info->supported_rates, default_rates, sizeof(default_rates));
                info->rate_count = sizeof(default_rates) / sizeof(default_rates[0]);
            } else {
                info->rate_count = 0;
            }
        }

        found_count++;
    }

    free(device_ids);
    *count = found_count;
    return ET_SUCCESS;
}

/**
 * @brief 포맷 지원 확인
 */
static bool macos_is_format_supported(const char* device_name, const ETAudioFormat* format) {
    if (!format) {
        return false;
    }

    // 기본적인 포맷 검증
    if (!et_audio_format_validate(format)) {
        return false;
    }

    // macOS CoreAudio는 대부분의 표준 포맷을 지원
    // 실제로는 디바이스별로 더 정확한 확인이 필요하지만,
    // 여기서는 기본적인 범위 내에서 지원한다고 가정
    return (format->sample_rate >= 8000 && format->sample_rate <= 192000 &&
            format->num_channels >= 1 && format->num_channels <= 8 &&
            (format->bit_depth == 16 || format->bit_depth == 24 || format->bit_depth == 32));
}

/**
 * @brief 지원되는 포맷 목록 조회
 */
static ETResult macos_get_supported_formats(const char* device_name, ETAudioFormat* formats, int* count) {
    if (!count) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "count가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기본적으로 지원하는 포맷들
    static const struct {
        uint32_t sample_rate;
        uint16_t num_channels;
        uint16_t bit_depth;
        bool is_float;
    } supported_formats[] = {
        {44100, 1, 16, false}, {44100, 2, 16, false},
        {44100, 1, 32, true},  {44100, 2, 32, true},
        {48000, 1, 16, false}, {48000, 2, 16, false},
        {48000, 1, 32, true},  {48000, 2, 32, true},
        {88200, 1, 32, true},  {88200, 2, 32, true},
        {96000, 1, 32, true},  {96000, 2, 32, true}
    };

    int format_count = sizeof(supported_formats) / sizeof(supported_formats[0]);

    if (formats && *count > 0) {
        int copy_count = (*count < format_count) ? *count : format_count;

        for (int i = 0; i < copy_count; i++) {
            formats[i] = et_audio_format_create(supported_formats[i].sample_rate,
                                              supported_formats[i].num_channels,
                                              512); // 기본 버퍼 크기
            formats[i].bit_depth = supported_formats[i].bit_depth;
            formats[i].is_float = supported_formats[i].is_float;
            formats[i].frame_size = supported_formats[i].num_channels *
                                  (supported_formats[i].is_float ? sizeof(float) :
                                   (supported_formats[i].bit_depth / 8));
        }
    }

    *count = format_count;
    return ET_SUCCESS;
}

// ============================================================================
// 인터페이스 구조체 생성
// ============================================================================

/**
 * @brief macOS 오디오 인터페이스 생성
 */
ETAudioInterface* et_create_macos_audio_interface(void) {
    ETAudioInterface* interface = (ETAudioInterface*)malloc(sizeof(ETAudioInterface));
    if (!interface) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "오디오 인터페이스 메모리 할당 실패");
        return NULL;
    }

    // 함수 포인터 설정
    interface->open_output_device = macos_open_output_device;
    interface->open_input_device = macos_open_input_device;
    interface->close_device = macos_close_device;
    interface->start_stream = macos_start_stream;
    interface->stop_stream = macos_stop_stream;
    interface->pause_stream = macos_pause_stream;
    interface->set_callback = macos_set_callback;
    interface->enumerate_devices = macos_enumerate_devices;
    interface->get_latency = macos_get_latency;
    interface->get_state = macos_get_state;
    interface->is_format_supported = macos_is_format_supported;
    interface->get_supported_formats = macos_get_supported_formats;
    interface->platform_data = NULL;

    return interface;
}

/**
 * @brief macOS 오디오 인터페이스 해제
 */
void et_destroy_macos_audio_interface(ETAudioInterface* interface) {
    if (interface) {
        free(interface);
    }
}

#else
// macOS가 아닌 플랫폼에서는 빈 구현
ETAudioInterface* et_create_macos_audio_interface(void) {
    return NULL;
}

void et_destroy_macos_audio_interface(ETAudioInterface* interface) {
    (void)interface;
}

// 경고 복원
LIBETUDE_RESTORE_WARNINGS

#endif // __APPLE__