/**
 * @file world_pipeline.c
 * @brief 통합 WORLD 처리 파이프라인 구현
 */

#include "world_pipeline.h"
#include "world_pipeline_config.h"
#include "world_streaming.h"
#include "world_performance_monitor.h"
#include "world_error.h"
#include "world_dsp_blocks.h"
#include <libetude/api.h>
#include <libetude/profiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// =============================================================================
// 내부 헬퍼 함수 선언
// =============================================================================

static ETResult world_pipeline_build_dsp_diagram(WorldPipeline* pipeline);
static ETResult world_pipeline_build_execution_graph(WorldPipeline* pipeline);
static ETResult world_pipeline_setup_profiling(WorldPipeline* pipeline);
static ETResult world_pipeline_validate_config(const WorldPipelineConfig* config);
static void world_pipeline_set_error(WorldPipeline* pipeline, ETResult error, const char* message);
static double world_pipeline_get_current_time(void);
static ETResult world_pipeline_create_default_blocks(WorldPipeline* pipeline);
static ETResult world_pipeline_connect_default_blocks(WorldPipeline* pipeline);

// =============================================================================
// 파이프라인 설정 관리
// =============================================================================

WorldPipelineConfig world_pipeline_config_default(void) {
    WorldPipelineConfig config = {0};

    // 오디오 설정 기본값
    config.sample_rate = 44100;
    config.frame_size = 1024;
    config.buffer_size = 4096;

    // WORLD 알고리즘 기본 설정
    config.f0_config.frame_period = 5.0;
    config.f0_config.f0_floor = 71.0;
    config.f0_config.f0_ceil = 800.0;
    config.f0_config.algorithm = 0; // DIO

    config.spectrum_config.q1 = -0.15;
    config.spectrum_config.fft_size = 2048;

    config.aperiodicity_config.threshold = 0.85;

    config.synthesis_config.sample_rate = 44100;
    config.synthesis_config.frame_period = 5.0;
    config.synthesis_config.enable_postfilter = true;

    // 그래프 최적화 기본 설정
    config.optimization.enable_node_fusion = true;
    config.optimization.enable_memory_reuse = true;
    config.optimization.enable_simd_optimization = true;
    config.optimization.enable_parallel_execution = true;
    config.optimization.max_thread_count = 0; // 자동

    // 메모리 관리 기본 설정
    config.memory_pool_size = 64 * 1024 * 1024; // 64MB
    config.enable_caching = true;

    // 성능 설정 기본값
    config.thread_count = 0; // 자동
    config.enable_profiling = false;
    config.enable_streaming = false;

    // 디버깅 설정 기본값
    config.enable_debug_output = false;
    strcpy(config.debug_output_dir, "./debug");

    return config;
}

bool world_pipeline_config_validate(const WorldPipelineConfig* config) {
    if (!config) return false;

    // 새로운 설정 시스템의 검증 함수 사용
    return world_pipeline_config_validate(config);

    return true;
}

ETResult world_pipeline_config_copy(const WorldPipelineConfig* src, WorldPipelineConfig* dst) {
    if (!src || !dst) return ET_ERROR_INVALID_PARAMETER;

    memcpy(dst, src, sizeof(WorldPipelineConfig));
    return ET_SUCCESS;
}

// =============================================================================
// 파이프라인 생성 및 관리
// =============================================================================

