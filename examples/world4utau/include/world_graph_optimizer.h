#ifndef WORLD_GRAPH_OPTIMIZER_H
#define WORLD_GRAPH_OPTIMIZER_H

#include <libetude/graph.h>
#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/simd_kernels.h>
#include "world_graph_node.h"
#include "world_graph_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 최적화 타입
 */
typedef enum {
    WORLD_OPTIMIZATION_NODE_FUSION,      /**< 노드 융합 */
    WORLD_OPTIMIZATION_MEMORY_REUSE,     /**< 메모리 재사용 */
    WORLD_OPTIMIZATION_SIMD,             /**< SIMD 최적화 */
    WORLD_OPTIMIZATION_PARALLEL,         /**< 병렬 실행 */
    WORLD_OPTIMIZATION_CACHE_LOCALITY,   /**< 캐시 지역성 */
    WORLD_OPTIMIZATION_DEAD_CODE_ELIMINATION, /**< 불필요한 코드 제거 */
    WORLD_OPTIMIZATION_CONSTANT_FOLDING, /**< 상수 접기 */
    WORLD_OPTIMIZATION_LOOP_UNROLLING    /**< 루프 언롤링 */
} WorldOptimizationType;

/**
 * @brief 최적화 레벨
 */
typedef enum {
    WORLD_OPTIMIZATION_LEVEL_NONE,       /**< 최적화 없음 */
    WORLD_OPTIMIZATION_LEVEL_BASIC,      /**< 기본 최적화 */
    WORLD_OPTIMIZATION_LEVEL_AGGRESSIVE, /**< 적극적 최적화 */
    WORLD_OPTIMIZATION_LEVEL_MAXIMUM     /**< 최대 최적화 */
} WorldOptimizationLevel;

/**
 * @brief 그래프 최적화 옵션
 */
typedef struct {
    WorldOptimizationLevel level;        /**< 최적화 레벨 */
    bool enable_node_fusion;             /**< 노드 융합 활성화 */
    bool enable_memory_reuse;            /**< 메모리 재사용 활성화 */
    bool enable_simd_optimization;       /**< SIMD 최적화 활성화 */
    bool enable_parallel_execution;      /**< 병렬 실행 활성화 */
    bool enable_cache_optimization;      /**< 캐시 최적화 활성화 */
    bool enable_dead_code_elimination;   /**< 불필요한 코드 제거 활성화 */
    bool enable_constant_folding;        /**< 상수 접기 활성화 */
    bool enable_loop_unrolling;          /**< 루프 언롤링 활성화 */

    // 병렬 실행 설정
    int max_thread_count;                /**< 최대 스레드 수 */
    bool enable_thread_affinity;        /**< 스레드 친화성 활성화 */

    // 메모리 최적화 설정
    size_t memory_alignment;             /**< 메모리 정렬 크기 */
    bool enable_memory_prefetch;         /**< 메모리 프리페치 활성화 */

    // SIMD 설정
    bool prefer_avx;                     /**< AVX 선호 */
    bool prefer_neon;                    /**< NEON 선호 */
    bool enable_vectorization;           /**< 벡터화 활성화 */
} WorldGraphOptimizationOptions;

/**
 * @brief 최적화 통계
 */
typedef struct {
    int nodes_fused;                     /**< 융합된 노드 수 */
    int memory_allocations_reduced;      /**< 줄어든 메모리 할당 수 */
    size_t memory_saved;                 /**< 절약된 메모리 크기 */
    int simd_operations_added;           /**< 추가된 SIMD 연산 수 */
    int parallel_sections_created;       /**< 생성된 병렬 섹션 수 */
    double optimization_time;            /**< 최적화 소요 시간 */
    double estimated_speedup;            /**< 예상 속도 향상 */
} WorldGraphOptimizationStats;

/**
 * @brief 노드 융합 정보
 */
typedef struct {
    int* node_ids;                       /**< 융합할 노드 ID 배열 */
    int node_count;                      /**< 노드 수 */
    WorldNodeType fused_type;            /**< 융합된 노드 타입 */
    void* fused_data;                    /**< 융합된 노드 데이터 */
    float fusion_benefit;                /**< 융합 이익 점수 */
} WorldNodeFusionInfo;

/**
 * @brief 병렬 실행 계획
 */
typedef struct {
    int** parallel_groups;               /**< 병렬 그룹 배열 */
    int* group_sizes;                    /**< 각 그룹의 크기 */
    int group_count;                     /**< 그룹 수 */
    int* execution_order;                /**< 실행 순서 */
    int total_nodes;                     /**< 총 노드 수 */
} WorldParallelExecutionPlan;

/**
 * @brief 메모리 재사용 계획
 */
typedef struct {
    int* buffer_ids;                     /**< 버퍼 ID 배열 */
    size_t* buffer_sizes;                /**< 버퍼 크기 배열 */
    int buffer_count;                    /**< 버퍼 수 */
    int** reuse_mapping;                 /**< 재사용 매핑 */
    size_t total_memory_saved;           /**< 총 절약된 메모리 */
} WorldMemoryReuseInfo;

// 그래프 최적화 메인 함수들
ETResult world_graph_optimize(ETGraph* graph,
                             const WorldGraphOptimizationOptions* options,
                             WorldGraphOptimizationStats* stats);

