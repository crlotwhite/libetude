# LibEtude 테스트 CMake 설정

# 테스트 프레임워크 설정 (간단한 자체 구현)
set(TEST_FRAMEWORK_SOURCES
    framework/test_framework.c
)

# 단위 테스트 소스 파일
set(UNIT_TEST_SOURCES
    unit/test_engine.c
    unit/test_kernels.c
    unit/test_tensor.c
    unit/test_memory.c
    unit/test_hardware.c
    unit/test_memory_allocator.c
    unit/test_memory_optimization.c
    unit/test_fast_math.c
)

# 통합 테스트 소스 파일
set(INTEGRATION_TEST_SOURCES
    integration/test_pipeline.c
    integration/test_streaming.c
)

# 성능 테스트 소스 파일
set(PERFORMANCE_TEST_SOURCES
    performance/benchmark_kernels.c
    performance/benchmark_inference.c
)

# 테스트 프레임워크 라이브러리 생성
add_library(test_framework STATIC ${TEST_FRAMEWORK_SOURCES})
target_include_directories(test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 단위 테스트 실행 파일
add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests
    test_framework
    libetude_static
)
target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 통합 테스트 실행 파일
add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(integration_tests
    test_framework
    libetude_static
)
target_include_directories(integration_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 성능 테스트 실행 파일
add_executable(performance_tests ${PERFORMANCE_TEST_SOURCES})
target_link_libraries(performance_tests
    test_framework
    libetude_static
)
target_include_directories(performance_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 하드웨어 테스트 실행 파일 (독립적으로 실행 가능)
add_executable(hardware_test unit/test_hardware.c)
target_link_libraries(hardware_test
    test_framework
    libetude_static
)
target_include_directories(hardware_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 메모리 할당자 테스트 실행 파일 (독립적으로 실행 가능)
add_executable(memory_allocator_test unit/test_memory_allocator.c)
target_link_libraries(memory_allocator_test
    test_framework
    libetude_static
)
target_include_directories(memory_allocator_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 메모리 최적화 테스트 실행 파일 (독립적으로 실행 가능)
add_executable(memory_optimization_test unit/test_memory_optimization.c)
target_link_libraries(memory_optimization_test
    test_framework
    libetude_static
)
target_include_directories(memory_optimization_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 고속 수학 함수 테스트 실행 파일 (독립적으로 실행 가능)
add_executable(fast_math_test unit/test_fast_math.c)
target_link_libraries(fast_math_test
    test_framework
    libetude_static
)
target_include_directories(fast_math_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# CTest 통합
add_test(NAME UnitTests COMMAND unit_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)
add_test(NAME PerformanceTests COMMAND performance_tests)
add_test(NAME HardwareTest COMMAND hardware_test)
add_test(NAME MemoryAllocatorTest COMMAND memory_allocator_test)
add_test(NAME MemoryOptimizationTest COMMAND memory_optimization_test)
add_test(NAME FastMathTest COMMAND fast_math_test)

# 테스트 속성 설정
set_tests_properties(UnitTests PROPERTIES
    TIMEOUT 60
    LABELS "unit"
)

set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 120
    LABELS "integration"
)

set_tests_properties(PerformanceTests PROPERTIES
    TIMEOUT 300
    LABELS "performance"
)

set_tests_properties(HardwareTest PROPERTIES
    TIMEOUT 30
    LABELS "hardware"
)

set_tests_properties(MemoryAllocatorTest PROPERTIES
    TIMEOUT 60
    LABELS "memory"
)

set_tests_properties(MemoryOptimizationTest PROPERTIES
    TIMEOUT 90
    LABELS "memory;optimization"
)

set_tests_properties(FastMathTest PROPERTIES
    TIMEOUT 60
    LABELS "math;performance"
)

# 테스트 데이터 복사
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)

# 커버리지 설정 (GCC/Clang)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    option(ENABLE_COVERAGE "Enable code coverage" OFF)
    if(ENABLE_COVERAGE)
        target_compile_options(unit_tests PRIVATE --coverage)
        target_link_options(unit_tests PRIVATE --coverage)
        target_compile_options(integration_tests PRIVATE --coverage)
        target_link_options(integration_tests PRIVATE --coverage)
    endif()
endif()