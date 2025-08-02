#include "world_visualization.h"
#include "world_error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// DOT 파일 헤더
static const char* DOT_HEADER =
    "digraph WorldDSPDiagram {\n"
    "    rankdir=LR;\n"
    "    node [shape=box, style=rounded];\n"
    "    edge [fontsize=10];\n"
    "    \n"
    "    // 범례\n"
    "    subgraph cluster_legend {\n"
    "        label=\"Legend\";\n"
    "        style=dashed;\n"
    "        legend_input [label=\"Input Block\", fillcolor=lightblue, style=filled];\n"
    "        legend_process [label=\"Process Block\", fillcolor=lightgreen, style=filled];\n"
    "        legend_output [label=\"Output Block\", fillcolor=lightcoral, style=filled];\n"
    "        legend_error [label=\"Error Block\", fillcolor=red, style=filled];\n"
    "    }\n"
    "    \n";

static const char* DOT_FOOTER = "}\n";

/**
 * @brief 블록 타입에 따른 색상 반환
 */
static const char* get_block_color(const char* block_name) {
    if (strstr(block_name, "input") || strstr(block_name, "Input")) {
        return "lightblue";
    } else if (strstr(block_name, "output") || strstr(block_name, "Output")) {
        return "lightcoral";
    } else if (strstr(block_name, "f0") || strstr(block_name, "F0")) {
        return "lightgreen";
    } else if (strstr(block_name, "spectrum") || strstr(block_name, "Spectrum")) {
        return "lightyellow";
    } else if (strstr(block_name, "aperiodicity") || strstr(block_name, "Aperiodicity")) {
        return "lightpink";
    } else if (strstr(block_name, "synthesis") || strstr(block_name, "Synthesis")) {
        return "lightcyan";
    } else {
        return "white";
    }
}

/**
 * @brief 실행 시간에 따른 테두리 색상 반환
 */
static const char* get_performance_color(double execution_time_ms) {
    if (execution_time_ms < 1.0) {
        return "green";      // 빠름
    } else if (execution_time_ms < 10.0) {
        return "orange";     // 보통
    } else {
        return "red";        // 느림
    }
}

VisualizationContext* world_visualization_create(const VisualizationConfig* config) {
    if (!config) {
        WORLD_LOG_ERROR(VISUALIZATION, "설정이 NULL입니다");
        return NULL;
    }

    VisualizationContext* viz_ctx = calloc(1, sizeof(VisualizationContext));
    if (!viz_ctx) {
        WORLD_LOG_ERROR(VISUALIZATION, "시각화 컨텍스트 메모리 할당 실패");
        return NULL;
    }

    // 설정 복사
    memcpy(&viz_ctx->config, config, sizeof(VisualizationConfig));

    // DOT 내용 버퍼 초기화
    viz_ctx->dot_content_size = 4096;
    viz_ctx->dot_content = malloc(viz_ctx->dot_content_size);
    if (!viz_ctx->dot_content) {
        WORLD_LOG_ERROR(VISUALIZATION, "DOT 내용 버퍼 메모리 할당 실패");
        free(viz_ctx);
        return NULL;
    }

    // 초기 블록 통계 배열 할당
    viz_ctx->block_count = 0;
    viz_ctx->block_stats = malloc(sizeof(BlockExecutionStats) * 32);
    if (!viz_ctx->block_stats) {
        WORLD_LOG_ERROR(VISUALIZATION, "블록 통계 배열 메모리 할당 실패");
        free(viz_ctx->dot_content);
        free(viz_ctx);
        return NULL;
    }

    WORLD_LOG_INFO(VISUALIZATION, "시각화 컨텍스트 생성 완료");
    return viz_ctx;
}

