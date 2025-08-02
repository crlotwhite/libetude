/**
 * @file audio_file_io.h
 * @brief WAV 파일 I/O 헤더 파일
 *
 * world4utau를 위한 WAV 파일 읽기/쓰기 기능을 제공합니다.
 * libetude 오디오 I/O 시스템과 통합되어 다양한 WAV 포맷을 지원합니다.
 */

#ifndef WORLD4UTAU_AUDIO_FILE_IO_H
#define WORLD4UTAU_AUDIO_FILE_IO_H

#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/error.h>
#include <libetude/audio_io.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV 파일 헤더 구조체
 */
typedef struct {
    // RIFF 헤더
    char riff_id[4];             /**< "RIFF" */
    uint32_t file_size;          /**< 파일 크기 - 8 */
    char wave_id[4];             /**< "WAVE" */

    // fmt 청크
    char fmt_id[4];              /**< "fmt " */
    uint32_t fmt_size;           /**< fmt 청크 크기 */
    uint16_t format_tag;         /**< 오디오 포맷 (1=PCM, 3=IEEE float) */
    uint16_t num_channels;       /**< 채널 수 */
    uint32_t sample_rate;        /**< 샘플링 레이트 */
    uint32_t bytes_per_sec;      /**< 초당 바이트 수 */
    uint16_t block_align;        /**< 블록 정렬 */
    uint16_t bits_per_sample;    /**< 샘플당 비트 수 */

    // data 청크
    char data_id[4];             /**< "data" */
    uint32_t data_size;          /**< 데이터 크기 */
} WAVHeader;

/**
 * @brief 오디오 파일 정보 구조체
 */
typedef struct {
    uint32_t sample_rate;        /**< 샘플링 레이트 (Hz) */
    uint16_t num_channels;       /**< 채널 수 */
    uint16_t bits_per_sample;    /**< 샘플당 비트 수 */
    uint32_t num_samples;        /**< 총 샘플 수 (per channel) */
    double duration_seconds;     /**< 재생 시간 (초) */
    bool is_float_format;        /**< IEEE float 포맷 여부 */
} AudioFileInfo;

/**
 * @brief 오디오 데이터 구조체
 */
typedef struct {
    float* data;                 /**< 오디오 데이터 (interleaved, normalized to [-1.0, 1.0]) */
    AudioFileInfo info;          /**< 파일 정보 */
    bool owns_data;              /**< 데이터 소유권 플래그 */
} AudioData;

/**
 * @brief WAV 파일 읽기
 *
 * WAV 파일을 읽어서 float 배열로 변환합니다.
 * 16bit, 24bit, 32bit PCM 및 32bit IEEE float 포맷을 지원합니다.
 *
 * @param file_path WAV 파일 경로
 * @param audio_data 읽은 오디오 데이터를 저장할 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult read_wav_file(const char* file_path, AudioData* audio_data);

/**
 * @brief WAV 파일 쓰기
 *
 * float 배열을 WAV 파일로 저장합니다.
 * 출력 포맷은 AudioFileInfo의 설정에 따라 결정됩니다.
 *
 * @param file_path 출력 WAV 파일 경로
 * @param audio_data 저장할 오디오 데이터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult write_wav_file(const char* file_path, const AudioData* audio_data);

/**
 * @brief WAV 파일 정보 조회
 *
 * WAV 파일의 헤더만 읽어서 파일 정보를 조회합니다.
 * 실제 오디오 데이터는 읽지 않습니다.
 *
 * @param file_path WAV 파일 경로
 * @param info 파일 정보를 저장할 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult get_wav_file_info(const char* file_path, AudioFileInfo* info);

/**
 * @brief 오디오 데이터 생성
 *
 * 지정된 크기의 오디오 데이터 구조체를 생성합니다.
 *
 * @param num_samples 샘플 수 (per channel)
 * @param num_channels 채널 수
 * @param sample_rate 샘플링 레이트
 * @return 생성된 오디오 데이터 구조체 (실패시 NULL)
 */
AudioData* audio_data_create(uint32_t num_samples, uint16_t num_channels, uint32_t sample_rate);

