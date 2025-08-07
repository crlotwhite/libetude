# LibEtude 소스 디렉토리 CMake 설정

# 소스 파일 수집
set(LIBETUDE_CORE_SOURCES
    # 코어 엔진
    core/api.c
    core/engine.c
    core/init.c

    # 하드웨어 감지
    core/hardware/hardware_detect.c

    # 커널 시스템
    core/kernels/kernel_registry.c
    core/kernels/cpu/cpu_kernels.c

    # 텐서 시스템
    core/tensor/tensor.c
    core/tensor/tensor_ops.c
    core/tensor/quantization.c

    # 그래프 시스템
    core/graph/graph.c
    core/graph/operators.c

    # 메모리 관리
    core/memory/memory_pool.c
    core/memory/allocator.c
    core/memory/memory_optimization.c

    # 수학 라이브러리
    core/math/fast_math.c
    core/math/activation.c

    # LEF 포맷
    lef/format/lef_format.c
    lef/format/lefx_format.c
    lef/loader/model_loader.c
    lef/extensions/extension_loader.c
    lef/extensions/conditional_activation.c
    lef/compression/compression.c
    lef/compression/voice_compression.c

    # 런타임 시스템
    runtime/scheduler/task_scheduler_simple.c
    runtime/profiler/profiler.c
    runtime/error/error_handling.c
    runtime/plugin/plugin.c
    runtime/plugin/plugin_dependency.c

    # 벤치마크 시스템
    core/benchmark/benchmark.c

    # 오디오 처리
    audio/io/audio_io.c
    audio/dsp/stft.c
    audio/dsp/mel_scale.c
    audio/vocoder/vocoder.c
    audio/effects/audio_effects.c
    audio/effects/reverb_plugin.c
    audio/effects/equalizer_plugin.c
    audio/effects/delay_plugin.c
    audio/effects/compressor_plugin.c

    # 플랫폼 추상화 레이어
    platform/common.c
    platform/factory.c
    platform/platform.c
    platform/audio_common.c
    platform/system_common.c
    platform/optimization.c
    platform/runtime_adaptation.c

    # 플랫폼별 구현
    platform/platform_core.c
    platform/platform_utils.c
    platform/desktop_optimization.c
    platform/embedded_optimization.c
    platform/mobile_optimization.c
    platform/mobile_power_management.c
    platform/thermal_management.c
    platform/memory_common.c
    platform/threading_common.c
    platform/filesystem_common.c
    platform/network_common.c
    platform/dynlib_common.c
    platform/dynlib_error_mapping.c
)

# SIMD 커널 소스 (조건부 컴파일)
if(LIBETUDE_ENABLE_SIMD)
    # 기본 SIMD 커널은 항상 포함 (fallback 구현 포함)
    list(APPEND LIBETUDE_CORE_SOURCES
        core/kernels/simd/simd_kernels.c
    )

    # 플랫폼별 최적화 커널들
    if(HAVE_SSE)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/sse_kernels.c
        )
    endif()

    if(HAVE_AVX2)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/avx_kernels.c
        )
        set_source_files_properties(core/kernels/simd/avx_kernels.c PROPERTIES COMPILE_FLAGS "-mavx2")
    endif()

    if(HAVE_NEON)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/neon_kernels.c
        )
    endif()

    message(STATUS "SIMD support enabled. HAVE_SSE=${HAVE_SSE}, HAVE_AVX=${HAVE_AVX}, HAVE_AVX2=${HAVE_AVX2}, HAVE_NEON=${HAVE_NEON}")
endif()

# GPU 커널 소스 (조건부 컴파일)
if(LIBETUDE_ENABLE_GPU)
    list(APPEND LIBETUDE_CORE_SOURCES
        core/kernels/gpu/gpu_kernels.c
    )
endif()

# 플랫폼별 소스 파일
if(WIN32)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/windows/platform_init.c
        platform/windows/windows_audio.c
        platform/windows/windows_utils.c
        platform/windows/windows_system.c
        platform/windows/dynlib_windows.c
        platform/windows/filesystem_windows.c
        platform/windows/memory_windows.c
        platform/windows/network_windows.c
        platform/windows/threading_windows.c
        platform/windows/factory_windows.c
    )
