/* LibEtude Windows 오류 처리 시스템 사용 예제 */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows.h"
#include "libetude/platform/windows_error.h"
#include <stdio.h>
#include <stdlib.h>

/* 사용자 정의 오류 콜백 */
static void custom_error_callback(const ETWindowsErrorInfo* error_info, void* user_data) {
    const char* app_name = (const char*)user_data;

    printf("\n[%s] 오류 발생!\n", app_name);
    printf("오류 코드: 0x%X\n", error_info->error_code);
    printf("메시지: %s\n", et_windows_get_error_message_korean(error_info->error_code));
    printf("모듈: %s, 함수: %s, 라인: %d\n",
           error_info->module_name, error_info->function_name, error_info->line_number);

    if (strlen(error_info->technical_details) > 0) {
        printf("세부사항: %s\n", error_info->technical_details);
    }

    /* 심각도에 따른 처리 */
    switch (error_info->severity) {
        case ET_WINDOWS_ERROR_SEVERITY_CRITICAL:
            printf("치명적 오류입니다. 애플리케이션을 종료합니다.\n");
            break;
        case ET_WINDOWS_ERROR_SEVERITY_ERROR:
            printf("오류가 발생했지만 계속 실행할 수 있습니다.\n");
            break;
        case ET_WINDOWS_ERROR_SEVERITY_WARNING:
            printf("경고: 일부 기능이 제한될 수 있습니다.\n");
            break;
        case ET_WINDOWS_ERROR_SEVERITY_INFO:
            printf("정보: 참고용 메시지입니다.\n");
            break;
    }
}

/* 사용자 정의 WASAPI 폴백 콜백 */
static ETResult custom_wasapi_fallback(ETWindowsErrorCode error_code, void* context) {
    printf("\n사용자 정의 WASAPI 폴백 실행 중...\n");
    printf("오류 코드: 0x%X (%s)\n", error_code,
           et_windows_get_error_message_korean(error_code));

    /* 실제 폴백 로직 구현 */
    printf("DirectSound로 폴백 시도 중...\n");

    /* 여기서 실제 DirectSound 초기화를 수행 */
    /* ETAudioDevice* device = (ETAudioDevice*)context; */
    /* return et_audio_fallback_to_directsound(device); */

    printf("폴백 완료!\n");
    return ET_SUCCESS;
}

/* 성능 저하 상태 출력 함수 */
static void print_degradation_state(void) {
    ETWindowsDegradationState state;
    if (et_windows_get_degradation_state(&state) == ET_SUCCESS) {
        printf("\n=== 현재 성능 저하 상태 ===\n");
        printf("오디오 품질 저하: %s\n", state.audio_quality_reduced ? "예" : "아니오");
        printf("SIMD 최적화 비활성화: %s\n", state.simd_optimization_disabled ? "예" : "아니오");
        printf("스레딩 제한: %s\n", state.threading_limited ? "예" : "아니오");
        printf("Large Page 비활성화: %s\n", state.large_pages_disabled ? "예" : "아니오");
        printf("ETW 로깅 비활성화: %s\n", state.etw_logging_disabled ? "예" : "아니오");
        printf("성능 스케일 팩터: %.2f\n", state.performance_scale_factor);
        printf("========================\n");
    }
}

/* 오류 통계 출력 함수 */
static void print_error_statistics(void) {
    ETWindowsErrorStatistics stats;
    if (et_windows_get_error_statistics(&stats) == ET_SUCCESS) {
        printf("\n=== 오류 통계 ===\n");
        printf("총 오류 발생 횟수: %u\n", stats.total_errors);
        printf("치명적 오류 횟수: %u\n", stats.critical_errors);
        printf("폴백 실행 횟수: %u\n", stats.fallback_executions);
        printf("복구 시도 횟수: %u\n", stats.recovery_attempts);
        printf("성공한 복구 횟수: %u\n", stats.successful_recoveries);
        printf("가장 빈번한 오류: 0x%X (%s)\n",
               stats.most_frequent_error,
               et_windows_get_error_message_korean(stats.most_frequent_error));
        printf("===============\n");
    }
}

