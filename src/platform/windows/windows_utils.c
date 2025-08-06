/**
 * @file windows_utils.c
 * @brief Windows 플랫폼 유틸리티 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/types.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

// ============================================================================
// Windows 오류 매핑 테이블
// ============================================================================

static const ETErrorMapping windows_audio_error_mappings[] = {
    // DirectSound 오류들
    {DSERR_ALLOCATED, ET_ERROR_HARDWARE, "디바이스가 이미 할당됨"},
    {DSERR_BADFORMAT, ET_ERROR_UNSUPPORTED, "지원되지 않는 오디오 포맷"},
    {DSERR_BUFFERLOST, ET_ERROR_HARDWARE, "오디오 버퍼 손실"},
    {DSERR_CONTROLUNAVAIL, ET_ERROR_UNSUPPORTED, "제어 기능 사용 불가"},
    {DSERR_INVALIDCALL, ET_ERROR_INVALID_STATE, "잘못된 함수 호출"},
    {DSERR_INVALIDPARAM, ET_ERROR_INVALID_ARGUMENT, "잘못된 매개변수"},
    {DSERR_NOAGGREGATION, ET_ERROR_UNSUPPORTED, "집계 지원 안함"},
    {DSERR_NODRIVER, ET_ERROR_NOT_FOUND, "오디오 드라이버 없음"},
    {DSERR_OTHERAPPHASPRIO, ET_ERROR_HARDWARE, "다른 앱이 우선권 보유"},
    {DSERR_OUTOFMEMORY, ET_ERROR_OUT_OF_MEMORY, "메모리 부족"},
    {DSERR_PRIOLEVELNEEDED, ET_ERROR_HARDWARE, "우선순위 레벨 필요"},
    {DSERR_UNINITIALIZED, ET_ERROR_NOT_INITIALIZED, "초기화되지 않음"},
    {DSERR_UNSUPPORTED, ET_ERROR_UNSUPPORTED, "지원되지 않는 기능"},

    // WASAPI 오류들
    {AUDCLNT_E_NOT_INITIALIZED, ET_ERROR_NOT_INITIALIZED, "오디오 클라이언트 초기화되지 않음"},
    {AUDCLNT_E_ALREADY_INITIALIZED, ET_ERROR_ALREADY_INITIALIZED, "오디오 클라이언트 이미 초기화됨"},
    {AUDCLNT_E_WRONG_ENDPOINT_TYPE, ET_ERROR_INVALID_ARGUMENT, "잘못된 엔드포인트 타입"},
    {AUDCLNT_E_DEVICE_INVALIDATED, ET_ERROR_NOT_FOUND, "오디오 디바이스 무효화됨"},
    {AUDCLNT_E_NOT_STOPPED, ET_ERROR_INVALID_STATE, "오디오 스트림이 정지되지 않음"},
    {AUDCLNT_E_BUFFER_TOO_LARGE, ET_ERROR_INVALID_ARGUMENT, "오디오 버퍼가 너무 큼"},
    {AUDCLNT_E_OUT_OF_ORDER, ET_ERROR_INVALID_STATE, "잘못된 순서로 호출됨"},
    {AUDCLNT_E_UNSUPPORTED_FORMAT, ET_ERROR_UNSUPPORTED, "지원되지 않는 오디오 포맷"},
    {AUDCLNT_E_INVALID_SIZE, ET_ERROR_INVALID_ARGUMENT, "잘못된 크기"},
    {AUDCLNT_E_DEVICE_IN_USE, ET_ERROR_HARDWARE, "오디오 디바이스 사용 중"},
    {AUDCLNT_E_BUFFER_OPERATION_PENDING, ET_ERROR_INVALID_STATE, "버퍼 작업 대기 중"},
    {AUDCLNT_E_THREAD_NOT_REGISTERED, ET_ERROR_INVALID_STATE, "스레드가 등록되지 않음"},
    {AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED, ET_ERROR_UNSUPPORTED, "독점 모드 허용되지 않음"},
    {AUDCLNT_E_ENDPOINT_CREATE_FAILED, ET_ERROR_HARDWARE, "엔드포인트 생성 실패"},
    {AUDCLNT_E_SERVICE_NOT_RUNNING, ET_ERROR_HARDWARE, "오디오 서비스 실행되지 않음"},
    {AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED, ET_ERROR_INVALID_ARGUMENT, "예상되지 않은 이벤트 핸들"},
    {AUDCLNT_E_EXCLUSIVE_MODE_ONLY, ET_ERROR_UNSUPPORTED, "독점 모드만 지원됨"},
    {AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL, ET_ERROR_INVALID_ARGUMENT, "버퍼 지속시간과 주기가 일치하지 않음"},
    {AUDCLNT_E_EVENTHANDLE_NOT_SET, ET_ERROR_INVALID_STATE, "이벤트 핸들이 설정되지 않음"},
    {AUDCLNT_E_INCORRECT_BUFFER_SIZE, ET_ERROR_INVALID_ARGUMENT, "잘못된 버퍼 크기"},
    {AUDCLNT_E_BUFFER_SIZE_ERROR, ET_ERROR_INVALID_ARGUMENT, "버퍼 크기 오류"},
    {AUDCLNT_E_CPUUSAGE_EXCEEDED, ET_ERROR_HARDWARE, "CPU 사용량 초과"},

    // WaveOut/WaveIn 오류들
    {MMSYSERR_ERROR, ET_ERROR_HARDWARE, "일반적인 오류"},
    {MMSYSERR_BADDEVICEID, ET_ERROR_NOT_FOUND, "잘못된 디바이스 ID"},
    {MMSYSERR_NOTENABLED, ET_ERROR_HARDWARE, "드라이버가 활성화되지 않음"},
    {MMSYSERR_ALLOCATED, ET_ERROR_HARDWARE, "디바이스가 이미 할당됨"},
    {MMSYSERR_INVALHANDLE, ET_ERROR_INVALID_ARGUMENT, "잘못된 핸들"},
    {MMSYSERR_NODRIVER, ET_ERROR_NOT_FOUND, "드라이버가 설치되지 않음"},
    {MMSYSERR_NOMEM, ET_ERROR_OUT_OF_MEMORY, "메모리 부족"},
    {WAVERR_BADFORMAT, ET_ERROR_UNSUPPORTED, "지원되지 않는 웨이브 포맷"},
    {WAVERR_STILLPLAYING, ET_ERROR_INVALID_STATE, "여전히 재생 중"},
    {WAVERR_UNPREPARED, ET_ERROR_NOT_INITIALIZED, "헤더가 준비되지 않음"},

    // 일반 Windows 오류들
    {ERROR_SUCCESS, ET_SUCCESS, "성공"},
    {ERROR_INVALID_PARAMETER, ET_ERROR_INVALID_ARGUMENT, "잘못된 매개변수"},
    {ERROR_NOT_ENOUGH_MEMORY, ET_ERROR_OUT_OF_MEMORY, "메모리 부족"},
    {ERROR_INVALID_HANDLE, ET_ERROR_INVALID_ARGUMENT, "잘못된 핸들"},
    {ERROR_NOT_SUPPORTED, ET_ERROR_UNSUPPORTED, "지원되지 않는 기능"},
    {ERROR_DEVICE_NOT_CONNECTED, ET_ERROR_NOT_FOUND, "디바이스가 연결되지 않음"},
    {ERROR_ACCESS_DENIED, ET_ERROR_HARDWARE, "접근 거부됨"},

    // HRESULT 공통 오류들
    {S_OK, ET_SUCCESS, "성공"},
    {E_INVALIDARG, ET_ERROR_INVALID_ARGUMENT, "잘못된 인수"},
    {E_OUTOFMEMORY, ET_ERROR_OUT_OF_MEMORY, "메모리 부족"},
    {E_FAIL, ET_ERROR_HARDWARE, "일반적인 실패"},
    {E_NOTIMPL, ET_ERROR_NOT_IMPLEMENTED, "구현되지 않음"},
    {E_NOINTERFACE, ET_ERROR_UNSUPPORTED, "인터페이스 지원 안함"},
    {E_POINTER, ET_ERROR_INVALID_ARGUMENT, "잘못된 포인터"},
    {E_HANDLE, ET_ERROR_INVALID_ARGUMENT, "잘못된 핸들"},
    {E_ABORT, ET_ERROR_RUNTIME, "작업 중단됨"},
    {E_ACCESSDENIED, ET_ERROR_HARDWARE, "접근 거부됨"}
};

static const size_t windows_audio_error_mapping_count =
    sizeof(windows_audio_error_mappings) / sizeof(windows_audio_error_mappings[0]);

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief Windows 오류 코드를 공통 오류 코드로 변환
 */
