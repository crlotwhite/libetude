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

    # 오디오 처리
    audio/io/audio_io.c
    audio/dsp/stft.c
    audio/dsp/mel_scale.c
    audio/vocoder/vocoder.c

    # 플랫폼별 구현
    platform/platform_utils.c
    platform/desktop_optimization.c
    platform/embedded_optimization.c
    platform/mobile_power_management.c
    platform/thermal_management.c
)

# SIMD 커널 소스 (조건부 컴파일)
if(LIBETUDE_ENABLE_SIMD)
    list(APPEND LIBETUDE_CORE_SOURCES
        core/kernels/simd/simd_kernels.c
    )

    if(HAVE_SSE)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/sse_kernels.c
        )
    endif()

    if(HAVE_AVX)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/avx_kernels.c
        )
    endif()

    if(HAVE_NEON)
        list(APPEND LIBETUDE_CORE_SOURCES
            core/kernels/simd/neon_kernels.c
        )
    endif()
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
        platform/windows/windows_audio.c
        platform/windows/windows_utils.c
    )
elseif(APPLE)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/macos/macos_audio.c
        platform/macos/macos_utils.c
    )
elseif(ANDROID)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/android/android_audio.c
        platform/android/android_utils.c
    )
elseif(UNIX)
    list(APPEND LIBETUDE_CORE_SOURCES
        platform/linux/linux_audio.c
        platform/linux/linux_utils.c
    )
endif()

# 정적 라이브러리 생성
add_library(libetude_static STATIC ${LIBETUDE_CORE_SOURCES})
target_include_directories(libetude_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 동적 라이브러리 생성 (선택적)
if(NOT LIBETUDE_MINIMAL)
    add_library(libetude_shared SHARED ${LIBETUDE_CORE_SOURCES})
    target_include_directories(libetude_shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_compile_definitions(libetude_shared PRIVATE LIBETUDE_EXPORTS)

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
    target_link_libraries(libetude_static PRIVATE winmm dsound psapi)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE winmm dsound psapi)
    endif()
elseif(APPLE)
    target_link_libraries(libetude_static PRIVATE "-framework AudioToolbox" "-framework CoreAudio")
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE "-framework AudioToolbox" "-framework CoreAudio")
    endif()
elseif(UNIX)
    target_link_libraries(libetude_static PRIVATE asound pthread m)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE asound pthread m)
    endif()
endif()

# OpenMP 지원
if(LIBETUDE_ENABLE_SIMD AND OpenMP_FOUND)
    target_link_libraries(libetude_static PRIVATE OpenMP::OpenMP_C)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

# 설치 설정
install(TARGETS libetude_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

if(NOT LIBETUDE_MINIMAL)
    install(TARGETS libetude_shared
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()