/* 오류 시나리오 시뮬레이션 */
static void simulate_error_scenarios(void) {
    printf("\n=== 오류 시나리오 시뮬레이션 ===\n");

    /* 1. WASAPI 초기화 실패 시뮬레이션 */
    printf("\n1. WASAPI 초기화 실패 시뮬레이션...\n");
    ET_WINDOWS_REPORT_HRESULT_ERROR(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED, E_FAIL,
                                   "WASAPI 초기화 실패 - 오디오 장치를 찾을 수 없습니다");

    /* 2. Large Page 권한 거부 시뮬레이션 */
    printf("\n2. Large Page 권한 거부 시뮬레이션...\n");
    ET_WINDOWS_REPORT_WIN32_ERROR(ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED,
                                 "Large Page 권한이 거부되었습니다. 일반 메모리를 사용합니다");

    /* 3. AVX2 지원 없음 시뮬레이션 */
    printf("\n3. AVX2 지원 없음 시뮬레이션...\n");
    ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE, 0, S_OK,
                           "windows_simd.c", "et_windows_simd_init", 156,
                           "CPU가 AVX2를 지원하지 않습니다. 기본 구현을 사용합니다");

    /* 4. ETW 등록 실패 시뮬레이션 */
    printf("\n4. ETW 프로바이더 등록 실패 시뮬레이션...\n");
    ET_WINDOWS_REPORT_WIN32_ERROR(ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED,
                                 "ETW 프로바이더 등록에 실패했습니다. 파일 로깅을 사용합니다");

    /* 5. 보안 검사 실패 시뮬레이션 (치명적 오류) */
    printf("\n5. 보안 검사 실패 시뮬레이션...\n");
    ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED, ERROR_ACCESS_DENIED, S_OK,
                           "windows_security.c", "et_windows_check_security", 89,
                           "보안 정책 위반이 감지되었습니다");
}

/* 복구 시나리오 시뮬레이션 */
static void simulate_recovery_scenarios(void) {
    printf("\n=== 복구 시나리오 시뮬레이션 ===\n");

    /* 성능 저하 상태 적용 */
    ETWindowsDegradationState degraded_state = { 0 };
    degraded_state.audio_quality_reduced = true;
    degraded_state.simd_optimization_disabled = true;
    degraded_state.large_pages_disabled = true;
    degraded_state.performance_scale_factor = 0.6f;

    printf("성능 저하 상태 적용 중...\n");
    et_windows_apply_degradation(&degraded_state);
    print_degradation_state();

    /* 복구 시도 */
    printf("\n복구 시도 중...\n");
    ETResult recovery_result = et_windows_attempt_recovery();
    if (recovery_result == ET_SUCCESS) {
        printf("복구 성공!\n");
    } else {
        printf("복구 실패 또는 부분 복구\n");
    }

    print_degradation_state();
}

int main(void) {
    printf("LibEtude Windows 오류 처리 시스템 예제\n");
    printf("=====================================\n");

    /* 1. Windows 플랫폼 초기화 */
    printf("\n1. Windows 플랫폼 초기화 중...\n");
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.development.enable_etw_logging = true;

    ETResult result = et_windows_init(&config);
    if (result != ET_SUCCESS) {
        printf("Windows 플랫폼 초기화 실패: %d\n", result);
        return 1;
    }
    printf("Windows 플랫폼 초기화 완료!\n");

    /* 2. 오류 처리 시스템 설정 */
    printf("\n2. 오류 처리 시스템 설정 중...\n");

    /* 사용자 정의 오류 콜백 등록 */
    const char* app_name = "LibEtude 예제 애플리케이션";
    et_windows_set_error_callback(custom_error_callback, (void*)app_name);

    /* 사용자 정의 폴백 콜백 등록 */
    et_windows_register_fallback(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                                custom_wasapi_fallback, NULL);

    /* 오류 로깅 활성화 */
    et_windows_enable_error_logging("libetude_error.log");

    printf("오류 처리 시스템 설정 완료!\n");

    /* 3. 시스템 정보 로깅 */
    printf("\n3. 시스템 정보 로깅 중...\n");
    et_windows_log_system_info();
    printf("시스템 정보 로깅 완료!\n");

    /* 4. 오류 시나리오 시뮬레이션 */
    simulate_error_scenarios();

    /* 5. 현재 상태 출력 */
    print_degradation_state();
    print_error_statistics();

    /* 6. 복구 시나리오 시뮬레이션 */
    simulate_recovery_scenarios();

    /* 7. 최종 통계 출력 */
    printf("\n=== 최종 통계 ===\n");
    print_error_statistics();

    /* 8. 오류 보고서 생성 */
    printf("\n8. 오류 보고서 생성 중...\n");
    result = et_windows_generate_error_report("libetude_error_report.txt");
    if (result == ET_SUCCESS) {
        printf("오류 보고서가 'libetude_error_report.txt'에 생성되었습니다.\n");
    } else {
        printf("오류 보고서 생성 실패: %d\n", result);
    }

    /* 9. 정리 */
    printf("\n9. 정리 중...\n");
    et_windows_disable_error_logging();
    et_windows_finalize();
    printf("정리 완료!\n");

    printf("\n예제 실행 완료! 생성된 파일들을 확인해보세요:\n");
    printf("- libetude_error.log: 오류 로그 파일\n");
    printf("- libetude_error_report.txt: 오류 보고서\n");

    return 0;
}