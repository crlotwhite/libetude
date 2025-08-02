/**
 * @file dsp_block_diagram.h
 * @brief DSP 블록 다이어그램 관리 인터페이스
 *
 * DSP 블록들을 연결하여 다이어그램을 구성하고 관리하는 기능을 제공합니다.
 */

#ifndef WORLD4UTAU_DSP_BLOCK_DIAGRAM_H
#define WORLD4UTAU_DSP_BLOCK_DIAGRAM_H

#include "dsp_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// DSP 블록 다이어그램 관리 함수들
// =============================================================================

/**
 * @brief DSP 블록 다이어그램 생성
 *
 * @param name 다이어그램 이름
 * @param max_blocks 최대 블록 수
 * @param max_connections 최대 연결 수
 * @param mem_pool 메모리 풀
 * @return DSPBlockDiagram* 생성된 다이어그램 (실패 시 NULL)
 */
DSPBlockDiagram* dsp_block_diagram_create(const char* name, int max_blocks,
                                         int max_connections, ETMemoryPool* mem_pool);

/**
 * @brief DSP 블록 다이어그램 해제
 *
 * @param diagram 해제할 다이어그램
 */
void dsp_block_diagram_destroy(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램에 블록 추가
 *
 * @param diagram 다이어그램
 * @param block 추가할 블록
 * @return ETResult 추가 결과
 */
ETResult dsp_block_diagram_add_block(DSPBlockDiagram* diagram, DSPBlock* block);

/**
 * @brief 다이어그램에서 블록 제거
 *
 * @param diagram 다이어그램
 * @param block_id 제거할 블록 ID
 * @return ETResult 제거 결과
 */
ETResult dsp_block_diagram_remove_block(DSPBlockDiagram* diagram, int block_id);

/**
 * @brief 블록 ID로 블록 찾기
 *
 * @param diagram 다이어그램
 * @param block_id 블록 ID
 * @return DSPBlock* 찾은 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_diagram_find_block(DSPBlockDiagram* diagram, int block_id);

/**
 * @brief 블록 이름으로 블록 찾기
 *
 * @param diagram 다이어그램
 * @param name 블록 이름
 * @return DSPBlock* 찾은 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_diagram_find_block_by_name(DSPBlockDiagram* diagram, const char* name);

/**
 * @brief 블록 간 연결 생성
 *
 * @param diagram 다이어그램
 * @param source_block_id 소스 블록 ID
 * @param source_port_index 소스 포트 인덱스
 * @param dest_block_id 대상 블록 ID
 * @param dest_port_index 대상 포트 인덱스
 * @return ETResult 연결 결과
 */
ETResult dsp_block_diagram_connect(DSPBlockDiagram* diagram,
                                  int source_block_id, int source_port_index,
                                  int dest_block_id, int dest_port_index);

/**
 * @brief 블록 간 연결 해제
 *
 * @param diagram 다이어그램
 * @param connection_id 연결 ID
 * @return ETResult 해제 결과
 */
ETResult dsp_block_diagram_disconnect(DSPBlockDiagram* diagram, int connection_id);

/**
 * @brief 다이어그램 검증
 *
 * @param diagram 검증할 다이어그램
 * @return bool 검증 결과 (true: 유효, false: 무효)
 */
bool dsp_block_diagram_validate(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램 빌드
 *
 * @param diagram 빌드할 다이어그램
 * @return ETResult 빌드 결과
 */
ETResult dsp_block_diagram_build(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램 초기화
 *
 * @param diagram 초기화할 다이어그램
 * @return ETResult 초기화 결과
 */
ETResult dsp_block_diagram_initialize(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램 실행 (모든 블록 처리)
 *
 * @param diagram 실행할 다이어그램
 * @param frame_count 처리할 프레임 수
 * @return ETResult 실행 결과
 */
ETResult dsp_block_diagram_process(DSPBlockDiagram* diagram, int frame_count);

/**
 * @brief 다이어그램 정리
 *
 * @param diagram 정리할 다이어그램
 */
void dsp_block_diagram_cleanup(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램 상태 출력 (디버깅용)
 *
 * @param diagram 다이어그램
 */
void dsp_block_diagram_print_status(DSPBlockDiagram* diagram);

/**
 * @brief 다이어그램을 DOT 형식으로 출력 (시각화용)
 *
 * @param diagram 다이어그램
 * @param filename 출력 파일명
 * @return ETResult 출력 결과
 */
ETResult dsp_block_diagram_export_dot(DSPBlockDiagram* diagram, const char* filename);

// =============================================================================
// 데이터 흐름 관리 함수들
// =============================================================================

/**
 * @brief 연결된 블록 간 데이터 전송
 *
 * @param connection 연결
 * @return ETResult 전송 결과
 */
ETResult dsp_connection_transfer_data(DSPConnection* connection);

/**
 * @brief 다이어그램의 모든 연결에서 데이터 전송
 *
 * @param diagram 다이어그램
 * @return ETResult 전송 결과
 */
ETResult dsp_block_diagram_transfer_all_data(DSPBlockDiagram* diagram);

/**
 * @brief 블록 실행 순서 계산 (토폴로지 정렬)
 *
 * @param diagram 다이어그램
 * @param execution_order 실행 순서 배열 (출력)
 * @param max_order_size 배열 최대 크기
 * @return int 실행 순서 배열 크기 (실패 시 -1)
 */
int dsp_block_diagram_calculate_execution_order(DSPBlockDiagram* diagram,
                                               int* execution_order,
                                               int max_order_size);

/**
 * @brief 순서에 따른 블록 실행
 *
 * @param diagram 다이어그램
 * @param execution_order 실행 순서 배열
 * @param order_size 배열 크기
 * @param frame_count 처리할 프레임 수
 * @return ETResult 실행 결과
 */
ETResult dsp_block_diagram_process_ordered(DSPBlockDiagram* diagram,
                                          const int* execution_order,
                                          int order_size, int frame_count);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_DSP_BLOCK_DIAGRAM_H