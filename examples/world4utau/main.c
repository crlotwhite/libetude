/**
 * @file main.c
 * @brief world4utau 메인 애플리케이션
 *
 * UTAU 호환 음성 합성 엔진의 메인 진입점입니다.
 * libetude 기반 WORLD 보코더를 사용하여 UTAU 호환 음성 합성을 수행합니다.
 */

#include "utau_interface.h"
#include "world_engine.h"
#include <libetude/api.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 메인 함수
 *
 * UTAU 호환 명령줄 인터페이스를 제공합니다.
 *
 * @param argc 명령줄 인수 개수
 * @param argv 명령줄 인수 배열
 * @return 0 성공, 그 외 오류 코드
 */
int main(int argc, char* argv[]) {
    // libetude 초기화
    ETResult result = et_initialize();
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: libetude 초기화 실패: %s\n", et_get_error_string(result));
        return -1;
    }

    printf("world4utau (libetude integration) - UTAU 호환 음성 합성 엔진\n");
    printf("Built with libetude %s\n\n", et_get_version_string());

    // 명령줄 인수 확인
    if (argc < 4) {
        print_usage(argv[0]);
        et_finalize();
        return -1;
    }

    // UTAU 파라미터 파싱
    UTAUParameters utau_params;
    result = parse_utau_parameters(argc, argv, &utau_params);
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 파라미터 파싱 실패: %s\n", et_get_error_string(result));
        et_finalize();
        return -1;
    }

    // 파라미터 유효성 검사
    if (!validate_utau_parameters(&utau_params)) {
        fprintf(stderr, "Error: 유효하지 않은 파라미터입니다.\n");
        utau_parameters_cleanup(&utau_params);
        et_finalize();
        return -1;
    }

    // 상세 모드에서 파라미터 출력
    if (utau_params.verbose_mode) {
        debug_print_parameters(&utau_params);
    }

    // WORLD 분석 엔진 설정
    WorldAnalysisConfig analysis_config;
    world_get_default_analysis_config(&analysis_config);

    // 사용자 설정 적용
    if (!utau_params.enable_optimization) {
        analysis_config.enable_simd_optimization = false;
        analysis_config.enable_gpu_acceleration = false;
    }

    // WORLD 분석 엔진 생성
    WorldAnalysisEngine* analysis_engine = world_analysis_create(&analysis_config);
    if (!analysis_engine) {
        fprintf(stderr, "Error: WORLD 분석 엔진 생성 실패\n");
        utau_parameters_cleanup(&utau_params);
        et_finalize();
        return -1;
    }

    // WORLD 합성 엔진 설정
    WorldSynthesisConfig synthesis_config;
    world_get_default_synthesis_config(&synthesis_config);
    synthesis_config.sample_rate = utau_params.sample_rate;

    if (!utau_params.enable_optimization) {
        synthesis_config.enable_simd_optimization = false;
        synthesis_config.enable_gpu_acceleration = false;
    }

    // WORLD 합성 엔진 생성
    WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
    if (!synthesis_engine) {
        fprintf(stderr, "Error: WORLD 합성 엔진 생성 실패\n");
        world_analysis_destroy(analysis_engine);
        utau_parameters_cleanup(&utau_params);
        et_finalize();
        return -1;
    }

    printf("WORLD 엔진 초기화 완료\n");

    // TODO: 실제 음성 처리 로직 구현
    // 현재는 기본 구조만 구현됨

    printf("입력 파일: %s\n", utau_params.input_wav_path);
    printf("출력 파일: %s\n", utau_params.output_wav_path);
    printf("목표 피치: %.2f Hz\n", utau_params.target_pitch);
    printf("벨로시티: %.2f\n", utau_params.velocity);

    if (utau_params.verbose_mode) {
        printf("처리 중...\n");
    }

    // 성공 메시지
    printf("처리 완료!\n");

    // 정리
    world_synthesis_destroy(synthesis_engine);
    world_analysis_destroy(analysis_engine);
    utau_parameters_cleanup(&utau_params);

    // libetude 종료
    et_finalize();

    return 0;
}