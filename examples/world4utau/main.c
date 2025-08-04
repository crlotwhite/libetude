/**
 * @file main.c
 * @brief world4utau 메인 애플리케이션
 *
 * UTAU 호환 음성 합성 엔진의 메인 진입점입니다.
 * libetude 기반 WORLD 보코더를 사용하여 UTAU 호환 음성 합성을 수행합니다.
 */

#include "utau_interface.h"
#include "world_engine.h"
#include "audio_file_io.h"
#include "world_error.h"
#include <libetude/api.h>
#include <libetude/performance_analyzer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    printf("world4utau (libetude integration) - UTAU 호환 음성 합성 엔진\n");
    printf("Built with libetude %s\n\n", libetude_get_version());

    // 명령줄 인수 확인
    if (argc < 4) {
        print_usage(argv[0]);
        return -1;
    }

    // UTAU 파라미터 파싱
    UTAUParameters utau_params;
    ETResult result = parse_utau_parameters(argc, argv, &utau_params);
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 파라미터 파싱 실패 (코드: %d)\n", result);
        return -1;
    }

    // 파라미터 유효성 검사
    if (!validate_utau_parameters(&utau_params)) {
        fprintf(stderr, "Error: 유효하지 않은 파라미터입니다.\n");
        utau_parameters_cleanup(&utau_params);
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
        return -1;
    }

    printf("WORLD 엔진 초기화 완료\n");

    // libetude 성능 분석기 초기화
    ETProfiler* profiler = NULL;
    ETResult profiler_result = et_profiler_create(&profiler);
    if (profiler_result != ET_SUCCESS) {
        fprintf(stderr, "Warning: 성능 프로파일러 초기화 실패, 기본 시간 측정 사용\n");
    }

    // 성능 모니터링 시작
    clock_t start_time = clock();
    if (profiler) {
        et_profiler_begin_session(profiler, "world4utau_processing");
        et_profiler_begin_event(profiler, "total_processing");
    }

    if (utau_params.verbose_mode) {
        printf("입력 파일: %s\n", utau_params.input_wav_path);
        printf("출력 파일: %s\n", utau_params.output_wav_path);
        printf("목표 피치: %.2f Hz\n", utau_params.target_pitch);
        printf("벨로시티: %.2f\n", utau_params.velocity);
        printf("처리 시작...\n");
    }

    // 1. 입력 오디오 파일 읽기
    if (profiler) {
        et_profiler_begin_event(profiler, "audio_file_loading");
    }

    float* input_audio = NULL;
    int audio_length = 0;
    int sample_rate = 0;

    result = read_wav_file(utau_params.input_wav_path, &input_audio, &audio_length, &sample_rate);

    if (profiler) {
        et_profiler_end_event(profiler, "audio_file_loading");
    }
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 입력 파일 읽기 실패 (%s): %s\n",
                utau_params.input_wav_path, world_get_error_string(result));
        if (profiler) {
            et_profiler_destroy(profiler);
        }
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        utau_parameters_cleanup(&utau_params);
        return -1;
    }

    if (utau_params.verbose_mode) {
        printf("오디오 파일 로드 완료: %d 샘플, %d Hz\n", audio_length, sample_rate);
    }

    // 샘플링 레이트 검증
    if (sample_rate != utau_params.sample_rate && utau_params.sample_rate > 0) {
        printf("Warning: 파일 샘플링 레이트(%d Hz)와 지정된 샘플링 레이트(%d Hz)가 다릅니다.\n",
               sample_rate, utau_params.sample_rate);
    }

    // 2. WORLD 분석 수행
    if (profiler) {
        et_profiler_begin_event(profiler, "world_analysis");
    }

    WorldParameters world_params;
    memset(&world_params, 0, sizeof(WorldParameters));

    if (utau_params.verbose_mode) {
        printf("WORLD 분석 시작...\n");
    }

    result = world_analyze_audio(analysis_engine, input_audio, audio_length, &world_params);

    if (profiler) {
        et_profiler_end_event(profiler, "world_analysis");
    }
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: WORLD 분석 실패: %s\n", world_get_error_string(result));
        if (profiler) {
            et_profiler_destroy(profiler);
        }
        free(input_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        utau_parameters_cleanup(&utau_params);
        return -1;
    }

    if (utau_params.verbose_mode) {
        printf("WORLD 분석 완료: F0 길이 %d, FFT 크기 %d\n",
               world_params.f0_length, world_params.fft_size);
    }

    // 3. UTAU 파라미터 적용
    if (profiler) {
        et_profiler_begin_event(profiler, "utau_parameter_application");
    }

    if (utau_params.verbose_mode) {
        printf("UTAU 파라미터 적용 중...\n");
    }

    // 피치 벤드 적용
    if (utau_params.pitch_bend && utau_params.pitch_bend_length > 0) {
        result = apply_pitch_bend(&world_params, utau_params.pitch_bend,
                                 utau_params.pitch_bend_length, utau_params.target_pitch);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 피치 벤드 적용 실패: %s\n", world_get_error_string(result));
        } else if (utau_params.verbose_mode) {
            printf("피치 벤드 적용 완료\n");
        }
    } else {
        // 기본 피치 조정
        result = apply_pitch_shift(&world_params, utau_params.target_pitch);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 피치 조정 실패: %s\n", world_get_error_string(result));
        } else if (utau_params.verbose_mode) {
            printf("피치 조정 완료 (%.2f Hz)\n", utau_params.target_pitch);
        }
    }

    // 볼륨 제어 적용
    if (utau_params.volume != 1.0f) {
        result = apply_volume_control(&world_params, utau_params.volume);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 볼륨 제어 적용 실패: %s\n", world_get_error_string(result));
        } else if (utau_params.verbose_mode) {
            printf("볼륨 제어 적용 완료 (%.2f)\n", utau_params.volume);
        }
    }

    // 모듈레이션 적용
    if (utau_params.modulation > 0.0f) {
        result = apply_modulation(&world_params, utau_params.modulation);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 모듈레이션 적용 실패: %s\n", world_get_error_string(result));
        } else if (utau_params.verbose_mode) {
            printf("모듈레이션 적용 완료 (%.2f)\n", utau_params.modulation);
        }
    }

    // 타이밍 제어 적용
    if (utau_params.velocity != 1.0f) {
        result = apply_timing_control(&world_params, utau_params.velocity);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 타이밍 제어 적용 실패: %s\n", world_get_error_string(result));
        } else if (utau_params.verbose_mode) {
            printf("타이밍 제어 적용 완료 (%.2f)\n", utau_params.velocity);
        }
    }

    if (profiler) {
        et_profiler_end_event(profiler, "utau_parameter_application");
    }

    // 4. WORLD 합성 수행
    if (profiler) {
        et_profiler_begin_event(profiler, "world_synthesis");
    }

    if (utau_params.verbose_mode) {
        printf("WORLD 합성 시작...\n");
    }

    float* output_audio = NULL;
    int output_length = 0;

    result = world_synthesize_audio(synthesis_engine, &world_params, &output_audio, &output_length);

    if (profiler) {
        et_profiler_end_event(profiler, "world_synthesis");
    }
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: WORLD 합성 실패: %s\n", world_get_error_string(result));
        if (profiler) {
            et_profiler_destroy(profiler);
        }
        world_parameters_cleanup(&world_params);
        free(input_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        utau_parameters_cleanup(&utau_params);
        return -1;
    }

    if (utau_params.verbose_mode) {
        printf("WORLD 합성 완료: %d 샘플 생성\n", output_length);
    }

    // 5. 출력 오디오 파일 저장
    if (profiler) {
        et_profiler_begin_event(profiler, "audio_file_saving");
    }

    if (utau_params.verbose_mode) {
        printf("출력 파일 저장 중...\n");
    }

    result = write_wav_file(utau_params.output_wav_path, output_audio, output_length, sample_rate);

    if (profiler) {
        et_profiler_end_event(profiler, "audio_file_saving");
    }
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 출력 파일 저장 실패 (%s): %s\n",
                utau_params.output_wav_path, world_get_error_string(result));
        if (profiler) {
            et_profiler_destroy(profiler);
        }
        free(output_audio);
        world_parameters_cleanup(&world_params);
        free(input_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        utau_parameters_cleanup(&utau_params);
        return -1;
    }

    // 성능 모니터링 종료
    clock_t end_time = clock();
    double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    if (profiler) {
        et_profiler_end_event(profiler, "total_processing");
        et_profiler_end_session(profiler);
    }

    // 성공 메시지 및 성능 정보 출력
    printf("처리 완료!\n");

    if (utau_params.verbose_mode) {
        printf("\n=== 성능 분석 결과 ===\n");
        printf("전체 처리 시간: %.3f 초\n", processing_time);
        printf("실시간 비율: %.2fx\n", (double)output_length / sample_rate / processing_time);

        // 오디오 길이 정보
        double audio_duration = (double)output_length / sample_rate;
        printf("오디오 길이: %.3f 초\n", audio_duration);

        // 처리 효율성
        if (processing_time < 0.1) {
            printf("처리 효율성: 실시간 처리 가능 (100ms 이내)\n");
        } else if (processing_time < audio_duration) {
            printf("처리 효율성: 실시간보다 빠름\n");
        } else {
            printf("처리 효율성: 실시간보다 느림\n");
        }

        // 메모리 사용량 정보 (추정치)
        size_t estimated_memory = (audio_length + output_length) * sizeof(float) +
                                 world_params.f0_length * sizeof(double) +
                                 world_params.f0_length * world_params.fft_size * sizeof(double) * 2;
        printf("추정 메모리 사용량: %.2f MB\n", estimated_memory / (1024.0 * 1024.0));

        // libetude 프로파일러 결과 출력
        if (profiler) {
            printf("\n=== 상세 성능 분석 ===\n");

            // 각 단계별 시간 정보 출력
            double file_loading_time = 0.0;
            double analysis_time = 0.0;
            double parameter_time = 0.0;
            double synthesis_time = 0.0;
            double file_saving_time = 0.0;

            // 프로파일러에서 시간 정보 추출 (가능한 경우)
            if (et_profiler_get_event_time(profiler, "audio_file_loading", &file_loading_time) == ET_SUCCESS) {
                printf("파일 로딩 시간: %.3f 초 (%.1f%%)\n",
                       file_loading_time, (file_loading_time / processing_time) * 100.0);
            }

            if (et_profiler_get_event_time(profiler, "world_analysis", &analysis_time) == ET_SUCCESS) {
                printf("WORLD 분석 시간: %.3f 초 (%.1f%%)\n",
                       analysis_time, (analysis_time / processing_time) * 100.0);
            }

            if (et_profiler_get_event_time(profiler, "utau_parameter_application", &parameter_time) == ET_SUCCESS) {
                printf("파라미터 적용 시간: %.3f 초 (%.1f%%)\n",
                       parameter_time, (parameter_time / processing_time) * 100.0);
            }

            if (et_profiler_get_event_time(profiler, "world_synthesis", &synthesis_time) == ET_SUCCESS) {
                printf("WORLD 합성 시간: %.3f 초 (%.1f%%)\n",
                       synthesis_time, (synthesis_time / processing_time) * 100.0);
            }

            if (et_profiler_get_event_time(profiler, "audio_file_saving", &file_saving_time) == ET_SUCCESS) {
                printf("파일 저장 시간: %.3f 초 (%.1f%%)\n",
                       file_saving_time, (file_saving_time / processing_time) * 100.0);
            }

            // 성능 분석 보고서 저장 (선택적)
            char profile_filename[256];
            snprintf(profile_filename, sizeof(profile_filename), "world4utau_profile_%ld.json", time(NULL));
            if (et_profiler_save_report(profiler, profile_filename) == ET_SUCCESS) {
                printf("성능 분석 보고서 저장: %s\n", profile_filename);
            }
        }

        printf("====================\n");
    }

    // 메모리 정리
    free(output_audio);
    world_parameters_cleanup(&world_params);
    free(input_audio);

    // 정리
    world_synthesis_destroy(synthesis_engine);
    world_analysis_destroy(analysis_engine);
    utau_parameters_cleanup(&utau_params);

    // 프로파일러 정리
    if (profiler) {
        et_profiler_destroy(profiler);
    }

    return 0;
}