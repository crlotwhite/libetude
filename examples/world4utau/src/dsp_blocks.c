/**
 * @file dsp_blocks.c
 * @brief DSP 블록 다이어그램 시스템 구현
 */

#include "dsp_blocks.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// =============================================================================
// DSP 블록 기본 함수들
// =============================================================================

DSPBlock* dsp_block_create(const char* name, DSPBlockType type,
                          int input_port_count, int output_port_count,
                          ETMemoryPool* mem_pool) {
    if (!name || input_port_count < 0 || output_port_count < 0) {
        return NULL;
    }

    // 블록 메모리 할당
    DSPBlock* block = (DSPBlock*)et_memory_pool_alloc(mem_pool, sizeof(DSPBlock));
    if (!block) {
        return NULL;
    }

    // 기본 정보 설정
    memset(block, 0, sizeof(DSPBlock));
    strncpy(block->name, name, sizeof(block->name) - 1);
    block->name[sizeof(block->name) - 1] = '\0';
    block->type = type;
    block->input_port_count = input_port_count;
    block->output_port_count = output_port_count;
    block->mem_pool = mem_pool;
    block->is_enabled = true;

    // 입력 포트 배열 할당
    if (input_port_count > 0) {
        block->input_ports = (DSPPort*)et_memory_pool_alloc(
            mem_pool, sizeof(DSPPort) * input_port_count);
        if (!block->input_ports) {
            et_memory_pool_free(mem_pool, block);
            return NULL;
        }
        memset(block->input_ports, 0, sizeof(DSPPort) * input_port_count);

        // 입력 포트 초기화
        for (int i = 0; i < input_port_count; i++) {
            block->input_ports[i].port_id = i;
            block->input_ports[i].direction = DSP_PORT_DIRECTION_INPUT;
            snprintf(block->input_ports[i].name, sizeof(block->input_ports[i].name),
                    "input_%d", i);
        }
    }

    // 출력 포트 배열 할당
    if (output_port_count > 0) {
        block->output_ports = (DSPPort*)et_memory_pool_alloc(
            mem_pool, sizeof(DSPPort) * output_port_count);
        if (!block->output_ports) {
            if (block->input_ports) {
                et_memory_pool_free(mem_pool, block->input_ports);
            }
            et_memory_pool_free(mem_pool, block);
            return NULL;
        }
        memset(block->output_ports, 0, sizeof(DSPPort) * output_port_count);

        // 출력 포트 초기화
        for (int i = 0; i < output_port_count; i++) {
            block->output_ports[i].port_id = i;
            block->output_ports[i].direction = DSP_PORT_DIRECTION_OUTPUT;
            snprintf(block->output_ports[i].name, sizeof(block->output_ports[i].name),
                    "output_%d", i);
        }
    }

    return block;
}

void dsp_block_destroy(DSPBlock* block) {
    if (!block) {
        return;
    }

    // 정리 함수 호출
    if (block->cleanup) {
        block->cleanup(block);
    }

    // 포트 버퍼 해제
    for (int i = 0; i < block->input_port_count; i++) {
        dsp_port_free_buffer(&block->input_ports[i]);
    }
    for (int i = 0; i < block->output_port_count; i++) {
        dsp_port_free_buffer(&block->output_ports[i]);
    }

    // 메모리 해제
    if (block->input_ports) {
        et_memory_pool_free(block->mem_pool, block->input_ports);
    }
    if (block->output_ports) {
        et_memory_pool_free(block->mem_pool, block->output_ports);
    }
    if (block->block_data) {
        et_memory_pool_free(block->mem_pool, block->block_data);
    }

    et_memory_pool_free(block->mem_pool, block);
}