elseif(APPLE)
    if(IOS)
        list(APPEND LIBETUDE_CORE_SOURCES
            platform/ios/platform_init.c
            platform/ios/ios_audio.c
            platform/ios/ios_utils.c
        )
    else()
        list(APPEND LIBETUDE_CORE_SOURCES
            platform/macos/platform_init.c
            platform/macos/macos_audio.c
            platform/macos/macos_utils.c
            platform/macos/macos_compat.c
            platform/macos/macos_system.c
            platform/macos/network_macos.c
            platform/macos/factory_macos.c
        )
    endif()
elseif(ANDROID)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/android/platform_init.c
        platform/android/android_audio.c
        platform/android/android_utils.c
    )
elseif(UNIX)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/linux/platform_init.c
        platform/linux/linux_audio.c
        platform/linux/linux_utils.c
        platform/linux/linux_system.c
        platform/linux/network_linux.c
        platform/linux/factory_linux.c
    )
endif()

# POSIX 공통 파일들 (Unix 계열 플랫폼에서 사용)
if(UNIX)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/posix/dynlib_posix.c
        platform/posix/filesystem_posix.c
        platform/posix/memory_posix.c
        platform/posix/threading_posix.c
    )
endif()

# 정적 라이브러리 생성
find_package(Threads REQUIRED)
add_library(libetude_static STATIC ${LIBETUDE_CORE_SOURCES})
target_include_directories(libetude_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
# Ensure -mavx2 is applied to avx_kernels.c for libetude_static
if(LIBETUDE_ENABLE_SIMD AND HAVE_AVX2)
    target_compile_options(libetude_static PRIVATE $<$<COMPILE_LANGUAGE:C>:-mavx2>)
endif()

# 동적 라이브러리 생성 (선택적)
if(NOT LIBETUDE_MINIMAL)
    add_library(libetude_shared SHARED ${LIBETUDE_CORE_SOURCES})
    target_include_directories(libetude_shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_compile_definitions(libetude_shared PRIVATE LIBETUDE_EXPORTS)
    # Ensure -mavx2 is applied to avx_kernels.c for libetude_shared
    if(LIBETUDE_ENABLE_SIMD AND HAVE_AVX2)
        target_compile_options(libetude_shared PRIVATE $<$<COMPILE_LANGUAGE:C>:-mavx2>)
    endif()
    # 라이브러리 이름 설정
    set_target_properties(libetude_shared PROPERTIES
        OUTPUT_NAME "libetude"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# 컴파일러별 최적화 플래그
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(libetude_static PRIVATE /O2 /Oi /Ot /GL)
        if(NOT LIBETUDE_MINIMAL)
            target_compile_options(libetude_shared PRIVATE /O2 /Oi /Ot /GL)
        endif()
    else()
        target_compile_options(libetude_static PRIVATE -O3 -ffast-math -funroll-loops)
        if(NOT LIBETUDE_MINIMAL)
            target_compile_options(libetude_shared PRIVATE -O3 -ffast-math -funroll-loops)
        endif()
    endif()
endif()

# 링크 라이브러리
if(WIN32)
    target_link_libraries(libetude_static PRIVATE winmm dsound psapi pdh)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE winmm dsound psapi pdh)
    endif()
elseif(APPLE)
    target_link_libraries(libetude_static PRIVATE "-framework AudioToolbox" "-framework CoreAudio" dl)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE "-framework AudioToolbox" "-framework CoreAudio" dl)
    endif()
elseif(UNIX)
    target_link_libraries(libetude_static PUBLIC asound Threads::Threads m dl uuid)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE asound pthread m dl uuid)
    endif()
endif()

# OpenMP 지원
if(LIBETUDE_ENABLE_SIMD AND OpenMP_FOUND)
    target_link_libraries(libetude_static PRIVATE OpenMP::OpenMP_C)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

# 설치 설정 (컴포넌트별)
install(TARGETS libetude_static
    ARCHIVE DESTINATION lib COMPONENT Development
    LIBRARY DESTINATION lib COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
)

if(NOT LIBETUDE_MINIMAL)
    install(TARGETS libetude_shared
        ARCHIVE DESTINATION lib COMPONENT Development
        LIBRARY DESTINATION lib COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
    )
endif()

# 헤더 파일 설치
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/libetude
    DESTINATION include
    COMPONENT Development
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# pkg-config 파일 설치
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/libetude.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libetude.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libetude.pc
    DESTINATION lib/pkgconfig
    COMPONENT Development
)