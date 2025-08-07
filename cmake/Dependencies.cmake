# LibEtude 의존성 관리
# Copyright (c) 2025 LibEtude Project

# 플랫폼 추상화 레이어 의존성 자동 감지
function(detect_platform_dependencies)
    message(STATUS "플랫폼 추상화 레이어 의존성 자동 감지 중...")

    # 플랫폼별 필수 의존성 감지
    if(WIN32)
        # Windows API 라이브러리 확인
        find_library(KERNEL32_LIB kernel32 REQUIRED)
        find_library(USER32_LIB user32 REQUIRED)
        find_library(ADVAPI32_LIB advapi32 REQUIRED)
        find_library(PSAPI_LIB psapi REQUIRED)
        find_library(PDH_LIB pdh REQUIRED)

        # 오디오 관련 라이브러리
        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            find_library(WINMM_LIB winmm REQUIRED)
            find_library(DSOUND_LIB dsound)
            find_library(OLE32_LIB ole32 REQUIRED)
            find_library(OLEAUT32_LIB oleaut32 REQUIRED)
        endif()

        # 네트워크 관련 라이브러리
        if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
            find_library(WS2_32_LIB ws2_32 REQUIRED)
            find_library(WSOCK32_LIB wsock32)
        endif()

        message(STATUS "Windows 의존성 감지 완료")

    elseif(APPLE)
        # macOS/iOS 프레임워크 확인
        find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
        find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
        find_library(IOKIT_FRAMEWORK IOKit)

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
            find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
            find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
        endif()

        if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
            find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)
        endif()

        message(STATUS "macOS/iOS 의존성 감지 완료")

    elseif(UNIX)
        # Linux 시스템 라이브러리 확인
        find_library(RT_LIB rt)
        find_library(DL_LIB dl REQUIRED)
        find_library(M_LIB m REQUIRED)

        # 오디오 관련 라이브러리
        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(ALSA alsa)
                pkg_check_modules(PULSE libpulse)
                pkg_check_modules(JACK jack)
            endif()

            if(NOT ALSA_FOUND AND NOT PULSE_FOUND AND NOT JACK_FOUND)
                message(WARNING "오디오 라이브러리(ALSA/PulseAudio/JACK)를 찾을 수 없습니다")
            endif()
        endif()

        # 시스템 정보 관련
        if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
            find_library(PROCFS_LIB proc)  # /proc 파일시스템 접근용
        endif()

        message(STATUS "Linux 의존성 감지 완료")
    endif()

    # 공통 의존성
    find_package(Threads REQUIRED)

    # 크로스 컴파일 환경 감지
    if(CMAKE_CROSSCOMPILING)
        message(STATUS "크로스 컴파일 환경 감지됨")
        message(STATUS "타겟 시스템: ${CMAKE_SYSTEM_NAME}")
        message(STATUS "타겟 프로세서: ${CMAKE_SYSTEM_PROCESSOR}")

        # 크로스 컴파일용 의존성 조정
        set(LIBETUDE_CROSS_COMPILING ON PARENT_SCOPE)
    endif()
endfunction()

