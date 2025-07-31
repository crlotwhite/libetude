/**
 * @file utau_interface.h
 * @brief UTAU 호환 인터페이스 헤더 파일
 *
 * UTAU 명령줄 파라미터 파싱 및 호환성 인터페이스를 제공합니다.
 * libetude 기반 world4utau 구현을 위한 UTAU 표준 인터페이스입니다.
 */

#ifndef WORLD4UTAU_UTAU_INTERFACE_H
#define WORLD4UTAU_UTAU_INTERFACE_H

#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/error.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UTAU 파라미터 구조체
 *
 * UTAU 명령줄에서 전달되는 모든 파라미터를 포함합니다.
 * 기존 world4utau와 완전히 호환되는 파라미터 구조입니다.
 */
typedef struct {
    // 파일 경로
    char* input_wav_path;        /**< 입력 WAV 파일 경로 */
    char* output_wav_path;       /**< 출력 WAV 파일 경로 */

    // 기본 음성 파라미터
    float target_pitch;          /**< 목표 피치 (Hz) */
    float velocity;              /**< 벨로시티 (0.0-1.0) */
    float volume;                /**< 볼륨 (0.0-1.0) */
    float modulation;            /**< 모듈레이션 강도 (0.0-1.0) */

    // 피치 벤드 데이터
    float* pitch_bend;           /**< 피치 벤드 데이터 배열 */
    int pitch_bend_length;       /**< 피치 벤드 데이터 길이 */

    // UTAU 특화 파라미터
    float consonant_velocity;    /**< 자음 벨로시티 (0.0-1.0) */
    float pre_utterance;         /**< 선행발성 (ms) */
    float overlap;               /**< 오버랩 (ms) */
    float start_point;           /**< 시작점 (ms) */

    // 오디오 설정
    int sample_rate;             /**< 샘플링 레이트 (Hz) */
    int bit_depth;               /**< 비트 깊이 (16, 24, 32) */

    // 플래그 및 옵션
    bool enable_cache;           /**< 캐시 사용 여부 */
    bool enable_optimization;    /**< 최적화 사용 여부 */
    bool verbose_mode;           /**< 상세 출력 모드 */

    // 내부 관리용
    bool owns_memory;            /**< 메모리 소유권 플래그 */
} UTAUParameters;

/**
 * @brief UTAU 파라미터 초기화
 *
 * UTAUParameters 구조체를 기본값으로 초기화합니다.
 *
 * @param params 초기화할 파라미터 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult utau_parameters_init(UTAUParameters* params);

/**
 * @brief UTAU 명령줄 파라미터 파싱
 *
 * argc, argv로부터 UTAU 호환 파라미터를 파싱합니다.
 * 기존 world4utau와 동일한 명령줄 형식을 지원합니다.
 *
 * @param argc 명령줄 인수 개수
 * @param argv 명령줄 인수 배열
 * @param params 파싱된 파라미터를 저장할 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult parse_utau_parameters(int argc, char* argv[], UTAUParameters* params);

/**
 * @brief UTAU 파라미터 유효성 검사
 *
 * 파싱된 UTAU 파라미터의 유효성을 검사합니다.
 * 파일 경로, 수치 범위, 필수 파라미터 등을 확인합니다.
 *
 * @param params 검사할 파라미터 구조체
 * @return true 유효함, false 유효하지 않음
 */
bool validate_utau_parameters(const UTAUParameters* params);

/**
 * @brief UTAU 파라미터 메모리 해제
 *
 * UTAUParameters 구조체에서 할당된 메모리를 해제합니다.
 *
 * @param params 해제할 파라미터 구조체
 */
void utau_parameters_cleanup(UTAUParameters* params);

/**
 * @brief 사용법 출력
 *
 * world4utau의 명령줄 사용법을 출력합니다.
 * 기존 world4utau와 동일한 형식으로 출력됩니다.
 *
 * @param program_name 프로그램 이름
 */
void print_usage(const char* program_name);

/**
 * @brief 버전 정보 출력
 *
 * world4utau 버전 및 libetude 버전 정보를 출력합니다.
 */
void print_version_info(void);

/**
 * @brief 피치 벤드 파일 로드
 *
 * 외부 파일에서 피치 벤드 데이터를 로드합니다.
 *
 * @param file_path 피치 벤드 파일 경로
 * @param pitch_bend 로드된 피치 벤드 데이터를 저장할 포인터
 * @param length 로드된 데이터 길이를 저장할 포인터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult load_pitch_bend_file(const char* file_path, float** pitch_bend, int* length);

/**
 * @brief 파라미터 디버그 출력
 *
 * 파싱된 UTAU 파라미터를 디버그 목적으로 출력합니다.
 *
 * @param params 출력할 파라미터 구조체
 */
void debug_print_parameters(const UTAUParameters* params);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_UTAU_INTERFACE_H