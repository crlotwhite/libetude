/**
 * @file dsp_blocks.h
 * @brief DSP 블록 다이어그램 시스템 인터페이스
 *
 * WORLD 처리 과정을 모듈화된 DSP 블록으로 설계하기 위한 기본 인터페이스를 제공합니다.
 * 각 블록은 독립적으로 동작하며, 포트 시스템을 통해 다른 블록과 연결됩니다.
 */

#ifndef WORLD4UTAU_DSP_BLOCKS_H
#define WORLD4UTAU_DSP_BLOCKS_H

#include <stdint.h>
#include <stdbool.h>
#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/error.h>

#ifdef __cplusplus
extern "C" {
#endif

// 전방 선언
typedef struct DSPBlock DSPBlock;
typedef struct DSPPort DSPPort;
typedef struct DSPConnection DSPConnection;
typedef struct DSPBlockDiagram DSPBlockDiagram;

/**
 * @brief DSP 블록 타입 열거형
 */
typedef enum {
    DSP_BLOCK_TYPE_AUDIO_INPUT,      ///< 오디오 입력 블록
    DSP_BLOCK_TYPE_F0_EXTRACTION,    ///< F0 추출 블록
    DSP_BLOCK_TYPE_SPECTRUM_ANALYSIS, ///< 스펙트럼 분석 블록
    DSP_BLOCK_TYPE_APERIODICITY_ANALYSIS, ///< 비주기성 분석 블록
    DSP_BLOCK_TYPE_PARAMETER_MERGE,   ///< 파라미터 병합 블록
    DSP_BLOCK_TYPE_SYNTHESIS,        ///< 음성 합성 블록
    DSP_BLOCK_TYPE_AUDIO_OUTPUT,     ///< 오디오 출력 블록
    DSP_BLOCK_TYPE_CUSTOM           ///< 사용자 정의 블록
} DSPBlockType;

/**
 * @brief DSP 포트 타입 열거형
 */
typedef enum {
    DSP_PORT_TYPE_AUDIO,            ///< 오디오 데이터 포트
    DSP_PORT_TYPE_F0,               ///< F0 데이터 포트
    DSP_PORT_TYPE_SPECTRUM,         ///< 스펙트럼 데이터 포트
    DSP_PORT_TYPE_APERIODICITY,     ///< 비주기성 데이터 포트
    DSP_PORT_TYPE_PARAMETERS,       ///< 파라미터 데이터 포트
    DSP_PORT_TYPE_CONTROL          ///< 제어 신호 포트
} DSPPortType;

/**
 * @brief DSP 포트 방향 열거형
 */
typedef enum {
    DSP_PORT_DIRECTION_INPUT,       ///< 입력 포트
    DSP_PORT_DIRECTION_OUTPUT       ///< 출력 포트
} DSPPortDirection;

/**
 * @brief DSP 포트 구조체
 */
struct DSPPort {
    int port_id;                    ///< 포트 ID
    char name[64];                  ///< 포트 이름
    DSPPortType type;               ///< 포트 타입
    DSPPortDirection direction;     ///< 포트 방향
    int buffer_size;                ///< 버퍼 크기
    void* buffer;                   ///< 데이터 버퍼
    bool is_connected;              ///< 연결 상태
    DSPConnection* connection;      ///< 연결 정보
};

/**
 * @brief DSP 블록 처리 함수 타입
 *
 * @param block 처리할 DSP 블록
 * @param frame_count 처리할 프레임 수
 * @return ETResult 처리 결과
 */
typedef ETResult (*DSPBlockProcessFunc)(DSPBlock* block, int frame_count);

/**
 * @brief DSP 블록 초기화 함수 타입
 *
 * @param block 초기화할 DSP 블록
 * @return ETResult 초기화 결과
 */
typedef ETResult (*DSPBlockInitFunc)(DSPBlock* block);

/**
 * @brief DSP 블록 정리 함수 타입
 *
 * @param block 정리할 DSP 블록
 */
typedef void (*DSPBlockCleanupFunc)(DSPBlock* block);

/**
 * @brief DSP 블록 구조체
 */
struct DSPBlock {
    int block_id;                   ///< 블록 ID
    char name[64];                  ///< 블록 이름
    DSPBlockType type;              ///< 블록 타입

    // 포트 정보
    DSPPort* input_ports;           ///< 입력 포트 배열
    int input_port_count;           ///< 입력 포트 수
    DSPPort* output_ports;          ///< 출력 포트 배열
    int output_port_count;          ///< 출력 포트 수

    // 블록별 데이터
    void* block_data;               ///< 블록별 사용자 데이터
    size_t block_data_size;         ///< 블록 데이터 크기

    // 처리 함수들
    DSPBlockProcessFunc process;    ///< 처리 함수
    DSPBlockInitFunc initialize;    ///< 초기화 함수
    DSPBlockCleanupFunc cleanup;    ///< 정리 함수

    // 상태 정보
    bool is_initialized;            ///< 초기화 상태
    bool is_enabled;                ///< 활성화 상태

    // 메모리 관리
    ETMemoryPool* mem_pool;         ///< 메모리 풀
};

/**
 * @brief DSP 연결 구조체
 */
struct DSPConnection {
    int connection_id;              ///< 연결 ID

    // 소스 정보
    int source_block_id;            ///< 소스 블록 ID
    int source_port_id;             ///< 소스 포트 ID
    DSPBlock* source_block;         ///< 소스 블록 포인터
    DSPPort* source_port;           ///< 소스 포트 포인터

    // 대상 정보
    int dest_block_id;              ///< 대상 블록 ID
    int dest_port_id;               ///< 대상 포트 ID
    DSPBlock* dest_block;           ///< 대상 블록 포인터
    DSPPort* dest_port;             ///< 대상 포트 포인터

    // 연결 설정
    int buffer_size;                ///< 연결 버퍼 크기
    bool is_active;                 ///< 연결 활성화 상태
};

/**
 * @brief DSP 블록 다이어그램 구조체
 */
struct DSPBlockDiagram {
    char name[128];                 ///< 다이어그램 이름

    // 블록 관리
    DSPBlock* blocks;               ///< 블록 배열
    int block_count;                ///< 블록 수
    int max_blocks;                 ///< 최대 블록 수

    // 연결 관리
    DSPConnection* connections;     ///< 연결 배열
    int connection_count;           ///< 연결 수
    int max_connections;            ///< 최대 연결 수

    // ID 관리
    int next_block_id;              ///< 다음 블록 ID
    int next_connection_id;         ///< 다음 연결 ID

    // 메모리 관리
    ETMemoryPool* mem_pool;         ///< 메모리 풀

    // 상태 정보
    bool is_built;                  ///< 빌드 완료 상태
    bool is_validated;              ///< 검증 완료 상태
};

// =============================================================================
// DSP 블록 기본 함수들
// =============================================================================

/**
 * @brief DSP 블록 생성
 *
 * @param name 블록 이름
 * @param type 블록 타입
 * @param input_port_count 입력 포트 수
 * @param output_port_count 출력 포트 수
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_create(const char* name, DSPBlockType type,
                          int input_port_count, int output_port_count,
                          ETMemoryPool* mem_pool);

/**
 * @brief DSP 블록 해제
 *
 * @param block 해제할 블록
 */
void dsp_block_destroy(DSPBlock* block);

/**
 * @brief DSP 블록 초기화
 *
 * @param block 초기화할 블록
 * @return ETResult 초기화 결과
 */
ETResult dsp_block_initialize(DSPBlock* block);

/**
 * @brief DSP 블록 처리 실행
 *
 * @param block 처리할 블록
 * @param frame_count 처리할 프레임 수
 * @return ETResult 처리 결과
 */
ETResult dsp_block_process(DSPBlock* block, int frame_count);

/**
 * @brief DSP 블록 정리
 *
 * @param block 정리할 블록
 */
void dsp_block_cleanup(DSPBlock* block);

// =============================================================================
// DSP 포트 관리 함수들
// =============================================================================

/**
 * @brief DSP 포트 설정
 *
 * @param block 블록
 * @param port_index 포트 인덱스
 * @param direction 포트 방향
 * @param name 포트 이름
 * @param type 포트 타입
 * @param buffer_size 버퍼 크기
 * @return ETResult 설정 결과
 */
ETResult dsp_block_set_port(DSPBlock* block, int port_index,
                           DSPPortDirection direction, const char* name,
                           DSPPortType type, int buffer_size);

/**
 * @brief 입력 포트 가져오기
 *
 * @param block 블록
 * @param port_index 포트 인덱스
 * @return DSPPort* 포트 포인터 (실패 시 NULL)
 */
DSPPort* dsp_block_get_input_port(DSPBlock* block, int port_index);

/**
 * @brief 출력 포트 가져오기
 *
 * @param block 블록
 * @param port_index 포트 인덱스
 * @return DSPPort* 포트 포인터 (실패 시 NULL)
 */
DSPPort* dsp_block_get_output_port(DSPBlock* block, int port_index);

/**
 * @brief 포트 버퍼 할당
 *
 * @param port 포트
 * @param mem_pool 메모리 풀
 * @return ETResult 할당 결과
 */
ETResult dsp_port_allocate_buffer(DSPPort* port, ETMemoryPool* mem_pool);

/**
 * @brief 포트 버퍼 해제
 *
 * @param port 포트
 */
void dsp_port_free_buffer(DSPPort* port);

// =============================================================================
// DSP 연결 관리 함수들
// =============================================================================

/**
 * @brief DSP 연결 생성
 *
 * @param source_block 소스 블록
 * @param source_port_index 소스 포트 인덱스
 * @param dest_block 대상 블록
 * @param dest_port_index 대상 포트 인덱스
 * @param mem_pool 메모리 풀
 * @return DSPConnection* 생성된 연결 (실패 시 NULL)
 */
DSPConnection* dsp_connection_create(DSPBlock* source_block, int source_port_index,
                                    DSPBlock* dest_block, int dest_port_index,
                                    ETMemoryPool* mem_pool);

/**
 * @brief DSP 연결 해제
 *
 * @param connection 해제할 연결
 */
void dsp_connection_destroy(DSPConnection* connection);

/**
 * @brief DSP 연결 검증
 *
 * @param connection 검증할 연결
 * @return bool 검증 결과 (true: 유효, false: 무효)
 */
bool dsp_connection_validate(DSPConnection* connection);

/**
 * @brief DSP 연결 활성화
 *
 * @param connection 활성화할 연결
 * @return ETResult 활성화 결과
 */
ETResult dsp_connection_activate(DSPConnection* connection);

/**
 * @brief DSP 연결 비활성화
 *
 * @param connection 비활성화할 연결
 */
void dsp_connection_deactivate(DSPConnection* connection);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_DSP_BLOCKS_H