WorldPipeline* world_pipeline_create(const WorldPipelineConfig* config) {
    if (!config) {
        WorldPipelineConfig default_config = world_pipeline_config_default();
        config = &default_config;
    }

    // 설정 검증
    if (!world_pipeline_config_validate(config)) {
        return NULL;
    }

    // 파이프라인 구조체 할당
    WorldPipeline* pipeline = (WorldPipeline*)calloc(1, sizeof(WorldPipeline));
    if (!pipeline) {
        return NULL;
    }

    // 설정 복사
    if (world_pipeline_config_copy(config, &pipeline->config) != ET_SUCCESS) {
        free(pipeline);
        return NULL;
    }

    // 초기 상태 설정
    pipeline->state = WORLD_PIPELINE_STATE_UNINITIALIZED;
    pipeline->is_initialized = false;
    pipeline->is_running = false;
    pipeline->is_streaming_active = false;
    pipeline->stream_context = NULL;
    pipeline->perf_monitor = NULL;
    pipeline->debug_enabled = config->debug.enable_debug_output;
    pipeline->creation_time = world_pipeline_get_current_time();
    pipeline->last_error = ET_SUCCESS;

    // 메모리 풀 생성
    pipeline->mem_pool = et_memory_pool_create(config->memory.memory_pool_size);
    if (!pipeline->mem_pool) {
        world_pipeline_set_error(pipeline, ET_ERROR_MEMORY_ALLOCATION, "메모리 풀 생성 실패");
        world_pipeline_destroy(pipeline);
        return NULL;
    }

    // 프로파일러 설정
    if (config->performance.enable_profiling) {
        if (world_pipeline_setup_profiling(pipeline) != ET_SUCCESS) {
            world_pipeline_destroy(pipeline);
            return NULL;
        }
    }

    // 성능 모니터 설정
    if (config->performance.enable_profiling || config->performance.enable_timing_analysis) {
        WorldPerfMonitorConfig perf_config = world_perf_monitor_config_default();
        perf_config.enable_time_monitoring = config->performance.enable_timing_analysis;
        perf_config.enable_memory_monitoring = config->performance.enable_memory_profiling;
        perf_config.enable_cpu_monitoring = config->performance.enable_cpu_profiling;
        perf_config.enable_console_output = config->debug.enable_verbose_logging;
        perf_config.enable_file_output = config->debug.enable_debug_output;
        strcpy(perf_config.output_file_path, config->performance.profile_output_dir);

        pipeline->perf_monitor = world_perf_monitor_create(&perf_config);
        if (!pipeline->perf_monitor) {
            world_pipeline_destroy(pipeline);
            return NULL;
        }
    }

    // 디버그 로그 파일 열기
    if (pipeline->debug_enabled) {
        char log_filename[512];
        snprintf(log_filename, sizeof(log_filename), "%s/pipeline_debug.log", config->debug.debug_output_dir);
        pipeline->debug_log_file = fopen(log_filename, "w");
        if (pipeline->debug_log_file) {
            fprintf(pipeline->debug_log_file, "WORLD Pipeline Debug Log - Created at %.3f\n",
                   pipeline->creation_time);
            fflush(pipeline->debug_log_file);
        }
    }

    return pipeline;
}

void world_pipeline_destroy(WorldPipeline* pipeline) {
    if (!pipeline) return;

    // 실행 중인 경우 중지
    if (pipeline->is_running) {
        world_pipeline_stop(pipeline);
    }

    // 정리 작업
    world_pipeline_cleanup(pipeline);

    // 디버그 로그 파일 닫기
    if (pipeline->debug_log_file) {
        fprintf(pipeline->debug_log_file, "Pipeline destroyed at %.3f\n", world_pipeline_get_current_time());
        fclose(pipeline->debug_log_file);
    }

    // 컴포넌트 해제
    if (pipeline->context) {
        world_graph_context_destroy(pipeline->context);
    }

    if (pipeline->execution_graph) {
        et_graph_destroy(pipeline->execution_graph);
    }

    if (pipeline->graph_builder) {
        world_graph_builder_destroy(pipeline->graph_builder);
    }

    if (pipeline->block_diagram) {
        dsp_block_diagram_destroy(pipeline->block_diagram);
    }

    if (pipeline->profiler) {
        et_profiler_destroy(pipeline->profiler);
    }

    if (pipeline->perf_monitor) {
        world_perf_monitor_destroy(pipeline->perf_monitor);
    }

    if (pipeline->mem_pool) {
        et_memory_pool_destroy(pipeline->mem_pool);
    }

    free(pipeline);
}

