# LibEtude 소스 디렉토리 CMake 설정

# 플랫폼 추상화 레이어 소스 수집
collect_platform_sources(PLATFORM_SOURCES)

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
)

# 플랫폼 추상화 레이어 소스 추가
list(APPEND LIBETUDE_CORE_SOURCES ${PLATFORM_SOURCES})

# 버전 소스 파일 추가
if(LIBETUDE_VERSION_SOURCE)
    list(APPEND LIBETUDE_CORE_SOURCES ${LIBETUDE_VERSION_SOURCE})
endif()

# 추가 소스 파일들
list(APPEND LIBETUDE_CORE_SOURCES
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

# 플랫폼별 소스 파일은 이미 collect_platform_sources()에서 처리됨

# 정적 라이브러리 생성
find_package(Threads REQUIRED)
add_library(libetude_static STATIC ${LIBETUDE_CORE_SOURCES})
target_include_directories(libetude_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 플랫폼 추상화 레이어 설정 적용
set_platform_compile_options(libetude_static)
link_platform_libraries(libetude_static)

# SIMD 최적화 적용
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

    # 플랫폼 추상화 레이어 설정 적용
    set_platform_compile_options(libetude_shared)
    link_platform_libraries(libetude_shared)

    # SIMD 최적화 적용
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

# 플랫폼별 라이브러리 링크는 link_platform_libraries()에서 처리됨
# 추가 라이브러리 링크
target_link_libraries(libetude_static PRIVATE Threads::Threads)
if(NOT LIBETUDE_MINIMAL)
    target_link_libraries(libetude_shared PRIVATE Threads::Threads)
endif()

# OpenMP 지원
if(LIBETUDE_ENABLE_SIMD AND OpenMP_FOUND)
    target_link_libraries(libetude_static PRIVATE OpenMP::OpenMP_C)
    if(NOT LIBETUDE_MINIMAL)
        target_link_libraries(libetude_shared PRIVATE OpenMP::OpenMP_C)
    endif()
endif()

# 라이브러리 별칭 생성 (CMake 타겟 네임스페이스)
add_library(LibEtude::libetude ALIAS libetude_static)
if(NOT LIBETUDE_MINIMAL)
    add_library(LibEtude::libetude_shared ALIAS libetude_shared)
endif()

# 설치 설정은 configure_libetude_install()에서 처리됨
# 타겟 내보내기를 위한 설치 설정
install(TARGETS libetude_static
    EXPORT LibEtudeTargets
    ARCHIVE DESTINATION lib COMPONENT Development
    LIBRARY DESTINATION lib COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
)

if(NOT LIBETUDE_MINIMAL)
    install(TARGETS libetude_shared
        EXPORT LibEtudeTargets
        ARCHIVE DESTINATION lib COMPONENT Development
        LIBRARY DESTINATION lib COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
    )
endif()

# pkg-config 플랫폼별 라이브러리 설정
if(WIN32)
    set(PKGCONFIG_PLATFORM_LIBS "Libs.private: -lkernel32 -luser32 -lwinmm -lpsapi -lpdh")
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -ldsound -lole32 -loleaut32")
    endif()
    if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -lws2_32")
    endif()
elseif(APPLE)
    set(PKGCONFIG_PLATFORM_LIBS "Libs.private: -framework CoreFoundation")
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -framework CoreAudio -framework AudioUnit -framework AudioToolbox")
    endif()
    if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -lpthread")
    endif()
    if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -ldl")
    endif()
elseif(UNIX)
    set(PKGCONFIG_PLATFORM_LIBS "Libs.private: -lm")
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION AND ALSA_FOUND)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -lasound")
    endif()
    if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -lpthread")
    endif()
    if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -ldl")
    endif()
    if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
        set(PKGCONFIG_PLATFORM_LIBS "${PKGCONFIG_PLATFORM_LIBS} -lrt")
    endif()
endif()

# 전체 설치 설정 적용
configure_libetude_install(libetude_static)