ETResult et_windows_error_to_common(int windows_error) {
    for (size_t i = 0; i < windows_audio_error_mapping_count; i++) {
        if (windows_audio_error_mappings[i].platform_error == windows_error) {
            return windows_audio_error_mappings[i].common_error;
        }
    }

    // 매핑되지 않은 오류는 일반적인 하드웨어 오류로 처리
    return ET_ERROR_HARDWARE;
}

/**
 * @brief Windows 오류 설명 가져오기
 */
const char* et_get_windows_error_description(int windows_error) {
    for (size_t i = 0; i < windows_audio_error_mapping_count; i++) {
        if (windows_audio_error_mappings[i].platform_error == windows_error) {
            return windows_audio_error_mappings[i].description;
        }
    }

    return "알 수 없는 Windows 오류";
}

/**
 * @brief Windows 시스템 오류 메시지 가져오기
 */
void et_get_windows_system_error_message(DWORD error_code, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }

    DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        (DWORD)buffer_size,
        NULL
    );

    if (result == 0) {
        snprintf(buffer, buffer_size, "Windows 오류 코드: %lu", error_code);
    } else {
        // 개행 문자 제거
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) {
            buffer[--len] = '\0';
        }
    }
}

#else
// Windows가 아닌 플랫폼에서는 빈 구현
ETResult et_windows_error_to_common(int windows_error) {
    (void)windows_error;
    return ET_ERROR_NOT_IMPLEMENTED;
}

const char* et_get_windows_error_description(int windows_error) {
    (void)windows_error;
    return "Windows 플랫폼이 아님";
}

void et_get_windows_system_error_message(unsigned long error_code, char* buffer, size_t buffer_size) {
    (void)error_code;
    if (buffer && buffer_size > 0) {
        snprintf(buffer, buffer_size, "Windows 플랫폼이 아님");
    }
}
#endif