ETResult world_pipeline_initialize(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    if (pipeline->is_initialized) {
        return ET_SUCCESS; // 이미 초기화됨
    }

    pipeline->state = WORLD_PIPELINE_STATE_INITIALIZED;

    // DSP 블록 다이어그램 구축
    ETResult result = world_pipeline_build_dsp_diagram(pipeline);
    if (result != ET_SUCCESS) {
        world_pipeline_set_error(pipeline, result, "DSP 블록 다이어그램 구축 실패");
        return result;
    }

    // 실행 그래프 구축
    result = world_pipeline_build_execution_graph(pipeline);
    if (result != ET_SUCCESS) {
        world_pipeline_set_error(pipeline, result, "실행 그래프 구축 실패");
        return result;
    }

    // 그래프 컨텍스트 생성 (기본 파라미터로)
    UTAUParameters default_utau_params = {0};
    pipeline->context = world_graph_context_create(&default_utau_params);
    if (!pipeline->context) {
        world_pipeline_set_error(pipeline, ET_ERROR_MEMORY_ALLOCATION, "그래프 컨텍스트 생성 실패");
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    // 성능 모니터 시작
    if (pipeline->perf_monitor) {
        world_perf_monitor_start(pipeline->perf_monitor);
    }

    pipeline->is_initialized = true;
    pipeline->state = WORLD_PIPELINE_STATE_READY;

    if (pipeline->debug_enabled && pipeline->debug_log_file) {
        fprintf(pipeline->debug_log_file, "Pipeline initialized successfully at %.3f\n",
               world_pipeline_get_current_time());
        fflush(pipeline->debug_log_file);
    }

    return ET_SUCCESS;
}

void world_pipeline_cleanup(WorldPipeline* pipeline) {
    if (!pipeline) return;

    // 스트리밍 중지
    if (pipeline->is_streaming_active && pipeline->stream_context) {
        world_stream_stop(pipeline->stream_context);
        pipeline->is_streaming_active = false;
        pipeline->stream_callback = NULL;
        pipeline->stream_user_data = NULL;
    }

    // 스트리밍 컨텍스트 해제
    if (pipeline->stream_context) {
        world_stream_context_destroy(pipeline->stream_context);
        pipeline->stream_context = NULL;
    }

    // 실행 중지
    if (pipeline->is_running) {
        pipeline->is_running = false;
    }

    // 컨텍스트 정리
    if (pipeline->context) {
        world_graph_context_reset(pipeline->context);
    }

    // 상태 초기화
    pipeline->state = WORLD_PIPELINE_STATE_UNINITIALIZED;
    pipeline->is_initialized = false;

    if (pipeline->debug_enabled && pipeline->debug_log_file) {
        fprintf(pipeline->debug_log_file, "Pipeline cleaned up at %.3f\n",
               world_pipeline_get_current_time());
        fflush(pipeline->debug_log_file);
    }
}

ETResult world_pipeline_reconfigure(WorldPipeline* pipeline, const WorldPipelineConfig* config) {
    if (!pipeline || !config) return ET_ERROR_INVALID_PARAMETER;

    // 설정 검증
    if (!world_pipeline_config_validate(config)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실행 중인 경우 중지
    bool was_running = pipeline->is_running;
    if (was_running) {
        ETResult result = world_pipeline_stop(pipeline);
        if (result != ET_SUCCESS) return result;
    }

    // 정리
    world_pipeline_cleanup(pipeline);

    // 새 설정 적용
    ETResult result = world_pipeline_config_copy(config, &pipeline->config);
    if (result != ET_SUCCESS) return result;

    // 재초기화
    result = world_pipeline_initialize(pipeline);
    if (result != ET_SUCCESS) return result;

    // 필요한 경우 재시작
    if (was_running) {
        // 이전 파라미터로 재시작하려면 별도 저장이 필요하지만
        // 여기서는 단순히 준비 상태로만 설정
        pipeline->state = WORLD_PIPELINE_STATE_READY;
    }

    return ET_SUCCESS;
}

// =============================================================================
// 파이프라인 실행
// =============================================================================

ETResult world_pipeline_process(WorldPipeline* pipeline,
                               const UTAUParameters* utau_params,
                               float* output_audio, int* output_length) {
    if (!pipeline || !utau_params || !output_audio || !output_length) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!pipeline->is_initialized) {
        ETResult result = world_pipeline_initialize(pipeline);
        if (result != ET_SUCCESS) return result;
    }

    if (pipeline->is_running) {
        return ET_ERROR_INVALID_STATE;
    }

    pipeline->is_running = true;
    pipeline->state = WORLD_PIPELINE_STATE_RUNNING;

    double start_time = world_pipeline_get_current_time();

    // 성능 측정 시작
    if (pipeline->perf_monitor) {
        world_perf_monitor_stage_begin(pipeline->perf_monitor, WORLD_PERF_STAGE_TOTAL);
        world_perf_monitor_stage_begin(pipeline->perf_monitor, WORLD_PERF_STAGE_PARAMETER_PARSING);
    }

    // UTAU 파라미터 설정
    ETResult result = world_graph_context_set_utau_parameters(pipeline->context, utau_params);
    if (result != ET_SUCCESS) {
        world_pipeline_set_error(pipeline, result, "UTAU 파라미터 설정 실패");
        pipeline->is_running = false;
        pipeline->state = WORLD_PIPELINE_STATE_ERROR;
        if (pipeline->perf_monitor) {
            world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_PARAMETER_PARSING);
            world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_TOTAL);
        }
        return result;
    }

    // 파라미터 파싱 완료
    if (pipeline->perf_monitor) {
        world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_PARAMETER_PARSING);
    }

    // 진행 상황 콜백 설정
    if (pipeline->progress_callback) {
        world_graph_context_set_progress_callback(pipeline->context,
                                                 pipeline->progress_callback,
                                                 pipeline->callback_user_data);
    }

    // 그래프 실행 시작
    if (pipeline->perf_monitor) {
        world_perf_monitor_stage_begin(pipeline->perf_monitor, WORLD_PERF_STAGE_SYNTHESIS);
    }

    result = world_graph_execute(pipeline->execution_graph, pipeline->context);
    if (result != ET_SUCCESS) {
        world_pipeline_set_error(pipeline, result, "그래프 실행 실패");
        pipeline->is_running = false;
        pipeline->state = WORLD_PIPELINE_STATE_ERROR;
        if (pipeline->perf_monitor) {
            world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_SYNTHESIS);
            world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_TOTAL);
        }
        return result;
    }

    // 그래프 실행 완료
    if (pipeline->perf_monitor) {
        world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_SYNTHESIS);
    }

    // 결과 추출 (실제 구현에서는 그래프 컨텍스트에서 결과를 가져와야 함)
    // 여기서는 임시로 기본값 설정
    *output_length = pipeline->config.audio.buffer_size;
    memset(output_audio, 0, *output_length * sizeof(float));

    pipeline->last_execution_time = world_pipeline_get_current_time() - start_time;

    // 성능 측정 완료
    if (pipeline->perf_monitor) {
        world_perf_monitor_stage_end(pipeline->perf_monitor, WORLD_PERF_STAGE_TOTAL);

        // 처리량 기록
        uint64_t samples_processed = *output_length;
        world_perf_monitor_record_throughput(pipeline->perf_monitor,
                                           WORLD_PERF_STAGE_TOTAL,
                                           samples_processed,
                                           pipeline->last_execution_time);
    }

    pipeline->is_running = false;
    pipeline->state = WORLD_PIPELINE_STATE_COMPLETED;

    if (pipeline->debug_enabled && pipeline->debug_log_file) {
        fprintf(pipeline->debug_log_file, "Pipeline processing completed in %.3f seconds\n",
               pipeline->last_execution_time);
        fflush(pipeline->debug_log_file);
    }

    return ET_SUCCESS;
}