int world_visualization_export_dsp_diagram(VisualizationContext* viz_ctx,
                                          const DSPBlockDiagram* diagram,
                                          const char* output_path) {
    if (!viz_ctx || !diagram || !output_path) {
        WORLD_LOG_ERROR(VISUALIZATION, "잘못된 매개변수");
        return -1;
    }

    // 출력 파일 열기
    viz_ctx->output_file = fopen(output_path, "w");
    if (!viz_ctx->output_file) {
        WORLD_LOG_ERROR(VISUALIZATION, "출력 파일 열기 실패: %s", output_path);
        return -1;
    }

    // DOT 헤더 작성
    fprintf(viz_ctx->output_file, "%s", DOT_HEADER);

    // 블록들을 DOT 노드로 변환
    fprintf(viz_ctx->output_file, "    // DSP 블록들\n");
    for (int i = 0; i < diagram->block_count; i++) {
        const DSPBlock* block = &diagram->blocks[i];
        const char* color = viz_ctx->config.use_colors ? get_block_color(block->name) : "white";

        fprintf(viz_ctx->output_file, "    \"%s\" [", block->name);
        fprintf(viz_ctx->output_file, "label=\"%s", block->name);

        // 실행 통계 정보 추가
        if (viz_ctx->config.show_execution_time) {
            for (int j = 0; j < viz_ctx->block_count; j++) {
                if (strcmp(viz_ctx->block_stats[j].block_name, block->name) == 0) {
                    fprintf(viz_ctx->output_file, "\\n실행시간: %.2fms",
                           viz_ctx->block_stats[j].execution_time_ms);

                    if (viz_ctx->config.show_memory_usage) {
                        fprintf(viz_ctx->output_file, "\\n메모리: %zuB",
                               viz_ctx->block_stats[j].memory_usage_bytes);
                    }

                    // 성능에 따른 테두리 색상
                    if (viz_ctx->config.use_colors) {
                        const char* perf_color = get_performance_color(
                            viz_ctx->block_stats[j].execution_time_ms);
                        fprintf(viz_ctx->output_file, "\", fillcolor=%s, color=%s, style=\"filled,bold\"",
                               color, perf_color);
                    }
                    break;
                }
            }
        }

        if (!viz_ctx->config.show_execution_time || viz_ctx->config.use_colors) {
            fprintf(viz_ctx->output_file, "\", fillcolor=%s, style=filled", color);
        }

        fprintf(viz_ctx->output_file, "];\n");
    }

    // 연결들을 DOT 엣지로 변환
    fprintf(viz_ctx->output_file, "\n    // 블록 연결들\n");
    for (int i = 0; i < diagram->connection_count; i++) {
        const DSPConnection* conn = &diagram->connections[i];

        if (conn->source_block_id < diagram->block_count &&
            conn->dest_block_id < diagram->block_count) {

            const char* source_name = diagram->blocks[conn->source_block_id].name;
            const char* dest_name = diagram->blocks[conn->dest_block_id].name;

            fprintf(viz_ctx->output_file, "    \"%s\" -> \"%s\"", source_name, dest_name);

            // 데이터 흐름 정보 추가
            if (viz_ctx->config.show_data_flow) {
                fprintf(viz_ctx->output_file, " [label=\"포트 %d→%d\\n버퍼: %dB\"",
                       conn->source_port, conn->dest_port, conn->buffer_size);

                if (viz_ctx->config.use_colors) {
                    // 버퍼 크기에 따른 엣지 색상
                    const char* edge_color = conn->buffer_size > 1024 ? "red" :
                                           conn->buffer_size > 256 ? "orange" : "green";
                    fprintf(viz_ctx->output_file, ", color=%s", edge_color);
                }

                fprintf(viz_ctx->output_file, "]");
            }

            fprintf(viz_ctx->output_file, ";\n");
        }
    }

    // DOT 푸터 작성
    fprintf(viz_ctx->output_file, "%s", DOT_FOOTER);

    fclose(viz_ctx->output_file);
    viz_ctx->output_file = NULL;

    WORLD_LOG_INFO(VISUALIZATION, "DSP 다이어그램 DOT 파일 생성 완료: %s", output_path);
    return 0;
}