/**
 * @brief 오디오 데이터 해제
 *
 * 오디오 데이터 구조체와 관련 메모리를 해제합니다.
 *
 * @param audio_data 해제할 오디오 데이터
 */
void audio_data_destroy(AudioData* audio_data);

/**
 * @brief 오디오 데이터 복사
 *
 * 오디오 데이터를 다른 구조체로 복사합니다.
 *
 * @param src 소스 오디오 데이터
 * @param dst 대상 오디오 데이터 (미리 할당되어야 함)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult audio_data_copy(const AudioData* src, AudioData* dst);

/**
 * @brief 오디오 데이터 정규화
 *
 * 오디오 데이터를 [-1.0, 1.0] 범위로 정규화합니다.
 *
 * @param audio_data 정규화할 오디오 데이터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult audio_data_normalize(AudioData* audio_data);

/**
 * @brief 모노 변환
 *
 * 스테레오 오디오를 모노로 변환합니다.
 * 좌우 채널의 평균을 계산합니다.
 *
 * @param audio_data 변환할 오디오 데이터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult audio_data_to_mono(AudioData* audio_data);

/**
 * @brief 샘플링 레이트 변환 (간단한 선형 보간)
 *
 * 오디오 데이터의 샘플링 레이트를 변경합니다.
 * 간단한 선형 보간을 사용합니다.
 *
 * @param audio_data 변환할 오디오 데이터
 * @param target_sample_rate 목표 샘플링 레이트
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult audio_data_resample(AudioData* audio_data, uint32_t target_sample_rate);

/**
 * @brief WAV 헤더 유효성 검사
 *
 * WAV 파일 헤더의 유효성을 검사합니다.
 *
 * @param header 검사할 WAV 헤더
 * @return true 유효함, false 유효하지 않음
 */
bool validate_wav_header(const WAVHeader* header);

/**
 * @brief 지원되는 포맷 확인
 *
 * 지정된 오디오 포맷이 지원되는지 확인합니다.
 *
 * @param format_tag 포맷 태그 (1=PCM, 3=IEEE float)
 * @param bits_per_sample 샘플당 비트 수
 * @return true 지원됨, false 지원되지 않음
 */
bool is_supported_format(uint16_t format_tag, uint16_t bits_per_sample);

/**
 * @brief 파일 크기 계산
 *
 * 오디오 데이터로부터 WAV 파일 크기를 계산합니다.
 *
 * @param audio_data 오디오 데이터
 * @return 예상 파일 크기 (바이트)
 */
uint32_t calculate_wav_file_size(const AudioData* audio_data);

/**
 * @brief libetude AudioFormat 변환
 *
 * AudioFileInfo를 libetude ETAudioFormat으로 변환합니다.
 *
 * @param info 오디오 파일 정보
 * @param buffer_size 버퍼 크기
 * @return libetude 오디오 포맷
 */
ETAudioFormat audio_info_to_et_format(const AudioFileInfo* info, uint32_t buffer_size);

/**
 * @brief 오디오 데이터 디버그 출력
 *
 * 오디오 데이터 정보를 디버그 목적으로 출력합니다.
 *
 * @param audio_data 출력할 오디오 데이터
 * @param label 라벨 (선택적)
 */
void debug_print_audio_data(const AudioData* audio_data, const char* label);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_AUDIO_FILE_IO_H/**

* @brief 고품질 WAV 파일 쓰기 (추가 옵션 지원)
 *
 * 더 세밀한 제어가 가능한 WAV 파일 쓰기 함수입니다.
 *
 * @param file_path 출력 WAV 파일 경로
 * @param audio_data 저장할 오디오 데이터
 * @param target_bits_per_sample 목표 비트 깊이 (0이면 자동 선택)
 * @param use_float_format IEEE float 포맷 사용 여부
 * @param apply_dithering 디더링 적용 여부 (비트 깊이 감소 시)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult write_wav_file_advanced(const char* file_path, const AudioData* audio_data,
                                uint16_t target_bits_per_sample, bool use_float_format,
                                bool apply_dithering);

/**
 * @brief WAV 파일 배치 변환
 *
 * 여러 WAV 파일을 일괄 변환합니다.
 *
 * @param input_files 입력 파일 경로 배열
 * @param output_files 출력 파일 경로 배열
 * @param num_files 파일 개수
 * @param target_sample_rate 목표 샘플링 레이트 (0이면 변경 안함)
 * @param target_bits_per_sample 목표 비트 깊이 (0이면 변경 안함)
 * @param convert_to_mono 모노 변환 여부
 * @return 성공한 파일 개수
 */