ETResult world_pipeline_process_async(WorldPipeline* pipeline,
                                     const UTAUParameters* utau_params,
                                     WorldPipelineCompletionCallback completion_callback,
                                     void* user_data) {
    if (!pipeline || !utau_params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 완료 콜백 설정
    pipeline->completion_callback = completion_callback;
    pipeline->callback_user_data = user_data;

    // 비동기 실행을 위해 별도 스레드에서 실행해야 하지만
    // 여기서는 단순화하여 동기 실행 후 콜백 호출
    int output_length = pipeline->config.audio.buffer_size;
    float* output_audio = (float*)malloc(output_length * sizeof(float));
    if (!output_audio) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    ETResult result = world_pipeline_process(pipeline, utau_params, output_audio, &output_length);

    // 완료 콜백 호출
    if (completion_callback) {
        const char* message = (result == ET_SUCCESS) ? "처리 완료" : pipeline->error_message;
        completion_callback(user_data, result, message);
    }

    free(output_audio);
    return result;
}

ETResult world_pipeline_process_streaming(WorldPipeline* pipeline,
                                         const UTAUParameters* utau_params,
                                         AudioStreamCallback stream_callback,
                                         void* user_data) {
    if (!pipeline || !utau_params || !stream_callback) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!pipeline->is_initialized) {
        ETResult result = world_pipeline_initialize(pipeline);
        if (result != ET_SUCCESS) return result;
    }

    // 스트리밍 컨텍스트 생성 (아직 없는 경우)
    if (!pipeline->stream_context) {
        WorldStreamConfig stream_config = world_stream_config_default();

        // 파이프라인 설정에서 스트리밍 설정 추출
        stream_config.chunk_size = pipeline->config.audio.frame_size;
        stream_config.sample_rate = pipeline->config.audio.sample_rate;
        stream_config.channel_count = pipeline->config.audio.channel_count;
        stream_config.target_latency_ms = 10.0; // 기본 10ms 지연
        stream_config.max_latency_ms = 50.0;    // 최대 50ms 지연
        stream_config.processing_thread_count = pipeline->config.optimization.max_thread_count > 0 ?
                                               pipeline->config.optimization.max_thread_count : 2;

        pipeline->stream_context = world_stream_context_create(&stream_config);
        if (!pipeline->stream_context) {
            return ET_ERROR_MEMORY_ALLOCATION;
        }
    }

    // 스트리밍 콜백 설정
    pipeline->stream_callback = stream_callback;
    pipeline->stream_user_data = user_data;

    // 오디오 콜백을 스트리밍 컨텍스트에 설정
    world_stream_set_audio_callback(pipeline->stream_context,
                                   (WorldStreamAudioCallback)stream_callback,
                                   user_data);

    // UTAU 파라미터 설정
    ETResult result = world_graph_context_set_utau_parameters(pipeline->context, utau_params);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 스트리밍 시작
    result = world_stream_start(pipeline->stream_context);
    if (result != ET_SUCCESS) {
        return result;
    }

    pipeline->is_streaming_active = true;

    // 실제 오디오 처리 및 스트리밍 (간단한 예시)
    // 실제 구현에서는 WORLD 처리 결과를 실시간으로 스트리밍
    float* audio_buffer = (float*)malloc(pipeline->config.audio.buffer_size * sizeof(float));
    if (!audio_buffer) {
        world_stream_stop(pipeline->stream_context);
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    // 임시 오디오 데이터 생성 및 스트리밍
    memset(audio_buffer, 0, pipeline->config.audio.buffer_size * sizeof(float));

    result = world_stream_push_audio(pipeline->stream_context,
                                    audio_buffer,
                                    pipeline->config.audio.buffer_size);

    free(audio_buffer);

    if (result != ET_SUCCESS) {
        world_stream_stop(pipeline->stream_context);
        pipeline->is_streaming_active = false;
        return result;
    }

    return ET_SUCCESS;
}

// =============================================================================
// 내부 헬퍼 함수 구현
// =============================================================================

static ETResult world_pipeline_build_dsp_diagram(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    // DSP 블록 다이어그램 생성
    pipeline->block_diagram = dsp_block_diagram_create("WORLD_Pipeline", 16, 32, pipeline->mem_pool);
    if (!pipeline->block_diagram) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    // 기본 블록들 생성
    ETResult result = world_pipeline_create_default_blocks(pipeline);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 블록들 연결
    result = world_pipeline_connect_default_blocks(pipeline);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 다이어그램 빌드
    result = dsp_block_diagram_build(pipeline->block_diagram);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

static ETResult world_pipeline_build_execution_graph(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->block_diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 그래프 빌더 생성
    WorldGraphBuilderConfig builder_config = {0};
    builder_config.max_nodes = 16;
    builder_config.max_connections = 32;
    builder_config.memory_pool_size = pipeline->config.memory.memory_pool_size / 4; // 일부 할당
    builder_config.enable_optimization = true;
    builder_config.enable_validation = true;

    pipeline->graph_builder = world_graph_builder_create(&builder_config);
    if (!pipeline->graph_builder) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    // DSP 다이어그램을 그래프로 변환
    ETResult result = world_graph_builder_convert_from_diagram(pipeline->graph_builder,
                                                              pipeline->block_diagram);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 그래프 최적화
    if (pipeline->config.optimization.enable_node_fusion ||
        pipeline->config.optimization.enable_memory_reuse ||
        pipeline->config.optimization.enable_simd_optimization) {
        result = world_graph_builder_optimize(pipeline->graph_builder);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 그래프 빌드
    pipeline->execution_graph = world_graph_builder_build(pipeline->graph_builder);
    if (!pipeline->execution_graph) {
        return ET_ERROR_GRAPH_BUILD_FAILED;
    }

    return ET_SUCCESS;
}

static ETResult world_pipeline_create_default_blocks(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->block_diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 오디오 입력 블록
    AudioIOConfig audio_input_config = {0};
    audio_input_config.sample_rate = pipeline->config.audio.sample_rate;
    audio_input_config.frame_size = pipeline->config.audio.frame_size;
    audio_input_config.is_input = true;

    DSPBlock* audio_input_block = create_world_audio_io_block(&audio_input_config);
    if (!audio_input_block) return ET_ERROR_MEMORY_ALLOCATION;

    ETResult result = dsp_block_diagram_add_block(pipeline->block_diagram, audio_input_block);
    if (result != ET_SUCCESS) return result;

    // F0 추출 블록
    DSPBlock* f0_block = create_world_f0_extraction_block(&pipeline->config.f0);
    if (!f0_block) return ET_ERROR_MEMORY_ALLOCATION;

    result = dsp_block_diagram_add_block(pipeline->block_diagram, f0_block);
    if (result != ET_SUCCESS) return result;

    // 스펙트럼 분석 블록
    DSPBlock* spectrum_block = create_world_spectrum_analysis_block(&pipeline->config.spectrum);
    if (!spectrum_block) return ET_ERROR_MEMORY_ALLOCATION;

    result = dsp_block_diagram_add_block(pipeline->block_diagram, spectrum_block);
    if (result != ET_SUCCESS) return result;

    // 비주기성 분석 블록
    DSPBlock* aperiodicity_block = create_world_aperiodicity_analysis_block(&pipeline->config.aperiodicity);
    if (!aperiodicity_block) return ET_ERROR_MEMORY_ALLOCATION;

    result = dsp_block_diagram_add_block(pipeline->block_diagram, aperiodicity_block);
    if (result != ET_SUCCESS) return result;

    // 합성 블록
    DSPBlock* synthesis_block = create_world_synthesis_block(&pipeline->config.synthesis);
    if (!synthesis_block) return ET_ERROR_MEMORY_ALLOCATION;

    result = dsp_block_diagram_add_block(pipeline->block_diagram, synthesis_block);
    if (result != ET_SUCCESS) return result;

    // 오디오 출력 블록
    AudioIOConfig audio_output_config = {0};
    audio_output_config.sample_rate = pipeline->config.audio.sample_rate;
    audio_output_config.frame_size = pipeline->config.audio.frame_size;
    audio_output_config.is_input = false;

    DSPBlock* audio_output_block = create_world_audio_io_block(&audio_output_config);
    if (!audio_output_block) return ET_ERROR_MEMORY_ALLOCATION;

    result = dsp_block_diagram_add_block(pipeline->block_diagram, audio_output_block);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

static ETResult world_pipeline_connect_default_blocks(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->block_diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 블록 ID는 추가 순서에 따라 0부터 시작
    // 0: audio_input, 1: f0_extraction, 2: spectrum_analysis,
    // 3: aperiodicity_analysis, 4: synthesis, 5: audio_output

    ETResult result;

    // 오디오 입력 -> F0 추출
    result = dsp_block_diagram_connect(pipeline->block_diagram, 0, 0, 1, 0);
    if (result != ET_SUCCESS) return result;

    // 오디오 입력 -> 스펙트럼 분석
    result = dsp_block_diagram_connect(pipeline->block_diagram, 0, 0, 2, 0);
    if (result != ET_SUCCESS) return result;

    // 오디오 입력 -> 비주기성 분석
    result = dsp_block_diagram_connect(pipeline->block_diagram, 0, 0, 3, 0);
    if (result != ET_SUCCESS) return result;

    // F0 추출 -> 합성
    result = dsp_block_diagram_connect(pipeline->block_diagram, 1, 0, 4, 0);
    if (result != ET_SUCCESS) return result;

    // 스펙트럼 분석 -> 합성
    result = dsp_block_diagram_connect(pipeline->block_diagram, 2, 0, 4, 1);
    if (result != ET_SUCCESS) return result;

    // 비주기성 분석 -> 합성
    result = dsp_block_diagram_connect(pipeline->block_diagram, 3, 0, 4, 2);
    if (result != ET_SUCCESS) return result;

    // 합성 -> 오디오 출력
    result = dsp_block_diagram_connect(pipeline->block_diagram, 4, 0, 5, 0);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

static ETResult world_pipeline_setup_profiling(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    pipeline->profiler = et_profiler_create();
    if (!pipeline->profiler) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    // 프로파일링 카테고리 설정
    et_profiler_add_category(pipeline->profiler, "pipeline_total");
    et_profiler_add_category(pipeline->profiler, "dsp_processing");
    et_profiler_add_category(pipeline->profiler, "graph_execution");
    et_profiler_add_category(pipeline->profiler, "memory_allocation");

    return ET_SUCCESS;
}

static ETResult world_pipeline_validate_config(const WorldPipelineConfig* config) {
    return world_pipeline_config_validate(config) ? ET_SUCCESS : ET_ERROR_INVALID_PARAMETER;
}

static void world_pipeline_set_error(WorldPipeline* pipeline, ETResult error, const char* message) {
    if (!pipeline) return;

    pipeline->last_error = error;
    if (message) {
        strncpy(pipeline->error_message, message, sizeof(pipeline->error_message) - 1);
        pipeline->error_message[sizeof(pipeline->error_message) - 1] = '\0';
    } else {
        pipeline->error_message[0] = '\0';
    }

    if (pipeline->debug_enabled && pipeline->debug_log_file) {
        fprintf(pipeline->debug_log_file, "ERROR: %d - %s\n", error, message ? message : "Unknown error");
        fflush(pipeline->debug_log_file);
    }
}

static double world_pipeline_get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// =============================================================================
// 상태 조회 함수들
// =============================================================================

WorldPipelineState world_pipeline_get_state(WorldPipeline* pipeline) {
    return pipeline ? pipeline->state : WORLD_PIPELINE_STATE_ERROR;
}

bool world_pipeline_is_running(WorldPipeline* pipeline) {
    return pipeline ? pipeline->is_running : false;
}

bool world_pipeline_is_completed(WorldPipeline* pipeline) {
    return pipeline ? (pipeline->state == WORLD_PIPELINE_STATE_COMPLETED) : false;
}

float world_pipeline_get_progress(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->context) return 0.0f;
    return world_graph_context_get_progress(pipeline->context);
}

// =============================================================================
// 파이프라인 제어 함수들
// =============================================================================

ETResult world_pipeline_pause(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    if (!pipeline->is_running) return ET_ERROR_INVALID_STATE;

    if (pipeline->context) {
        ETResult result = world_graph_context_pause(pipeline->context);
        if (result == ET_SUCCESS) {
            pipeline->state = WORLD_PIPELINE_STATE_PAUSED;
        }
        return result;
    }

    return ET_ERROR_INVALID_STATE;
}

ETResult world_pipeline_resume(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    if (pipeline->state != WORLD_PIPELINE_STATE_PAUSED) return ET_ERROR_INVALID_STATE;

    if (pipeline->context) {
        ETResult result = world_graph_context_resume(pipeline->context);
        if (result == ET_SUCCESS) {
            pipeline->state = WORLD_PIPELINE_STATE_RUNNING;
        }
        return result;
    }

    return ET_ERROR_INVALID_STATE;
}

ETResult world_pipeline_stop(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    pipeline->is_running = false;
    pipeline->is_streaming_active = false;

    if (pipeline->context) {
        world_graph_context_stop(pipeline->context);
    }

    pipeline->state = WORLD_PIPELINE_STATE_READY;
    return ET_SUCCESS;
}

ETResult world_pipeline_restart(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    ETResult result = world_pipeline_stop(pipeline);
    if (result != ET_SUCCESS) return result;

    world_pipeline_cleanup(pipeline);

    return world_pipeline_initialize(pipeline);
}

// =============================================================================
// 성능 모니터링 및 오류 처리
// =============================================================================

const WorldGraphStats* world_pipeline_get_stats(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->context) return NULL;
    return world_graph_context_get_stats(pipeline->context);
}

/**
 * @brief 파이프라인 성능 통계 조회
 *
 * @param pipeline 파이프라인
 * @return const WorldPipelinePerformance* 성능 통계 (NULL 시 실패)
 */
const WorldPipelinePerformance* world_pipeline_get_performance_stats(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->perf_monitor) return NULL;
    return world_perf_monitor_get_performance(pipeline->perf_monitor);
}

double world_pipeline_get_execution_time(WorldPipeline* pipeline) {
    return pipeline ? pipeline->last_execution_time : 0.0;
}

size_t world_pipeline_get_memory_usage(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->context) return 0;
    return world_graph_context_get_memory_usage(pipeline->context);
}

ETResult world_pipeline_reset_stats(WorldPipeline* pipeline) {
    if (!pipeline || !pipeline->context) return ET_ERROR_INVALID_PARAMETER;
    return world_graph_context_reset_stats(pipeline->context);
}

ETResult world_pipeline_get_last_error(WorldPipeline* pipeline) {
    return pipeline ? pipeline->last_error : ET_ERROR_INVALID_PARAMETER;
}

const char* world_pipeline_get_error_message(WorldPipeline* pipeline) {
    return pipeline ? pipeline->error_message : "Invalid pipeline";
}

ETResult world_pipeline_clear_error(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    pipeline->last_error = ET_SUCCESS;
    pipeline->error_message[0] = '\0';
    return ET_SUCCESS;
}

// =============================================================================
// 콜백 설정 함수들
// =============================================================================

ETResult world_pipeline_set_progress_callback(WorldPipeline* pipeline,
                                             WorldPipelineProgressCallback callback,
                                             void* user_data) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    pipeline->progress_callback = callback;
    pipeline->callback_user_data = user_data;
    return ET_SUCCESS;
}

