# LibEtude 테스트 CMake 설정

# Unity 테스트 프레임워크 추가
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(unity)

# C 표준 설정
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Unity 테스트 러너 생성 함수
function(create_unity_test test_name test_sources)
    add_executable(${test_name} ${test_sources})
    target_link_libraries(${test_name}
        unity
        libetude_static
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${unity_SOURCE_DIR}/src
    )
    # Unity 설정
    target_compile_definitions(${test_name} PRIVATE
        UNITY_INCLUDE_DOUBLE
        UNITY_INCLUDE_FLOAT
    )
    # pthread 라이브러리 링크 (Unix 계열)
    if(NOT WIN32)
        target_link_libraries(${test_name} pthread m)
    endif()
endfunction()

# 개별 텐서 테스트
create_unity_test(tensor_test "unit/test_tensor.c")

# API 테스트
create_unity_test(api_test "unit/test_api.c")

# 독립적인 테스트들 (libetude 의존성 없음)

# Unity 기본 테스트
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../test_unity_simple.c)
    add_executable(test_unity_simple ../test_unity_simple.c)
    target_link_libraries(test_unity_simple unity)
    target_include_directories(test_unity_simple PRIVATE
        ${unity_SOURCE_DIR}/src
    )
    target_compile_definitions(test_unity_simple PRIVATE
        UNITY_INCLUDE_DOUBLE
        UNITY_INCLUDE_FLOAT
    )
endif()

# 수학 함수 테스트
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../test_math_simple.c)
    add_executable(test_math_simple ../test_math_simple.c)
    target_link_libraries(test_math_simple unity)
    target_include_directories(test_math_simple PRIVATE
        ${unity_SOURCE_DIR}/src
    )
    target_compile_definitions(test_math_simple PRIVATE
        UNITY_INCLUDE_DOUBLE
        UNITY_INCLUDE_FLOAT
    )

    # 윈도우에서 수학 라이브러리 링크
    if(NOT WIN32)
        target_link_libraries(test_math_simple m)
    endif()
endif()

# 고속 수학 함수 테스트
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../test_fast_math_minimal.c)
    add_executable(test_fast_math_minimal ../test_fast_math_minimal.c)
    target_link_libraries(test_fast_math_minimal unity)
    target_include_directories(test_fast_math_minimal PRIVATE
        ${unity_SOURCE_DIR}/src
    )
    target_compile_definitions(test_fast_math_minimal PRIVATE
        UNITY_INCLUDE_DOUBLE
        UNITY_INCLUDE_FLOAT
    )

    # 윈도우에서 수학 라이브러리 링크
    if(NOT WIN32)
        target_link_libraries(test_fast_math_minimal m)
    endif()
endif()