ETResult world_graph_optimize_with_builder(WorldGraphBuilder* builder,
                                          const WorldGraphOptimizationOptions* options,
                                          WorldGraphOptimizationStats* stats);

// 개별 최적화 함수들
ETResult world_graph_optimize_node_fusion(WorldGraphBuilder* builder,
                                         WorldNodeFusionInfo** fusion_info,
                                         int* fusion_count);

ETResult world_graph_optimize_memory_reuse(WorldGraphBuilder* builder,
                                          WorldMemoryReuseInfo* reuse_info);

ETResult world_graph_optimize_simd(WorldGraphBuilder* builder);

ETResult world_graph_optimize_parallel_execution(WorldGraphBuilder* builder,
                                                WorldParallelExecutionPlan* plan);

ETResult world_graph_optimize_cache_locality(WorldGraphBuilder* builder);

ETResult world_graph_optimize_dead_code_elimination(WorldGraphBuilder* builder);

// 의존성 분석 및 스케줄링
ETResult world_graph_analyze_dependencies(WorldGraphBuilder* builder,
                                         int** dependency_matrix,
                                         int* node_count);

ETResult world_graph_schedule_parallel_execution(WorldGraphBuilder* builder,
                                                WorldParallelExecutionPlan* plan,
                                                int thread_count);

ETResult world_graph_topological_sort(WorldGraphBuilder* builder,
                                     int** sorted_nodes,
                                     int* node_count);

// 노드 융합 관련 함수들
bool world_graph_can_fuse_nodes(WorldGraphNode* node1, WorldGraphNode* node2);
float world_graph_calculate_fusion_benefit(WorldGraphNode* node1, WorldGraphNode* node2);
ETResult world_graph_fuse_nodes(WorldGraphBuilder* builder,
                               const WorldNodeFusionInfo* fusion_info);

WorldGraphNode* world_graph_create_fused_node(WorldGraphBuilder* builder,
                                             const WorldNodeFusionInfo* fusion_info);

// 메모리 최적화 관련 함수들
ETResult world_graph_analyze_memory_usage(WorldGraphBuilder* builder,
                                         size_t** memory_usage_per_node,
                                         size_t* total_usage);

ETResult world_graph_find_memory_reuse_opportunities(WorldGraphBuilder* builder,
                                                    WorldMemoryReuseInfo* reuse_info);

ETResult world_graph_apply_memory_reuse(WorldGraphBuilder* builder,
                                       const WorldMemoryReuseInfo* reuse_info);

// SIMD 최적화 관련 함수들
ETResult world_graph_identify_simd_opportunities(WorldGraphBuilder* builder,
                                                WorldGraphNode*** simd_nodes,
                                                int* simd_node_count);

ETResult world_graph_apply_simd_optimization(WorldGraphBuilder* builder,
                                            WorldGraphNode** simd_nodes,
                                            int simd_node_count);

bool world_graph_node_supports_simd(WorldGraphNode* node);
ETResult world_graph_convert_node_to_simd(WorldGraphNode* node);

// 병렬 실행 관련 함수들
ETResult world_graph_find_parallel_sections(WorldGraphBuilder* builder,
                                           WorldParallelExecutionPlan* plan);

ETResult world_graph_validate_parallel_plan(WorldGraphBuilder* builder,
                                           const WorldParallelExecutionPlan* plan);

ETResult world_graph_apply_parallel_optimization(WorldGraphBuilder* builder,
                                                const WorldParallelExecutionPlan* plan);

// 캐시 최적화 관련 함수들
ETResult world_graph_analyze_cache_access_patterns(WorldGraphBuilder* builder,
                                                  int** access_matrix,
                                                  int* matrix_size);

ETResult world_graph_reorder_nodes_for_cache(WorldGraphBuilder* builder,
                                            const int* access_matrix,
                                            int matrix_size);

ETResult world_graph_optimize_data_layout(WorldGraphBuilder* builder);

// 최적화 검증 및 평가
ETResult world_graph_validate_optimization(WorldGraphBuilder* builder,
                                          const WorldGraphOptimizationOptions* options);

ETResult world_graph_estimate_performance_gain(WorldGraphBuilder* builder,
                                              const WorldGraphOptimizationStats* stats,
                                              double* estimated_speedup);

ETResult world_graph_benchmark_optimization(WorldGraphBuilder* builder,
                                           const WorldGraphOptimizationOptions* options,
                                           double* before_time,
                                           double* after_time);

// 유틸리티 함수들
WorldGraphOptimizationOptions world_graph_get_default_optimization_options(void);
ETResult world_graph_print_optimization_stats(const WorldGraphOptimizationStats* stats);
ETResult world_graph_export_optimization_report(const WorldGraphOptimizationStats* stats,
                                               const char* filename);

// 메모리 관리
void world_node_fusion_info_destroy(WorldNodeFusionInfo* fusion_info, int count);
void world_parallel_execution_plan_destroy(WorldParallelExecutionPlan* plan);
void world_memory_reuse_info_destroy(WorldMemoryReuseInfo* reuse_info);

#ifdef __cplusplus
}
#endif

#endif // WORLD_GRAPH_OPTIMIZER_H