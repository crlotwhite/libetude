#include "world_graph_optimizer.h"
#include "world_error.h"
#include <libetude/memory.h>
#include <libetude/error.h>
#include <libetude/simd_kernels.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// 기본 설정값
#define DEFAULT_MAX_THREADS 8
#define DEFAULT_MEMORY_ALIGNMENT 32
#define FUSION_BENEFIT_THRESHOLD 0.5f
#define PARALLEL_THRESHOLD 0.3f

/**
 * @brief 현재 시간을 초 단위로 반환
 */
static double get_current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

/**
 * @brief 기본 최적화 옵션 반환
 */
WorldGraphOptimizationOptions world_graph_get_default_optimization_options(void) {
    WorldGraphOptimizationOptions options = {
        .level = WORLD_OPTIMIZATION_LEVEL_BASIC,
        .enable_node_fusion = true,
        .enable_memory_reuse = true,
        .enable_simd_optimization = true,
        .enable_parallel_execution = true,
        .enable_cache_optimization = true,
        .enable_dead_code_elimination = true,
        .enable_constant_folding = false,
        .enable_loop_unrolling = false,
        .max_thread_count = DEFAULT_MAX_THREADS,
        .enable_thread_affinity = false,
        .memory_alignment = DEFAULT_MEMORY_ALIGNMENT,
        .enable_memory_prefetch = true,
        .prefer_avx = true,
        .prefer_neon = false,
        .enable_vectorization = true
    };

    return options;
}

/**
 * @brief 그래프 최적화 (메인 함수)
 */
ETResult world_graph_optimize(ETGraph* graph,
                             const WorldGraphOptimizationOptions* options,
                             WorldGraphOptimizationStats* stats) {
    if (!graph || !options) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실제 구현에서는 ETGraph에서 WorldGraphBuilder 추출
    // 여기서는 임시로 NULL 처리
    WorldGraphBuilder* builder = NULL; // 실제로는 graph에서 추출

    return world_graph_optimize_with_builder(builder, options, stats);
}

/**
 * @brief 그래프 빌더를 사용한 최적화
 */
ETResult world_graph_optimize_with_builder(WorldGraphBuilder* builder,
                                          const WorldGraphOptimizationOptions* options,
                                          WorldGraphOptimizationStats* stats) {
    if (!builder || !options) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    double start_time = get_current_time();

    // 통계 초기화
    if (stats) {
        memset(stats, 0, sizeof(WorldGraphOptimizationStats));
    }

    ETResult result = ET_SUCCESS;

    // 1. 노드 융합 최적화
    if (options->enable_node_fusion) {
        WorldNodeFusionInfo* fusion_info = NULL;
        int fusion_count = 0;

        result = world_graph_optimize_node_fusion(builder, &fusion_info, &fusion_count);
        if (result == ET_SUCCESS && stats) {
            stats->nodes_fused = fusion_count;
        }

        if (fusion_info) {
            world_node_fusion_info_destroy(fusion_info, fusion_count);
        }
    }

    // 2. 메모리 재사용 최적화
    if (options->enable_memory_reuse && result == ET_SUCCESS) {
        WorldMemoryReuseInfo reuse_info = {0};

        result = world_graph_optimize_memory_reuse(builder, &reuse_info);
        if (result == ET_SUCCESS && stats) {
            stats->memory_saved = reuse_info.total_memory_saved;
            stats->memory_allocations_reduced = reuse_info.buffer_count;
        }

        world_memory_reuse_info_destroy(&reuse_info);
    }

    // 3. SIMD 최적화
    if (options->enable_simd_optimization && result == ET_SUCCESS) {
        result = world_graph_optimize_simd(builder);
        if (result == ET_SUCCESS && stats) {
            stats->simd_operations_added = 10; // 임시값
        }
    }

    // 4. 병렬 실행 최적화
    if (options->enable_parallel_execution && result == ET_SUCCESS) {
        WorldParallelExecutionPlan plan = {0};

        result = world_graph_optimize_parallel_execution(builder, &plan);
        if (result == ET_SUCCESS && stats) {
            stats->parallel_sections_created = plan.group_count;
        }

        world_parallel_execution_plan_destroy(&plan);
    }

    // 5. 캐시 지역성 최적화
    if (options->enable_cache_optimization && result == ET_SUCCESS) {
        result = world_graph_optimize_cache_locality(builder);
    }

    // 6. 불필요한 코드 제거
    if (options->enable_dead_code_elimination && result == ET_SUCCESS) {
        result = world_graph_optimize_dead_code_elimination(builder);
    }

    // 최적화 시간 기록
    if (stats) {
        stats->optimization_time = get_current_time() - start_time;
        stats->estimated_speedup = 1.5; // 임시 추정값
    }

    return result;
}