# SIMD 벡터화된 수학 함수 테스트
add_executable(test_simd_math_functions unit/test_simd_math_functions.c)
target_link_libraries(test_simd_math_functions
    unity
    libetude_static
)
target_include_directories(test_simd_math_functions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(test_simd_math_functions PRIVATE
    UNITY_INCLUDE_DOUBLE
    UNITY_INCLUDE_FLOAT
)

# 윈도우에서 수학 라이브러리 링크
if(NOT WIN32)
    target_link_libraries(test_simd_math_functions m pthread)
    target_link_libraries(tensor_test m pthread)
endif()

# 레거시 테스트 (호환성을 위해 유지)
add_executable(simple_unity_test simple_unity_test.c)
target_link_libraries(simple_unity_test unity)
target_include_directories(simple_unity_test PRIVATE
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(simple_unity_test PRIVATE
    UNITY_INCLUDE_DOUBLE
    UNITY_INCLUDE_FLOAT
)

add_executable(math_simple_test unit/test_math_simple.c)
target_link_libraries(math_simple_test unity)
target_include_directories(math_simple_test PRIVATE
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(math_simple_test PRIVATE
    UNITY_INCLUDE_DOUBLE
    UNITY_INCLUDE_FLOAT
)

if(NOT WIN32)
    target_link_libraries(math_simple_test m)
endif()

add_executable(fast_math_minimal_test unit/test_fast_math_minimal.c)
target_link_libraries(fast_math_minimal_test unity)
target_include_directories(fast_math_minimal_test PRIVATE
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(fast_math_minimal_test PRIVATE
    UNITY_INCLUDE_DOUBLE
    UNITY_INCLUDE_FLOAT
)

if(NOT WIN32)
    target_link_libraries(fast_math_minimal_test m)
endif()

# 개별 테스트 실행 파일들 (독립적으로 실행 가능)
create_unity_test(fast_math_test "unit/test_fast_math.c")
create_unity_test(simd_kernels_test "unit/test_simd_kernels.c")
create_unity_test(kernels_test "unit/test_kernels.c")
create_unity_test(memory_test "unit/test_memory.c")
create_unity_test(memory_optimization_test "unit/test_memory_optimization.c")
create_unity_test(memory_allocator_test "unit/test_memory_allocator.c")
create_unity_test(hardware_test "unit/test_hardware.c")
create_unity_test(kernel_registry_test "unit/test_kernel_registry.c")
create_unity_test(quantization_test "unit/test_quantization.c")
create_unity_test(graph_test "unit/test_graph.c")
create_unity_test(lef_format_test "unit/test_lef_format.c")
create_unity_test(lefx_format_test "unit/test_lefx_format.c")
create_unity_test(model_loader_test "unit/test_model_loader.c")
create_unity_test(compression_test "unit/test_compression.c")
create_unity_test(diff_model_test "unit/test_diff_model.c")
create_unity_test(lefx_loader_test "unit/test_lefx_loader.c")
create_unity_test(conditional_activation_test "unit/test_conditional_activation.c")
create_unity_test(task_scheduler_test "unit/test_task_scheduler.c")
create_unity_test(profiler_test "unit/test_profiler.c")
create_unity_test(error_handling_test "unit/test_error_handling.c")

# LEF 포맷 포괄적 테스트 (15.3 작업)
create_unity_test(lef_format_comprehensive_test "unit/test_lef_format_comprehensive.c")
create_unity_test(lefx_extension_comprehensive_test "unit/test_lefx_extension_comprehensive.c")

# 오디오 처리 테스트
create_unity_test(audio_io_test "unit/test_audio_io.c")
create_unity_test(stft_test "unit/test_stft.c")
create_unity_test(mel_scale_test "unit/test_mel_scale.c")
create_unity_test(vocoder_test "unit/test_vocoder.c")

# 데스크톱 최적화 테스트
create_unity_test(desktop_optimization_test "unit/test_desktop_optimization.c")

# 모바일 최적화 테스트
create_unity_test(mobile_optimization_test "unit/test_mobile_optimization.c")

# 임베디드 최적화 테스트
create_unity_test(embedded_optimization_test "unit/test_embedded_optimization.c")

# 플러그인 시스템 테스트
create_unity_test(plugin_test "unit/test_plugin.c")

# 플러그인 의존성 관리 테스트
create_unity_test(plugin_dependency_test "unit/test_plugin_dependency.c")

# 오디오 효과 플러그인 테스트
create_unity_test(audio_effects_test "unit/test_audio_effects.c")

# UTAU 파라미터 파싱 테스트 (world4utau 예제용)
# 이 테스트는 world4utau 예제 소스를 직접 포함합니다
add_executable(utau_parameter_parsing_test
    unit/test_utau_parameter_parsing.c
    ../examples/world4utau/src/utau_interface.c
)
target_link_libraries(utau_parameter_parsing_test
    libetude_static
)
target_include_directories(utau_parameter_parsing_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/world4utau/include
)
# pthread 라이브러리 링크 (Unix 계열)
if(NOT WIN32)
    target_link_libraries(utau_parameter_parsing_test pthread m)
endif()

# F0 추출 테스트 (world4utau 예제용)
# 이 테스트는 world4utau 예제 소스를 직접 포함합니다
add_executable(f0_extraction_test
    unit/test_f0_extraction.c
    ../examples/world4utau/src/world_engine.c
    ../examples/world4utau/src/utau_interface.c
)
target_link_libraries(f0_extraction_test
    libetude_static
)
target_include_directories(f0_extraction_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/world4utau/include
)
# pthread 라이브러리 링크 (Unix 계열)
if(NOT WIN32)
    target_link_libraries(f0_extraction_test pthread m)
endif()

# 통합 테스트 (16번 작업)
create_unity_test(integration_pipeline_test "integration/test_pipeline.c")
create_unity_test(integration_streaming_test "integration/test_streaming.c")
create_unity_test(integration_performance_test "integration/test_performance.c")
create_unity_test(integration_platform_test "integration/test_platform.c")

# Unity CTest 통합
include(CTest)

# 테스트 등록
add_test(NAME TensorTest COMMAND tensor_test)
add_test(NAME SIMDKernelsTest COMMAND simd_kernels_test)
add_test(NAME KernelsTest COMMAND kernels_test)
add_test(NAME MemoryTest COMMAND memory_test)
add_test(NAME MemoryOptimizationTest COMMAND memory_optimization_test)
add_test(NAME MemoryAllocatorTest COMMAND memory_allocator_test)
add_test(NAME HardwareTest COMMAND hardware_test)
add_test(NAME KernelRegistryTest COMMAND kernel_registry_test)
add_test(NAME QuantizationTest COMMAND quantization_test)
add_test(NAME GraphTest COMMAND graph_test)
add_test(NAME LEFFormatTest COMMAND lef_format_test)
add_test(NAME LEFXFormatTest COMMAND lefx_format_test)
add_test(NAME ModelLoaderTest COMMAND model_loader_test)
add_test(NAME CompressionTest COMMAND compression_test)
add_test(NAME DiffModelTest COMMAND diff_model_test)
add_test(NAME LEFXLoaderTest COMMAND lefx_loader_test)
add_test(NAME ConditionalActivationTest COMMAND conditional_activation_test)
add_test(NAME TaskSchedulerTest COMMAND task_scheduler_test)
add_test(NAME ProfilerTest COMMAND profiler_test)
add_test(NAME ErrorHandlingTest COMMAND error_handling_test)

# LEF 포맷 포괄적 테스트 (15.3 작업)
add_test(NAME LEFFormatComprehensiveTest COMMAND lef_format_comprehensive_test)
add_test(NAME LEFXExtensionComprehensiveTest COMMAND lefx_extension_comprehensive_test)

# 오디오 처리 테스트
add_test(NAME AudioIOTest COMMAND audio_io_test)
add_test(NAME STFTTest COMMAND stft_test)
add_test(NAME MelScaleTest COMMAND mel_scale_test)
add_test(NAME VocoderTest COMMAND vocoder_test)

# 데스크톱 최적화 테스트
add_test(NAME DesktopOptimizationTest COMMAND desktop_optimization_test)

# 모바일 최적화 테스트
add_test(NAME MobileOptimizationTest COMMAND mobile_optimization_test)

# 임베디드 최적화 테스트
add_test(NAME EmbeddedOptimizationTest COMMAND embedded_optimization_test)

# 플러그인 시스템 테스트
add_test(NAME PluginTest COMMAND plugin_test)

# 플러그인 의존성 관리 테스트
add_test(NAME PluginDependencyTest COMMAND plugin_dependency_test)

# 오디오 효과 플러그인 테스트
add_test(NAME AudioEffectsTest COMMAND audio_effects_test)

# UTAU 파라미터 파싱 테스트
add_test(NAME UTAUParameterParsingTest COMMAND utau_parameter_parsing_test)

# F0 추출 테스트
add_test(NAME F0ExtractionTest COMMAND f0_extraction_test)

# 통합 테스트 등록 (16번 작업)
add_test(NAME IntegrationPipelineTest COMMAND integration_pipeline_test)
add_test(NAME IntegrationStreamingTest COMMAND integration_streaming_test)
add_test(NAME IntegrationPerformanceTest COMMAND integration_performance_test)
add_test(NAME IntegrationPlatformTest COMMAND integration_platform_test)

# 테스트 등록 (새로운 독립적인 테스트들)
if(TARGET test_unity_simple)
    add_test(NAME UnityBasicTest COMMAND test_unity_simple)
endif()
if(TARGET test_math_simple)
    add_test(NAME MathFunctionsTest COMMAND test_math_simple)
endif()
if(TARGET test_fast_math_minimal)
    add_test(NAME FastMathTest COMMAND test_fast_math_minimal)
endif()
add_test(NAME SIMDMathFunctionsTest COMMAND test_simd_math_functions)

# 레거시 테스트 등록 (호환성을 위해 유지)
add_test(NAME SimpleUnityTest COMMAND simple_unity_test)
add_test(NAME MathSimpleTest COMMAND math_simple_test)
add_test(NAME FastMathMinimalTest COMMAND fast_math_minimal_test)

# 새로운 테스트 속성 설정
set_tests_properties(TensorTest PROPERTIES
    TIMEOUT 60
    LABELS "tensor;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(GraphTest PROPERTIES
    TIMEOUT 60
    LABELS "graph;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LEFFormatTest PROPERTIES
    TIMEOUT 60
    LABELS "lef;format;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LEFXFormatTest PROPERTIES
    TIMEOUT 60
    LABELS "lefx;format;extension;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# LEF 포맷 포괄적 테스트 속성 설정 (15.3 작업)
set_tests_properties(LEFFormatComprehensiveTest PROPERTIES
    TIMEOUT 300
    LABELS "lef;format;comprehensive;quantization;compression;streaming;mmap;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LEFXExtensionComprehensiveTest PROPERTIES
    TIMEOUT 300
    LABELS "lefx;extension;comprehensive;speaker;language;effect;differential;plugin;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(ModelLoaderTest PROPERTIES
    TIMEOUT 60
    LABELS "lef;loader;model;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(CompressionTest PROPERTIES
    TIMEOUT 120
    LABELS "compression;lef;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(DiffModelTest PROPERTIES
    TIMEOUT 120
    LABELS "diff;model;lefx;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LEFXLoaderTest PROPERTIES
    TIMEOUT 120
    LABELS "lefx;loader;extension;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(ConditionalActivationTest PROPERTIES
    TIMEOUT 120
    LABELS "conditional;activation;lefx;extension;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(TaskSchedulerTest PROPERTIES
    TIMEOUT 120
    LABELS "scheduler;runtime;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(ProfilerTest PROPERTIES
    TIMEOUT 120
    LABELS "profiler;runtime;performance;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(ErrorHandlingTest PROPERTIES
    TIMEOUT 120
    LABELS "error;logging;runtime;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 오디오 처리 테스트 속성 설정
set_tests_properties(AudioIOTest PROPERTIES
    TIMEOUT 60
    LABELS "audio;io;platform;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(STFTTest PROPERTIES
    TIMEOUT 120
    LABELS "audio;stft;dsp;simd;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(MelScaleTest PROPERTIES
    TIMEOUT 120
    LABELS "audio;mel;dsp;simd;optimization;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(VocoderTest PROPERTIES
    TIMEOUT 120
    LABELS "audio;vocoder;graph;realtime;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 데스크톱 최적화 테스트 속성 설정
set_tests_properties(DesktopOptimizationTest PROPERTIES
    TIMEOUT 180
    LABELS "desktop;optimization;multicore;gpu;audio;performance;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 모바일 최적화 테스트 속성 설정
set_tests_properties(MobileOptimizationTest PROPERTIES
    TIMEOUT 300
    LABELS "mobile;optimization;battery;thermal;memory;power;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 임베디드 최적화 테스트 속성 설정
set_tests_properties(EmbeddedOptimizationTest PROPERTIES
    TIMEOUT 300
    LABELS "embedded;optimization;minimal;memory;power;lowpower;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 플러그인 시스템 테스트 속성 설정
set_tests_properties(PluginTest PROPERTIES
    TIMEOUT 120
    LABELS "plugin;runtime;dynamic;extension;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 플러그인 의존성 관리 테스트 속성 설정
set_tests_properties(PluginDependencyTest PROPERTIES
    TIMEOUT 180
    LABELS "plugin;dependency;version;update;security;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 오디오 효과 플러그인 테스트 속성 설정
set_tests_properties(AudioEffectsTest PROPERTIES
    TIMEOUT 180
    LABELS "audio;effects;plugin;reverb;equalizer;pipeline;realtime;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# UTAU 파라미터 파싱 테스트 속성 설정
set_tests_properties(UTAUParameterParsingTest PROPERTIES
    TIMEOUT 120
    LABELS "utau;parameter;parsing;world4utau;example;validation;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# F0 추출 테스트 속성 설정
set_tests_properties(F0ExtractionTest PROPERTIES
    TIMEOUT 300
    LABELS "f0;extraction;world;dio;harvest;world4utau;example;accuracy;performance;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 통합 테스트 속성 설정 (16번 작업)
set_tests_properties(IntegrationPipelineTest PROPERTIES
    TIMEOUT 300
    LABELS "integration;pipeline;endtoend;text2audio;streaming;error;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(IntegrationStreamingTest PROPERTIES
    TIMEOUT 300
    LABELS "integration;streaming;realtime;latency;buffer;audio;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(IntegrationPerformanceTest PROPERTIES
    TIMEOUT 600
    LABELS "integration;performance;latency;throughput;memory;cpu;profiling;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(IntegrationPlatformTest PROPERTIES
    TIMEOUT 600
    LABELS "integration;platform;desktop;mobile;embedded;crossplatform;core;libetude"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(TARGET test_unity_simple)
    set_tests_properties(UnityBasicTest PROPERTIES
        TIMEOUT 30
        LABELS "unity;basic;standalone"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

if(TARGET test_math_simple)
    set_tests_properties(MathFunctionsTest PROPERTIES
        TIMEOUT 30
        LABELS "math;functions;standalone"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

if(TARGET test_fast_math_minimal)
    set_tests_properties(FastMathTest PROPERTIES
        TIMEOUT 30
        LABELS "math;fast;standalone"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

set_tests_properties(SIMDMathFunctionsTest PROPERTIES
    TIMEOUT 60
    LABELS "math;simd;vectorized"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 레거시 테스트 속성 설정
set_tests_properties(SimpleUnityTest PROPERTIES
    TIMEOUT 30
    LABELS "unity;basic;legacy"
)

set_tests_properties(MathSimpleTest PROPERTIES
    TIMEOUT 30
    LABELS "math;simple;legacy"
)

set_tests_properties(FastMathMinimalTest PROPERTIES
    TIMEOUT 30
    LABELS "math;fast;legacy"
)

# 테스트 데이터 복사
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# 크로스 플랫폼 테스트 실행 타겟
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS tensor_test test_unity_simple test_math_simple test_fast_math_minimal
    COMMENT "모든 독립적인 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 텐서 테스트 실행 타겟
add_custom_target(run_tensor_test
    COMMAND tensor_test
    DEPENDS tensor_test
    COMMENT "텐서 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 특정 라벨별 테스트 실행 타겟
add_custom_target(run_unity_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "unity"
    DEPENDS test_unity_simple
    COMMENT "Unity 프레임워크 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_math_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "math"
    DEPENDS test_math_simple test_fast_math_minimal
    COMMENT "수학 함수 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_standalone_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "standalone"
    DEPENDS test_unity_simple test_math_simple test_fast_math_minimal
    COMMENT "독립적인 테스트만 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 개별 테스트 실행 타겟
add_custom_target(run_unity_basic
    COMMAND test_unity_simple
    DEPENDS test_unity_simple
    COMMENT "Unity 기본 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_math_functions
    COMMAND test_math_simple
    DEPENDS test_math_simple
    COMMENT "수학 함수 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_fast_math
    COMMAND test_fast_math_minimal
    DEPENDS test_fast_math_minimal
    COMMENT "고속 수학 함수 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 통합 테스트 실행 타겟 (16번 작업)
add_custom_target(run_integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "integration"
    DEPENDS integration_pipeline_test integration_streaming_test integration_performance_test integration_platform_test
    COMMENT "모든 통합 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_pipeline_test
    COMMAND integration_pipeline_test
    DEPENDS integration_pipeline_test
    COMMENT "엔드투엔드 파이프라인 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_streaming_test
    COMMAND integration_streaming_test
    DEPENDS integration_streaming_test
    COMMENT "실시간 스트리밍 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_performance_test
    COMMAND integration_performance_test
    DEPENDS integration_performance_test
    COMMENT "성능 측정 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(run_platform_test
    COMMAND integration_platform_test
    DEPENDS integration_platform_test
    COMMENT "플랫폼별 테스트 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 테스트 요약 정보 출력 타겟
add_custom_target(test_info
    COMMAND ${CMAKE_COMMAND} -E echo "=== LibEtude 테스트 정보 ==="
    COMMAND ${CMAKE_COMMAND} -E echo "사용 가능한 테스트 타겟:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_tensor_test: 텐서 시스템 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_all_tests: 모든 독립적인 테스트 실행"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_unity_tests: Unity 프레임워크 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_math_tests: 수학 함수 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_standalone_tests: 독립적인 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_unity_basic: Unity 기본 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_math_functions: 수학 함수 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_fast_math: 고속 수학 함수 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "통합 테스트 타겟:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_integration_tests: 모든 통합 테스트 실행"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_pipeline_test: 엔드투엔드 파이프라인 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_streaming_test: 실시간 스트리밍 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_performance_test: 성능 측정 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo "  - run_platform_test: 플랫폼별 테스트"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "CTest 명령어:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest: 모든 테스트 실행"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L tensor: 텐서 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L standalone: 독립적인 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L math: 수학 관련 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L integration: 통합 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L performance: 성능 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest -L platform: 플랫폼별 테스트만"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ctest --verbose: 상세한 출력"
    COMMENT "테스트 정보 출력"
)

# 커버리지 설정 (GCC/Clang)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    option(ENABLE_COVERAGE "Enable code coverage" OFF)
    if(ENABLE_COVERAGE)
        # 독립적인 테스트들에 커버리지 적용
        target_compile_options(tensor_test PRIVATE --coverage)
        target_link_options(tensor_test PRIVATE --coverage)
        target_compile_options(test_unity_simple PRIVATE --coverage)
        target_link_options(test_unity_simple PRIVATE --coverage)
        target_compile_options(test_math_simple PRIVATE --coverage)
        target_link_options(test_math_simple PRIVATE --coverage)
        target_compile_options(test_fast_math_minimal PRIVATE --coverage)
        target_link_options(test_fast_math_minimal PRIVATE --coverage)
    endif()
endif()