int batch_convert_wav_files(const char** input_files, const char** output_files,
                           int num_files, uint32_t target_sample_rate,
                           uint16_t target_bits_per_sample, bool convert_to_mono);

/**
 * @brief WAV 파일 메타데이터 출력
 *
 * WAV 파일의 상세 정보를 출력합니다.
 *
 * @param file_path WAV 파일 경로
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult print_wav_file_metadata(const char* file_path);

/**
 * @brief 오디오 데이터 통계 계산
 *
 * 오디오 데이터의 통계 정보를 계산합니다.
 *
 * @param audio_data 분석할 오디오 데이터
 * @param min_value 최솟값을 저장할 포인터
 * @param max_value 최댓값을 저장할 포인터
 * @param mean_value 평균값을 저장할 포인터
 * @param rms_value RMS 값을 저장할 포인터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult calculate_audio_statistics(const AudioData* audio_data,
                                   float* min_value, float* max_value,
                                   float* mean_value, float* rms_value);

/**
 * @brief 오디오 데이터 무음 감지
 *
 * 오디오 데이터에서 무음 구간을 감지합니다.
 *
 * @param audio_data 분석할 오디오 데이터
 * @param threshold 무음 임계값 (절댓값)
 * @param min_duration_ms 최소 무음 지속 시간 (밀리초)
 * @param silence_start 무음 시작 시간을 저장할 배열
 * @param silence_end 무음 종료 시간을 저장할 배열
 * @param max_regions 최대 무음 구간 수
 * @return 감지된 무음 구간 수
 */
int detect_silence_regions(const AudioData* audio_data, float threshold,
                          float min_duration_ms, float* silence_start,
                          float* silence_end, int max_regions);

/**
 * @brief 오디오 데이터 트림 (무음 제거)
 *
 * 오디오 데이터의 앞뒤 무음을 제거합니다.
 *
 * @param audio_data 트림할 오디오 데이터
 * @param threshold 무음 임계값
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult trim_audio_silence(AudioData* audio_data, float threshold);/**
 *
 @brief libetude 오디오 I/O 통합 구조체
 */
typedef struct {
    ETAudioDevice* input_device;     /**< 입력 디바이스 */
    ETAudioDevice* output_device;    /**< 출력 디바이스 */
    ETAudioBuffer* input_buffer;     /**< 입력 버퍼 */
    ETAudioBuffer* output_buffer;    /**< 출력 버퍼 */
    ETAudioFormat format;            /**< 오디오 포맷 */

    // 콜백 관련
    void (*audio_callback)(float* input, float* output, int num_frames, void* user_data);
    void* user_data;                 /**< 사용자 데이터 */

    // 상태 관리
    bool is_initialized;             /**< 초기화 상태 */
    bool is_running;                 /**< 실행 상태 */

    // 통계
    uint64_t frames_processed;       /**< 처리된 프레임 수 */
    double cpu_usage;                /**< CPU 사용률 */
} WorldAudioIO;

/**
 * @brief libetude 오디오 I/O 시스템 초기화
 *
 * world4utau를 위한 libetude 오디오 I/O 시스템을 초기화합니다.
 *
 * @param audio_io 초기화할 오디오 I/O 구조체
 * @param sample_rate 샘플링 레이트
 * @param num_channels 채널 수
 * @param buffer_size 버퍼 크기 (프레임 수)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_audio_io_init(WorldAudioIO* audio_io, uint32_t sample_rate,
                            uint16_t num_channels, uint32_t buffer_size);

/**
 * @brief libetude 오디오 I/O 시스템 해제
 *
 * 오디오 I/O 시스템을 정리하고 리소스를 해제합니다.
 *
 * @param audio_io 해제할 오디오 I/O 구조체
 */