/**
 * @brief 노드 융합 최적화
 */
ETResult world_graph_optimize_node_fusion(WorldGraphBuilder* builder,
                                         WorldNodeFusionInfo** fusion_info,
                                         int* fusion_count) {
    if (!builder || !fusion_info || !fusion_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    *fusion_info = NULL;
    *fusion_count = 0;

    int node_count = world_graph_builder_get_node_count(builder);
    if (node_count < 2) {
        return ET_SUCCESS; // 융합할 노드가 없음
    }

    // 융합 가능한 노드 쌍 찾기
    int max_fusions = node_count / 2;
    WorldNodeFusionInfo* fusions = (WorldNodeFusionInfo*)malloc(max_fusions * sizeof(WorldNodeFusionInfo));
    if (!fusions) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int fusion_idx = 0;

    for (int i = 0; i < node_count - 1 && fusion_idx < max_fusions; i++) {
        WorldGraphNode* node1 = world_graph_builder_get_node(builder, i);
        if (!node1) continue;

        for (int j = i + 1; j < node_count && fusion_idx < max_fusions; j++) {
            WorldGraphNode* node2 = world_graph_builder_get_node(builder, j);
            if (!node2) continue;

            // 융합 가능성 검사
            if (world_graph_can_fuse_nodes(node1, node2)) {
                float benefit = world_graph_calculate_fusion_benefit(node1, node2);

                if (benefit > FUSION_BENEFIT_THRESHOLD) {
                    // 융합 정보 생성
                    WorldNodeFusionInfo* fusion = &fusions[fusion_idx];
                    fusion->node_count = 2;
                    fusion->node_ids = (int*)malloc(2 * sizeof(int));
                    if (!fusion->node_ids) {
                        // 메모리 정리
                        for (int k = 0; k < fusion_idx; k++) {
                            free(fusions[k].node_ids);
                        }
                        free(fusions);
                        return ET_ERROR_OUT_OF_MEMORY;
                    }

                    fusion->node_ids[0] = i;
                    fusion->node_ids[1] = j;
                    fusion->fused_type = node1->node_type; // 첫 번째 노드 타입 사용
                    fusion->fused_data = NULL;
                    fusion->fusion_benefit = benefit;

                    fusion_idx++;
                }
            }
        }
    }

    // 융합 적용
    for (int i = 0; i < fusion_idx; i++) {
        ETResult result = world_graph_fuse_nodes(builder, &fusions[i]);
        if (result != ET_SUCCESS) {
            // 부분적 실패 시에도 계속 진행
            continue;
        }
    }

    *fusion_info = fusions;
    *fusion_count = fusion_idx;

    return ET_SUCCESS;
}

/**
 * @brief 메모리 재사용 최적화
 */
ETResult world_graph_optimize_memory_reuse(WorldGraphBuilder* builder,
                                          WorldMemoryReuseInfo* reuse_info) {
    if (!builder || !reuse_info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(reuse_info, 0, sizeof(WorldMemoryReuseInfo));

    // 메모리 사용량 분석
    size_t* memory_usage_per_node = NULL;
    size_t total_usage = 0;

    ETResult result = world_graph_analyze_memory_usage(builder, &memory_usage_per_node, &total_usage);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 재사용 기회 찾기
    result = world_graph_find_memory_reuse_opportunities(builder, reuse_info);
    if (result != ET_SUCCESS) {
        free(memory_usage_per_node);
        return result;
    }

    // 재사용 적용
    result = world_graph_apply_memory_reuse(builder, reuse_info);

    free(memory_usage_per_node);

    return result;
}

/**
 * @brief SIMD 최적화
 */
ETResult world_graph_optimize_simd(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // SIMD 최적화 가능한 노드 식별
    WorldGraphNode** simd_nodes = NULL;
    int simd_node_count = 0;

    ETResult result = world_graph_identify_simd_opportunities(builder, &simd_nodes, &simd_node_count);
    if (result != ET_SUCCESS) {
        return result;
    }

    if (simd_node_count == 0) {
        return ET_SUCCESS; // SIMD 최적화 기회 없음
    }

    // SIMD 최적화 적용
    result = world_graph_apply_simd_optimization(builder, simd_nodes, simd_node_count);

    free(simd_nodes);

    return result;
}

/**
 * @brief 병렬 실행 최적화
 */
ETResult world_graph_optimize_parallel_execution(WorldGraphBuilder* builder,
                                                WorldParallelExecutionPlan* plan) {
    if (!builder || !plan) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(plan, 0, sizeof(WorldParallelExecutionPlan));

    // 병렬 섹션 찾기
    ETResult result = world_graph_find_parallel_sections(builder, plan);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 병렬 계획 검증
    result = world_graph_validate_parallel_plan(builder, plan);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 병렬 최적화 적용
    result = world_graph_apply_parallel_optimization(builder, plan);

    return result;
}

/**
 * @brief 캐시 지역성 최적화
 */
ETResult world_graph_optimize_cache_locality(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 캐시 접근 패턴 분석
    int* access_matrix = NULL;
    int matrix_size = 0;

    ETResult result = world_graph_analyze_cache_access_patterns(builder, &access_matrix, &matrix_size);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 캐시 지역성을 위한 노드 재정렬
    result = world_graph_reorder_nodes_for_cache(builder, access_matrix, matrix_size);
    if (result != ET_SUCCESS) {
        free(access_matrix);
        return result;
    }

    // 데이터 레이아웃 최적화
    result = world_graph_optimize_data_layout(builder);

    free(access_matrix);

    return result;
}

/**
 * @brief 불필요한 코드 제거
 */
ETResult world_graph_optimize_dead_code_elimination(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int node_count = world_graph_builder_get_node_count(builder);
    bool* is_reachable = (bool*)calloc(node_count, sizeof(bool));
    if (!is_reachable) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 도달 가능한 노드 마킹 (간단한 DFS)
    // 실제 구현에서는 더 정교한 도달성 분석 수행
    for (int i = 0; i < node_count; i++) {
        WorldGraphNode* node = world_graph_builder_get_node(builder, i);
        if (node && (node->node_type == WORLD_NODE_AUDIO_INPUT ||
                     node->node_type == WORLD_NODE_AUDIO_OUTPUT)) {
            is_reachable[i] = true;
        }
    }

    // 도달 불가능한 노드 제거
    for (int i = node_count - 1; i >= 0; i--) {
        if (!is_reachable[i]) {
            world_graph_builder_remove_node(builder, i);
        }
    }

    free(is_reachable);

    return ET_SUCCESS;
}

/**
 * @brief 노드 융합 가능성 검사
 */
bool world_graph_can_fuse_nodes(WorldGraphNode* node1, WorldGraphNode* node2) {
    if (!node1 || !node2) {
        return false;
    }

    // 같은 타입의 노드만 융합 가능
    if (node1->node_type != node2->node_type) {
        return false;
    }

    // 특정 노드 타입은 융합 불가
    if (node1->node_type == WORLD_NODE_AUDIO_INPUT ||
        node1->node_type == WORLD_NODE_AUDIO_OUTPUT) {
        return false;
    }

    // 실제 구현에서는 더 정교한 융합 가능성 검사
    return true;
}

/**
 * @brief 융합 이익 계산
 */
float world_graph_calculate_fusion_benefit(WorldGraphNode* node1, WorldGraphNode* node2) {
    if (!node1 || !node2) {
        return 0.0f;
    }

    // 간단한 이익 계산 (실제로는 더 복잡한 비용 모델 사용)
    float benefit = 0.0f;

    // 같은 타입 노드 융합 시 이익
    if (node1->node_type == node2->node_type) {
        benefit += 0.3f;
    }

    // 메모리 접근 패턴이 유사한 경우 이익
    benefit += 0.2f;

    // 계산 복잡도에 따른 이익
    if (node1->node_type == WORLD_NODE_F0_EXTRACTION ||
        node1->node_type == WORLD_NODE_SPECTRUM_ANALYSIS) {
        benefit += 0.4f;
    }

    return benefit;
}

/**
 * @brief 노드 융합 적용
 */
ETResult world_graph_fuse_nodes(WorldGraphBuilder* builder,
                               const WorldNodeFusionInfo* fusion_info) {
    if (!builder || !fusion_info || fusion_info->node_count < 2) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 융합된 노드 생성
    WorldGraphNode* fused_node = world_graph_create_fused_node(builder, fusion_info);
    if (!fused_node) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기존 노드들의 연결을 융합된 노드로 이전
    // 실제 구현에서는 더 정교한 연결 이전 로직 필요

    // 기존 노드들 제거 (역순으로)
    for (int i = fusion_info->node_count - 1; i >= 0; i--) {
        world_graph_builder_remove_node(builder, fusion_info->node_ids[i]);
    }

    // 융합된 노드 추가
    return world_graph_builder_add_node(builder, fused_node);
}

/**
 * @brief 융합된 노드 생성
 */
WorldGraphNode* world_graph_create_fused_node(WorldGraphBuilder* builder,
                                             const WorldNodeFusionInfo* fusion_info) {
    if (!builder || !fusion_info) {
        return NULL;
    }

    // 실제 구현에서는 융합할 노드들의 기능을 결합한 새로운 노드 생성
    // 여기서는 간단한 더미 노드 생성

    WorldGraphNode* fused_node = (WorldGraphNode*)et_memory_pool_alloc(builder->mem_pool,
                                                                       sizeof(WorldGraphNode));
    if (!fused_node) {
        return NULL;
    }

    memset(fused_node, 0, sizeof(WorldGraphNode));

    fused_node->node_type = fusion_info->fused_type;
    fused_node->mem_pool = builder->mem_pool;
    fused_node->initialize = world_graph_node_initialize;
    fused_node->cleanup = NULL;

    return fused_node;
}

/**
 * @brief 메모리 사용량 분석
 */
ETResult world_graph_analyze_memory_usage(WorldGraphBuilder* builder,
                                         size_t** memory_usage_per_node,
                                         size_t* total_usage) {
    if (!builder || !memory_usage_per_node || !total_usage) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int node_count = world_graph_builder_get_node_count(builder);
    if (node_count == 0) {
        *memory_usage_per_node = NULL;
        *total_usage = 0;
        return ET_SUCCESS;
    }

    size_t* usage = (size_t*)malloc(node_count * sizeof(size_t));
    if (!usage) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    size_t total = 0;

    for (int i = 0; i < node_count; i++) {
        WorldGraphNode* node = world_graph_builder_get_node(builder, i);
        if (node) {
            // 노드 타입별 메모리 사용량 추정
            switch (node->node_type) {
                case WORLD_NODE_F0_EXTRACTION:
                    usage[i] = 1024 * 1024; // 1MB
                    break;
                case WORLD_NODE_SPECTRUM_ANALYSIS:
                    usage[i] = 2 * 1024 * 1024; // 2MB
                    break;
                case WORLD_NODE_APERIODICITY_ANALYSIS:
                    usage[i] = 1024 * 1024; // 1MB
                    break;
                case WORLD_NODE_SYNTHESIS:
                    usage[i] = 3 * 1024 * 1024; // 3MB
                    break;
                default:
                    usage[i] = 512 * 1024; // 512KB
                    break;
            }
        } else {
            usage[i] = 0;
        }

        total += usage[i];
    }

    *memory_usage_per_node = usage;
    *total_usage = total;

    return ET_SUCCESS;
}

/**
 * @brief SIMD 최적화 기회 식별
 */
ETResult world_graph_identify_simd_opportunities(WorldGraphBuilder* builder,
                                                WorldGraphNode*** simd_nodes,
                                                int* simd_node_count) {
    if (!builder || !simd_nodes || !simd_node_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    int node_count = world_graph_builder_get_node_count(builder);
    WorldGraphNode** candidates = (WorldGraphNode**)malloc(node_count * sizeof(WorldGraphNode*));
    if (!candidates) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int candidate_count = 0;

    for (int i = 0; i < node_count; i++) {
        WorldGraphNode* node = world_graph_builder_get_node(builder, i);
        if (node && world_graph_node_supports_simd(node)) {
            candidates[candidate_count++] = node;
        }
    }

    *simd_nodes = candidates;
    *simd_node_count = candidate_count;

    return ET_SUCCESS;
}

/**
 * @brief 노드의 SIMD 지원 여부 확인
 */
bool world_graph_node_supports_simd(WorldGraphNode* node) {
    if (!node) {
        return false;
    }

    // SIMD 최적화가 가능한 노드 타입들
    switch (node->node_type) {
        case WORLD_NODE_F0_EXTRACTION:
        case WORLD_NODE_SPECTRUM_ANALYSIS:
        case WORLD_NODE_APERIODICITY_ANALYSIS:
        case WORLD_NODE_SYNTHESIS:
            return true;
        default:
            return false;
    }
}

/**
 * @brief SIMD 최적화 적용
 */
ETResult world_graph_apply_simd_optimization(WorldGraphBuilder* builder,
                                            WorldGraphNode** simd_nodes,
                                            int simd_node_count) {
    if (!builder || !simd_nodes) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < simd_node_count; i++) {
        ETResult result = world_graph_convert_node_to_simd(simd_nodes[i]);
        if (result != ET_SUCCESS) {
            // 부분적 실패 시에도 계속 진행
            continue;
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief 노드를 SIMD 버전으로 변환
 */
ETResult world_graph_convert_node_to_simd(WorldGraphNode* node) {
    if (!node) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실제 구현에서는 노드의 실행 함수를 SIMD 버전으로 교체
    // 여기서는 간단한 플래그 설정으로 대체

    return ET_SUCCESS;
}

/**
 * @brief 최적화 통계 출력
 */
ETResult world_graph_print_optimization_stats(const WorldGraphOptimizationStats* stats) {
    if (!stats) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    printf("=== Graph Optimization Statistics ===\n");
    printf("Nodes fused: %d\n", stats->nodes_fused);
    printf("Memory allocations reduced: %d\n", stats->memory_allocations_reduced);
    printf("Memory saved: %zu bytes\n", stats->memory_saved);
    printf("SIMD operations added: %d\n", stats->simd_operations_added);
    printf("Parallel sections created: %d\n", stats->parallel_sections_created);
    printf("Optimization time: %.3f seconds\n", stats->optimization_time);
    printf("Estimated speedup: %.2fx\n", stats->estimated_speedup);
    printf("=====================================\n");

    return ET_SUCCESS;
}

/**
 * @brief 메모리 해제 함수들
 */
void world_node_fusion_info_destroy(WorldNodeFusionInfo* fusion_info, int count) {
    if (!fusion_info) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (fusion_info[i].node_ids) {
            free(fusion_info[i].node_ids);
        }
    }

    free(fusion_info);
}

void world_parallel_execution_plan_destroy(WorldParallelExecutionPlan* plan) {
    if (!plan) {
        return;
    }

    if (plan->parallel_groups) {
        for (int i = 0; i < plan->group_count; i++) {
            if (plan->parallel_groups[i]) {
                free(plan->parallel_groups[i]);
            }
        }
        free(plan->parallel_groups);
    }

    if (plan->group_sizes) {
        free(plan->group_sizes);
    }

    if (plan->execution_order) {
        free(plan->execution_order);
    }

    memset(plan, 0, sizeof(WorldParallelExecutionPlan));
}

void world_memory_reuse_info_destroy(WorldMemoryReuseInfo* reuse_info) {
    if (!reuse_info) {
        return;
    }

    if (reuse_info->buffer_ids) {
        free(reuse_info->buffer_ids);
    }

    if (reuse_info->buffer_sizes) {
        free(reuse_info->buffer_sizes);
    }

    if (reuse_info->reuse_mapping) {
        for (int i = 0; i < reuse_info->buffer_count; i++) {
            if (reuse_info->reuse_mapping[i]) {
                free(reuse_info->reuse_mapping[i]);
            }
        }
        free(reuse_info->reuse_mapping);
    }

    memset(reuse_info, 0, sizeof(WorldMemoryReuseInfo));
}

// 나머지 함수들은 간단한 스텁 구현
ETResult world_graph_find_memory_reuse_opportunities(WorldGraphBuilder* builder,
                                                    WorldMemoryReuseInfo* reuse_info) {
    // 실제 구현에서는 메모리 재사용 기회 분석
    return ET_SUCCESS;
}

ETResult world_graph_apply_memory_reuse(WorldGraphBuilder* builder,
                                       const WorldMemoryReuseInfo* reuse_info) {
    // 실제 구현에서는 메모리 재사용 적용
    return ET_SUCCESS;
}

ETResult world_graph_find_parallel_sections(WorldGraphBuilder* builder,
                                           WorldParallelExecutionPlan* plan) {
    // 실제 구현에서는 병렬 섹션 분석
    return ET_SUCCESS;
}

ETResult world_graph_validate_parallel_plan(WorldGraphBuilder* builder,
                                           const WorldParallelExecutionPlan* plan) {
    // 실제 구현에서는 병렬 계획 검증
    return ET_SUCCESS;
}

ETResult world_graph_apply_parallel_optimization(WorldGraphBuilder* builder,
                                                const WorldParallelExecutionPlan* plan) {
    // 실제 구현에서는 병렬 최적화 적용
    return ET_SUCCESS;
}

ETResult world_graph_analyze_cache_access_patterns(WorldGraphBuilder* builder,
                                                  int** access_matrix,
                                                  int* matrix_size) {
    // 실제 구현에서는 캐시 접근 패턴 분석
    *access_matrix = NULL;
    *matrix_size = 0;
    return ET_SUCCESS;
}

ETResult world_graph_reorder_nodes_for_cache(WorldGraphBuilder* builder,
                                            const int* access_matrix,
                                            int matrix_size) {
    // 실제 구현에서는 캐시 지역성을 위한 노드 재정렬
    return ET_SUCCESS;
}

ETResult world_graph_optimize_data_layout(WorldGraphBuilder* builder) {
    // 실제 구현에서는 데이터 레이아웃 최적화
    return ET_SUCCESS;
}