int world_visualization_export_graph_nodes(VisualizationContext* viz_ctx,
                                          const WorldGraphContext* graph_context,
                                          const char* output_path) {
    if (!viz_ctx || !graph_context || !output_path) {
        WORLD_LOG_ERROR(VISUALIZATION, "잘못된 매개변수");
        return -1;
    }

    // 출력 파일 열기
    viz_ctx->output_file = fopen(output_path, "w");
    if (!viz_ctx->output_file) {
        WORLD_LOG_ERROR(VISUALIZATION, "출력 파일 열기 실패: %s", output_path);
        return -1;
    }

    // DOT 헤더 작성
    fprintf(viz_ctx->output_file, "%s", DOT_HEADER);

    // 그래프 노드들을 DOT 노드로 변환
    fprintf(viz_ctx->output_file, "    // 그래프 노드들\n");

    // 노드 타입별 서브그래프 생성
    fprintf(viz_ctx->output_file, "    subgraph cluster_analysis {\n");
    fprintf(viz_ctx->output_file, "        label=\"Analysis Stage\";\n");
    fprintf(viz_ctx->output_file, "        style=dashed;\n");
    fprintf(viz_ctx->output_file, "        color=blue;\n");

    // 분석 단계 노드들 (예시)
    fprintf(viz_ctx->output_file, "        \"F0_Extraction\" [fillcolor=lightgreen, style=filled];\n");
    fprintf(viz_ctx->output_file, "        \"Spectrum_Analysis\" [fillcolor=lightyellow, style=filled];\n");
    fprintf(viz_ctx->output_file, "        \"Aperiodicity_Analysis\" [fillcolor=lightpink, style=filled];\n");
    fprintf(viz_ctx->output_file, "    }\n\n");

    fprintf(viz_ctx->output_file, "    subgraph cluster_synthesis {\n");
    fprintf(viz_ctx->output_file, "        label=\"Synthesis Stage\";\n");
    fprintf(viz_ctx->output_file, "        style=dashed;\n");
    fprintf(viz_ctx->output_file, "        color=red;\n");
    fprintf(viz_ctx->output_file, "        \"Parameter_Merge\" [fillcolor=lightcyan, style=filled];\n");
    fprintf(viz_ctx->output_file, "        \"Voice_Synthesis\" [fillcolor=lightcoral, style=filled];\n");
    fprintf(viz_ctx->output_file, "    }\n\n");

    // 노드 간 연결
    fprintf(viz_ctx->output_file, "    // 노드 연결들\n");
    fprintf(viz_ctx->output_file, "    \"F0_Extraction\" -> \"Parameter_Merge\";\n");
    fprintf(viz_ctx->output_file, "    \"Spectrum_Analysis\" -> \"Parameter_Merge\";\n");
    fprintf(viz_ctx->output_file, "    \"Aperiodicity_Analysis\" -> \"Parameter_Merge\";\n");
    fprintf(viz_ctx->output_file, "    \"Parameter_Merge\" -> \"Voice_Synthesis\";\n");

    // 실행 상태 정보 추가
    if (graph_context->is_analysis_complete) {
        fprintf(viz_ctx->output_file, "    \"Analysis_Complete\" [shape=ellipse, fillcolor=green, style=filled];\n");
    }

    if (graph_context->is_synthesis_complete) {
        fprintf(viz_ctx->output_file, "    \"Synthesis_Complete\" [shape=ellipse, fillcolor=green, style=filled];\n");
    }

    // 성능 정보 추가
    if (viz_ctx->config.show_execution_time) {
        fprintf(viz_ctx->output_file, "\n    // 성능 정보\n");
        fprintf(viz_ctx->output_file, "    \"Performance_Info\" [shape=note, label=\"분석 시간: %.2fms\\n합성 시간: %.2fms\\n메모리 사용량: %zuB\"];\n",
               graph_context->analysis_time, graph_context->synthesis_time, graph_context->memory_usage);
    }

    // DOT 푸터 작성
    fprintf(viz_ctx->output_file, "%s", DOT_FOOTER);

    fclose(viz_ctx->output_file);
    viz_ctx->output_file = NULL;

    WORLD_LOG_INFO(VISUALIZATION, "그래프 노드 DOT 파일 생성 완료: %s", output_path);
    return 0;
}