# 의존성 찾기 함수
function(find_libetude_dependencies)
    message(STATUS "LibEtude 의존성 확인 중...")

    # 플랫폼 추상화 레이어 의존성 감지
    detect_platform_dependencies()

    # 필수 의존성
    find_package(Threads REQUIRED)

    # 수학 라이브러리 (Unix 계열에서 필요)
    if(UNIX AND NOT APPLE)
        find_library(MATH_LIBRARY m)
        if(NOT MATH_LIBRARY)
            message(FATAL_ERROR "수학 라이브러리를 찾을 수 없습니다.")
        endif()
    endif()

    # 플랫폼별 의존성
    if(WIN32)
        # Windows 의존성
        find_library(WINMM_LIBRARY winmm)
        find_library(DSOUND_LIBRARY dsound)
        if(NOT WINMM_LIBRARY)
            message(FATAL_ERROR "winmm 라이브러리를 찾을 수 없습니다.")
        endif()
    elseif(APPLE)
        # macOS 의존성
        find_library(COREAUDIO_FRAMEWORK CoreAudio)
        find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
        find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
        if(NOT COREAUDIO_FRAMEWORK OR NOT AUDIOUNIT_FRAMEWORK OR NOT AUDIOTOOLBOX_FRAMEWORK)
            message(FATAL_ERROR "macOS 오디오 프레임워크를 찾을 수 없습니다.")
        endif()
    elseif(UNIX)
        # Linux 의존성
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(ALSA alsa)
            pkg_check_modules(PULSE libpulse)
        endif()

        if(NOT ALSA_FOUND AND NOT PULSE_FOUND)
            message(WARNING "ALSA 또는 PulseAudio를 찾을 수 없습니다. 오디오 기능이 제한될 수 있습니다.")
        endif()
    endif()

    # GPU 가속 의존성 (선택적)
    if(LIBETUDE_ENABLE_GPU)
        # CUDA 찾기
        find_package(CUDAToolkit QUIET)
        if(CUDAToolkit_FOUND)
            message(STATUS "CUDA 발견: ${CUDAToolkit_VERSION}")
            add_definitions(-DLIBETUDE_HAVE_CUDA)
        endif()

        # OpenCL 찾기
        find_package(OpenCL QUIET)
        if(OpenCL_FOUND)
            message(STATUS "OpenCL 발견")
            add_definitions(-DLIBETUDE_HAVE_OPENCL)
        endif()

        # Metal (macOS)
        if(APPLE)
            find_library(METAL_FRAMEWORK Metal)
            if(METAL_FRAMEWORK)
                message(STATUS "Metal 발견")
                add_definitions(-DLIBETUDE_HAVE_METAL)
            endif()
        endif()
    endif()

    # 압축 라이브러리 (선택적)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LZ4 liblz4)
        pkg_check_modules(ZSTD libzstd)
    endif()

    if(LZ4_FOUND)
        message(STATUS "LZ4 압축 라이브러리 발견")
        add_definitions(-DLIBETUDE_HAVE_LZ4)
    endif()

    if(ZSTD_FOUND)
        message(STATUS "ZSTD 압축 라이브러리 발견")
        add_definitions(-DLIBETUDE_HAVE_ZSTD)
    endif()

    message(STATUS "의존성 확인 완료")
endfunction()

# 의존성 링크 함수
function(link_libetude_dependencies target)
    # 스레드 라이브러리
    target_link_libraries(${target} PRIVATE Threads::Threads)

    # 수학 라이브러리
    if(MATH_LIBRARY)
        target_link_libraries(${target} PRIVATE ${MATH_LIBRARY})
    endif()

    # 플랫폼별 라이브러리
    if(WIN32)
        target_link_libraries(${target} PRIVATE ${WINMM_LIBRARY})
        if(DSOUND_LIBRARY)
            target_link_libraries(${target} PRIVATE ${DSOUND_LIBRARY})
        endif()
    elseif(APPLE)
        target_link_libraries(${target} PRIVATE
            ${COREAUDIO_FRAMEWORK}
            ${AUDIOUNIT_FRAMEWORK}
            ${AUDIOTOOLBOX_FRAMEWORK}
        )
        if(METAL_FRAMEWORK)
            target_link_libraries(${target} PRIVATE ${METAL_FRAMEWORK})
        endif()
    elseif(UNIX)
        if(ALSA_FOUND)
            target_link_libraries(${target} PRIVATE ${ALSA_LIBRARIES})
            target_include_directories(${target} PRIVATE ${ALSA_INCLUDE_DIRS})
        endif()
        if(PULSE_FOUND)
            target_link_libraries(${target} PRIVATE ${PULSE_LIBRARIES})
            target_include_directories(${target} PRIVATE ${PULSE_INCLUDE_DIRS})
        endif()
    endif()

    # GPU 라이브러리
    if(LIBETUDE_ENABLE_GPU)
        if(CUDAToolkit_FOUND)
            target_link_libraries(${target} PRIVATE CUDA::cudart CUDA::cublas)
        endif()
        if(OpenCL_FOUND)
            target_link_libraries(${target} PRIVATE OpenCL::OpenCL)
        endif()
    endif()

    # 압축 라이브러리
    if(LZ4_FOUND)
        target_link_libraries(${target} PRIVATE ${LZ4_LIBRARIES})
        target_include_directories(${target} PRIVATE ${LZ4_INCLUDE_DIRS})
    endif()
    if(ZSTD_FOUND)
        target_link_libraries(${target} PRIVATE ${ZSTD_LIBRARIES})
        target_include_directories(${target} PRIVATE ${ZSTD_INCLUDE_DIRS})
    endif()