ETResult world_pipeline_set_completion_callback(WorldPipeline* pipeline,
                                               WorldPipelineCompletionCallback callback,
                                               void* user_data) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    pipeline->completion_callback = callback;
    pipeline->callback_user_data = user_data;
    return ET_SUCCESS;
}

// =============================================================================
// 디버깅 및 유틸리티 함수들
// =============================================================================

ETResult world_pipeline_dump_state(WorldPipeline* pipeline, const char* filename) {
    if (!pipeline || !filename) return ET_ERROR_INVALID_PARAMETER;

    FILE* file = fopen(filename, "w");
    if (!file) return ET_ERROR_FILE_IO;

    fprintf(file, "WORLD Pipeline State Dump\n");
    fprintf(file, "========================\n\n");
    fprintf(file, "State: %d\n", pipeline->state);
    fprintf(file, "Initialized: %s\n", pipeline->is_initialized ? "Yes" : "No");
    fprintf(file, "Running: %s\n", pipeline->is_running ? "Yes" : "No");
    fprintf(file, "Streaming: %s\n", pipeline->is_streaming_active ? "Yes" : "No");
    fprintf(file, "Last Error: %d\n", pipeline->last_error);
    fprintf(file, "Error Message: %s\n", pipeline->error_message);
    fprintf(file, "Creation Time: %.3f\n", pipeline->creation_time);
    fprintf(file, "Last Execution Time: %.3f\n", pipeline->last_execution_time);

    // 설정 정보
    fprintf(file, "\nConfiguration:\n");
    fprintf(file, "Sample Rate: %d\n", pipeline->config.audio.sample_rate);
    fprintf(file, "Frame Size: %d\n", pipeline->config.audio.frame_size);
    fprintf(file, "Buffer Size: %d\n", pipeline->config.audio.buffer_size);
    fprintf(file, "Memory Pool Size: %zu\n", pipeline->config.memory.memory_pool_size);
    fprintf(file, "Thread Count: %d\n", pipeline->config.optimization.max_thread_count);

    fclose(file);
    return ET_SUCCESS;
}