ETResult dsp_block_initialize(DSPBlock* block) {
    if (!block) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (block->is_initialized) {
        return ET_SUCCESS;
    }

    // 포트 버퍼 할당
    for (int i = 0; i < block->input_port_count; i++) {
        ETResult result = dsp_port_allocate_buffer(&block->input_ports[i], block->mem_pool);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    for (int i = 0; i < block->output_port_count; i++) {
        ETResult result = dsp_port_allocate_buffer(&block->output_ports[i], block->mem_pool);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 사용자 정의 초기화 함수 호출
    if (block->initialize) {
        ETResult result = block->initialize(block);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    block->is_initialized = true;
    return ET_SUCCESS;
}

ETResult dsp_block_process(DSPBlock* block, int frame_count) {
    if (!block) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!block->is_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (!block->is_enabled) {
        return ET_SUCCESS; // 비활성화된 블록은 처리하지 않음
    }

    if (!block->process) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return block->process(block, frame_count);
}

void dsp_block_cleanup(DSPBlock* block) {
    if (!block) {
        return;
    }

    if (block->cleanup) {
        block->cleanup(block);
    }

    block->is_initialized = false;
}

// =============================================================================
// DSP 포트 관리 함수들
// =============================================================================

ETResult dsp_block_set_port(DSPBlock* block, int port_index,
                           DSPPortDirection direction, const char* name,
                           DSPPortType type, int buffer_size) {
    if (!block || !name || buffer_size <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    DSPPort* port = NULL;

    if (direction == DSP_PORT_DIRECTION_INPUT) {
        if (port_index >= block->input_port_count) {
            return ET_ERROR_INVALID_PARAMETER;
        }
        port = &block->input_ports[port_index];
    } else {
        if (port_index >= block->output_port_count) {
            return ET_ERROR_INVALID_PARAMETER;
        }
        port = &block->output_ports[port_index];
    }

    // 포트 설정
    strncpy(port->name, name, sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';
    port->type = type;
    port->buffer_size = buffer_size;

    return ET_SUCCESS;
}

DSPPort* dsp_block_get_input_port(DSPBlock* block, int port_index) {
    if (!block || port_index < 0 || port_index >= block->input_port_count) {
        return NULL;
    }

    return &block->input_ports[port_index];
}

DSPPort* dsp_block_get_output_port(DSPBlock* block, int port_index) {
    if (!block || port_index < 0 || port_index >= block->output_port_count) {
        return NULL;
    }

    return &block->output_ports[port_index];
}

ETResult dsp_port_allocate_buffer(DSPPort* port, ETMemoryPool* mem_pool) {
    if (!port || !mem_pool || port->buffer_size <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (port->buffer) {
        return ET_SUCCESS; // 이미 할당됨
    }

    // 포트 타입에 따른 버퍼 크기 계산
    size_t element_size = sizeof(float); // 기본값
    switch (port->type) {
        case DSP_PORT_TYPE_AUDIO:
            element_size = sizeof(float);
            break;
        case DSP_PORT_TYPE_F0:
            element_size = sizeof(double);
            break;
        case DSP_PORT_TYPE_SPECTRUM:
        case DSP_PORT_TYPE_APERIODICITY:
            element_size = sizeof(double*); // 2D 배열 포인터
            break;
        case DSP_PORT_TYPE_PARAMETERS:
            element_size = sizeof(void*); // 구조체 포인터
            break;
        case DSP_PORT_TYPE_CONTROL:
            element_size = sizeof(int);
            break;
    }

    size_t total_size = element_size * port->buffer_size;
    port->buffer = et_memory_pool_alloc(mem_pool, total_size);
    if (!port->buffer) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(port->buffer, 0, total_size);
    return ET_SUCCESS;
}

void dsp_port_free_buffer(DSPPort* port) {
    if (!port || !port->buffer) {
        return;
    }

    // 메모리 풀에서 할당된 메모리는 풀이 해제될 때 함께 해제됨
    port->buffer = NULL;
}

// =============================================================================
// DSP 연결 관리 함수들
// =============================================================================

DSPConnection* dsp_connection_create(DSPBlock* source_block, int source_port_index,
                                    DSPBlock* dest_block, int dest_port_index,
                                    ETMemoryPool* mem_pool) {
    if (!source_block || !dest_block || !mem_pool) {
        return NULL;
    }

    // 포트 유효성 검사
    DSPPort* source_port = dsp_block_get_output_port(source_block, source_port_index);
    DSPPort* dest_port = dsp_block_get_input_port(dest_block, dest_port_index);

    if (!source_port || !dest_port) {
        return NULL;
    }

    // 연결 생성
    DSPConnection* connection = (DSPConnection*)et_memory_pool_alloc(mem_pool, sizeof(DSPConnection));
    if (!connection) {
        return NULL;
    }

    memset(connection, 0, sizeof(DSPConnection));

    // 연결 정보 설정
    connection->source_block_id = source_block->block_id;
    connection->source_port_id = source_port_index;
    connection->source_block = source_block;
    connection->source_port = source_port;

    connection->dest_block_id = dest_block->block_id;
    connection->dest_port_id = dest_port_index;
    connection->dest_block = dest_block;
    connection->dest_port = dest_port;

    connection->buffer_size = dest_port->buffer_size;

    return connection;
}

void dsp_connection_destroy(DSPConnection* connection) {
    if (!connection) {
        return;
    }

    // 연결 비활성화
    dsp_connection_deactivate(connection);

    // 메모리는 메모리 풀에서 관리됨
}

bool dsp_connection_validate(DSPConnection* connection) {
    if (!connection || !connection->source_block || !connection->dest_block ||
        !connection->source_port || !connection->dest_port) {
        return false;
    }

    // 포트 방향 검사
    if (connection->source_port->direction != DSP_PORT_DIRECTION_OUTPUT ||
        connection->dest_port->direction != DSP_PORT_DIRECTION_INPUT) {
        return false;
    }

    // 포트 타입 호환성 검사
    if (connection->source_port->type != connection->dest_port->type) {
        return false;
    }

    // 버퍼 크기 호환성 검사
    if (connection->source_port->buffer_size != connection->dest_port->buffer_size) {
        return false;
    }

    return true;
}

ETResult dsp_connection_activate(DSPConnection* connection) {
    if (!connection) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!dsp_connection_validate(connection)) {
        return ET_ERROR_INVALID_CONNECTION;
    }

    // 포트 연결 설정
    connection->source_port->is_connected = true;
    connection->source_port->connection = connection;

    connection->dest_port->is_connected = true;
    connection->dest_port->connection = connection;

    connection->is_active = true;

    return ET_SUCCESS;
}

void dsp_connection_deactivate(DSPConnection* connection) {
    if (!connection) {
        return;
    }

    // 포트 연결 해제
    if (connection->source_port) {
        connection->source_port->is_connected = false;
        connection->source_port->connection = NULL;
    }

    if (connection->dest_port) {
        connection->dest_port->is_connected = false;
        connection->dest_port->connection = NULL;
    }

    connection->is_active = false;
}