endfunction()

# 컴파일러별 최적화 플래그 설정
function(set_libetude_optimization_flags target)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if(MSVC)
            # MSVC 최적화
            target_compile_options(${target} PRIVATE
                /O2          # 최대 속도 최적화
                /Oi          # 내장 함수 사용
                /Ot          # 속도 우선 최적화
                /GL          # 전체 프로그램 최적화
                /fp:fast     # 빠른 부동소수점
            )
            target_link_options(${target} PRIVATE /LTCG)  # 링크 타임 코드 생성
        else()
            # GCC/Clang 최적화
            target_compile_options(${target} PRIVATE
                -O3                    # 최대 최적화
                -ffast-math           # 빠른 수학 연산
                -funroll-loops        # 루프 언롤링
                -fomit-frame-pointer  # 프레임 포인터 생략
            )

            # 플랫폼별 최적화
            if(NOT LIBETUDE_MINIMAL)
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
                    target_compile_options(${target} PRIVATE -march=native -mtune=native)
                elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
                    target_compile_options(${target} PRIVATE -mcpu=native)
                endif()
            endif()
        endif()
    endif()
endfunction()

# SIMD 지원 확인 및 설정
function(configure_simd_support target)
    if(LIBETUDE_ENABLE_SIMD)
        include(CheckCSourceCompiles)

        # SSE 지원 확인
        set(CMAKE_REQUIRED_FLAGS "-msse")
        check_c_source_compiles("
            #include <xmmintrin.h>
            int main() { __m128 a = _mm_setzero_ps(); return 0; }
        " HAVE_SSE)

        if(HAVE_SSE)
            target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_SSE)
            if(NOT MSVC)
                target_compile_options(${target} PRIVATE -msse)
            endif()
        endif()

        # AVX 지원 확인
        set(CMAKE_REQUIRED_FLAGS "-mavx")
        check_c_source_compiles("
            #include <immintrin.h>
            int main() { __m256 a = _mm256_setzero_ps(); return 0; }
        " HAVE_AVX)

        if(HAVE_AVX)
            target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_AVX)
            if(NOT MSVC)
                target_compile_options(${target} PRIVATE -mavx)
            endif()
        endif()

        # NEON 지원 확인 (ARM)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
            check_c_source_compiles("
                #include <arm_neon.h>
                int main() { float32x4_t a = vdupq_n_f32(0.0f); return 0; }
            " HAVE_NEON)

            if(HAVE_NEON)
                target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_NEON)
            endif()
        endif()

        set(CMAKE_REQUIRED_FLAGS "")
    endif()
endfunction()

# 디버그 정보 설정
function(configure_debug_info target)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if(MSVC)
            target_compile_options(${target} PRIVATE /Zi)  # 디버그 정보 생성
            target_link_options(${target} PRIVATE /DEBUG)
        else()
            target_compile_options(${target} PRIVATE -g)   # 디버그 정보 생성
        endif()
    endif()
endfunction()

# 경고 설정
function(configure_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4           # 경고 레벨 4
            /WX           # 경고를 오류로 처리
            /wd4996       # deprecated 함수 경고 비활성화
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall         # 모든 경고
            -Wextra       # 추가 경고
            -Werror       # 경고를 오류로 처리
            -Wno-unused-parameter  # 사용하지 않는 매개변수 경고 비활성화
        )
    endif()
endfunction()

# 전체 설정 적용 함수
function(configure_libetude_target target)
    find_libetude_dependencies()
    link_libetude_dependencies(${target})
    set_libetude_optimization_flags(${target})
    configure_simd_support(${target})
    configure_debug_info(${target})
    configure_warnings(${target})

    # 위치 독립적 코드 (PIC) 설정
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # C/C++ 표준 설정
    set_target_properties(${target} PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endfunction()