ETResult world_pipeline_export_dot(WorldPipeline* pipeline, const char* filename) {
    if (!pipeline || !filename) return ET_ERROR_INVALID_PARAMETER;

    if (pipeline->block_diagram) {
        return dsp_block_diagram_export_dot(pipeline->block_diagram, filename);
    }

    return ET_ERROR_INVALID_STATE;
}

ETResult world_pipeline_validate(WorldPipeline* pipeline) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    // 설정 검증
    if (!world_pipeline_config_validate(&pipeline->config)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 컴포넌트 검증
    if (pipeline->is_initialized) {
        if (!pipeline->block_diagram || !pipeline->execution_graph || !pipeline->context) {
            return ET_ERROR_INVALID_STATE;
        }

        // 블록 다이어그램 검증
        if (!dsp_block_diagram_validate(pipeline->block_diagram)) {
            return ET_ERROR_INVALID_STATE;
        }

        // 그래프 빌더 검증
        if (pipeline->graph_builder) {
            ETResult result = world_graph_builder_validate(pipeline->graph_builder);
            if (result != ET_SUCCESS) return result;
        }
    }

    return ET_SUCCESS;
}

void world_pipeline_print_info(WorldPipeline* pipeline) {
    if (!pipeline) {
        printf("Pipeline: NULL\n");
        return;
    }

    printf("WORLD Pipeline Information\n");
    printf("=========================\n");
    printf("State: %d\n", pipeline->state);
    printf("Initialized: %s\n", pipeline->is_initialized ? "Yes" : "No");
    printf("Running: %s\n", pipeline->is_running ? "Yes" : "No");
    printf("Sample Rate: %d Hz\n", pipeline->config.audio.sample_rate);
    printf("Frame Size: %d samples\n", pipeline->config.audio.frame_size);
    printf("Thread Count: %d\n", pipeline->config.optimization.max_thread_count);
    printf("Memory Pool Size: %.2f MB\n", pipeline->config.memory.memory_pool_size / (1024.0 * 1024.0));
    printf("Profiling: %s\n", pipeline->config.performance.enable_profiling ? "Enabled" : "Disabled");
    printf("Caching: %s\n", pipeline->config.memory.enable_memory_tracking ? "Enabled" : "Disabled");

    if (pipeline->last_error != ET_SUCCESS) {
        printf("Last Error: %d - %s\n", pipeline->last_error, pipeline->error_message);
    }

    if (pipeline->last_execution_time > 0.0) {
        printf("Last Execution Time: %.3f seconds\n", pipeline->last_execution_time);
    }
}

ETResult world_pipeline_wait_for_completion(WorldPipeline* pipeline, double timeout_seconds) {
    if (!pipeline) return ET_ERROR_INVALID_PARAMETER;

    if (!pipeline->is_running) return ET_SUCCESS;

    if (pipeline->context) {
        return world_graph_context_wait_for_completion(pipeline->context);
    }

    return ET_ERROR_INVALID_STATE;
}