int world_visualization_add_execution_stats(VisualizationContext* viz_ctx,
                                           const char* block_name,
                                           double execution_time,
                                           size_t memory_usage) {
    if (!viz_ctx || !block_name) {
        WORLD_LOG_ERROR(VISUALIZATION, "잘못된 매개변수");
        return -1;
    }

    // 기존 통계 찾기
    for (int i = 0; i < viz_ctx->block_count; i++) {
        if (strcmp(viz_ctx->block_stats[i].block_name, block_name) == 0) {
            // 기존 통계 업데이트
            viz_ctx->block_stats[i].execution_time_ms = execution_time;
            viz_ctx->block_stats[i].memory_usage_bytes = memory_usage;
            viz_ctx->block_stats[i].execution_count++;
            return 0;
        }
    }

    // 새 통계 추가
    if (viz_ctx->block_count >= 32) {
        WORLD_LOG_ERROR(VISUALIZATION, "블록 통계 배열이 가득참");
        return -1;
    }

    BlockExecutionStats* stats = &viz_ctx->block_stats[viz_ctx->block_count];
    strncpy(stats->block_name, block_name, sizeof(stats->block_name) - 1);
    stats->block_name[sizeof(stats->block_name) - 1] = '\0';
    stats->execution_time_ms = execution_time;
    stats->memory_usage_bytes = memory_usage;
    stats->execution_count = 1;
    stats->has_error = false;
    stats->error_message[0] = '\0';

    viz_ctx->block_count++;

    WORLD_LOG_DEBUG(VISUALIZATION, "블록 '%s' 실행 통계 추가: %.2fms, %zuB",
                   block_name, execution_time, memory_usage);
    return 0;
}

int world_visualization_add_data_flow(VisualizationContext* viz_ctx,
                                     const char* source_block,
                                     const char* dest_block,
                                     size_t data_size,
                                     double transfer_time) {
    if (!viz_ctx || !source_block || !dest_block) {
        WORLD_LOG_ERROR(VISUALIZATION, "잘못된 매개변수");
        return -1;
    }

    // 데이터 흐름 정보는 현재 DOT 생성 시에 직접 사용
    // 향후 확장을 위해 로그만 남김
    WORLD_LOG_DEBUG(VISUALIZATION, "데이터 흐름: %s -> %s (%zuB, %.2fms)",
                   source_block, dest_block, data_size, transfer_time);
    return 0;
}

int world_visualization_render_to_image(const char* dot_file_path,
                                       const char* output_path,
                                       const char* format) {
    if (!dot_file_path || !output_path || !format) {
        WORLD_LOG_ERROR(VISUALIZATION, "잘못된 매개변수");
        return -1;
    }

    // Graphviz 명령어 구성
    char command[1024];
    snprintf(command, sizeof(command), "dot -T%s \"%s\" -o \"%s\"",
             format, dot_file_path, output_path);

    WORLD_LOG_INFO(VISUALIZATION, "Graphviz 실행: %s", command);

    // 시스템 명령어 실행
    int result = system(command);
    if (result != 0) {
        WORLD_LOG_ERROR(VISUALIZATION, "Graphviz 실행 실패 (코드: %d). Graphviz가 설치되어 있는지 확인하세요.", result);
        return -1;
    }

    WORLD_LOG_INFO(VISUALIZATION, "이미지 생성 완료: %s", output_path);
    return 0;
}

void world_visualization_destroy(VisualizationContext* viz_ctx) {
    if (!viz_ctx) {
        return;
    }

    if (viz_ctx->output_file) {
        fclose(viz_ctx->output_file);
    }

    free(viz_ctx->dot_content);
    free(viz_ctx->block_stats);
    free(viz_ctx);

    WORLD_LOG_INFO(VISUALIZATION, "시각화 컨텍스트 해제 완료");
}