void world_audio_io_cleanup(WorldAudioIO* audio_io);

/**
 * @brief 오디오 콜백 설정
 *
 * 실시간 오디오 처리를 위한 콜백 함수를 설정합니다.
 *
 * @param audio_io 오디오 I/O 구조체
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_audio_io_set_callback(WorldAudioIO* audio_io,
                                    void (*callback)(float* input, float* output,
                                                   int num_frames, void* user_data),
                                    void* user_data);

/**
 * @brief 오디오 스트림 시작
 *
 * 실시간 오디오 스트림을 시작합니다.
 *
 * @param audio_io 오디오 I/O 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_audio_io_start(WorldAudioIO* audio_io);

/**
 * @brief 오디오 스트림 정지
 *
 * 실시간 오디오 스트림을 정지합니다.
 *
 * @param audio_io 오디오 I/O 구조체
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_audio_io_stop(WorldAudioIO* audio_io);

/**
 * @brief AudioData를 libetude 버퍼로 변환
 *
 * AudioData 구조체를 libetude ETAudioBuffer로 변환합니다.
 *
 * @param audio_data 변환할 오디오 데이터
 * @param buffer 대상 libetude 버퍼
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult audio_data_to_et_buffer(const AudioData* audio_data, ETAudioBuffer* buffer);

/**
 * @brief libetude 버퍼를 AudioData로 변환
 *
 * libetude ETAudioBuffer를 AudioData 구조체로 변환합니다.
 *
 * @param buffer 소스 libetude 버퍼
 * @param audio_data 대상 오디오 데이터
 * @param sample_rate 샘플링 레이트
 * @param num_channels 채널 수
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult et_buffer_to_audio_data(const ETAudioBuffer* buffer, AudioData* audio_data,
                                uint32_t sample_rate, uint16_t num_channels);

/**
 * @brief 크로스 플랫폼 오디오 디바이스 열거
 *
 * 사용 가능한 오디오 디바이스 목록을 조회합니다.
 *
 * @param device_names 디바이스 이름을 저장할 배열
 * @param max_devices 최대 디바이스 수
 * @param is_input 입력 디바이스 여부 (false면 출력 디바이스)
 * @return 발견된 디바이스 수
 */
int enumerate_audio_devices(char device_names[][256], int max_devices, bool is_input);

/**
 * @brief 오디오 디바이스 정보 조회
 *
 * 지정된 오디오 디바이스의 상세 정보를 조회합니다.
 *
 * @param device_name 디바이스 이름 (NULL이면 기본 디바이스)
 * @param is_input 입력 디바이스 여부
 * @param supported_rates 지원되는 샘플링 레이트 배열
 * @param max_rates 최대 샘플링 레이트 수
 * @param supported_channels 지원되는 채널 수 배열
 * @param max_channels 최대 채널 수
 * @return 조회 성공 여부
 */
bool get_audio_device_info(const char* device_name, bool is_input,
                          uint32_t* supported_rates, int max_rates,
                          uint16_t* supported_channels, int max_channels);

/**
 * @brief 실시간 오디오 처리 성능 모니터링
 *
 * 실시간 오디오 처리의 성능을 모니터링합니다.
 *
 * @param audio_io 오디오 I/O 구조체
 * @param cpu_usage CPU 사용률을 저장할 포인터
 * @param buffer_underruns 버퍼 언더런 횟수를 저장할 포인터
 * @param latency_ms 지연시간을 저장할 포인터 (밀리초)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult monitor_audio_performance(const WorldAudioIO* audio_io, double* cpu_usage,
                                  uint32_t* buffer_underruns, uint32_t* latency_ms);

/**
 * @brief 오디오 스트림 품질 테스트
 *
 * 오디오 스트림의 품질을 테스트합니다.
 *
 * @param audio_io 오디오 I/O 구조체
 * @param test_duration_seconds 테스트 지속 시간 (초)
 * @param test_frequency 테스트 주파수 (Hz)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult test_audio_stream_quality(WorldAudioIO* audio_io, float test_duration_seconds,
                                  float test_frequency);