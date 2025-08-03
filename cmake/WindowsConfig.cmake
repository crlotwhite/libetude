# LibEtude Windows 특화 빌드 설정
# Copyright (c) 2025 LibEtude Project

# Windows SDK 자동 감지 함수
function(detect_windows_sdk)
    message(STATUS "Windows SDK 감지 중...")

    # Windows SDK 버전 감지
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
        message(STATUS "Visual Studio에서 Windows SDK 버전 감지: ${WINDOWS_SDK_VERSION}")
    else()
        # 레지스트리에서 Windows SDK 버전 찾기
        get_filename_component(WINDOWS_KITS_DIR
            "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
            ABSOLUTE)

        if(EXISTS "${WINDOWS_KITS_DIR}")
            file(GLOB SDK_VERSIONS "${WINDOWS_KITS_DIR}/Include/*")
            list(SORT SDK_VERSIONS)
            list(REVERSE SDK_VERSIONS)

            foreach(SDK_PATH ${SDK_VERSIONS})
                get_filename_component(SDK_VERSION ${SDK_PATH} NAME)
                if(SDK_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
                    set(WINDOWS_SDK_VERSION ${SDK_VERSION})
                    message(STATUS "Windows SDK 버전 감지: ${WINDOWS_SDK_VERSION}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    if(NOT WINDOWS_SDK_VERSION)
        message(WARNING "Windows SDK를 찾을 수 없습니다. 기본 시스템 헤더를 사용합니다.")
        set(WINDOWS_SDK_VERSION "10.0.19041.0" PARENT_SCOPE)
    else()
        set(WINDOWS_SDK_VERSION ${WINDOWS_SDK_VERSION} PARENT_SCOPE)
    endif()
endfunction()

# MSVC 컴파일러 최적화 설정
function(configure_msvc_optimization target)
    if(MSVC)
        message(STATUS "MSVC 최적화 플래그 설정 중...")

        # 기본 최적화 플래그
        target_compile_options(${target} PRIVATE
            /W4                    # 경고 레벨 4
            /wd4996                # deprecated 함수 경고 비활성화
            /wd4819                # 문자 인코딩 경고 비활성화
            /wd4100                # 사용하지 않는 매개변수 경고 비활성화
            /wd4244                # 타입 변환 경고 비활성화
            /wd4047                # 포인터 레벨 차이 경고 비활성화
            /wd4024                # 매개변수 타입 차이 경고 비활성화
            /wd4013                # 정의되지 않은 함수 경고 비활성화
            /wd4133                # 호환되지 않는 타입 경고 비활성화
            /D_CRT_SECURE_NO_WARNINGS  # CRT 보안 경고 비활성화
            /DWIN32_LEAN_AND_MEAN      # Windows 헤더 최소화
            /DNOMINMAX                 # min/max 매크로 비활성화
            /D_WIN32_WINNT=0x0A00      # Windows 10 이상 타겟
            /utf-8                     # UTF-8 인코딩 사용
        )

        # Release 모드 최적화
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_compile_options(${target} PRIVATE
                /O2                # 최대 속도 최적화
                /Oi                # 내장 함수 사용
                /Ot                # 속도 우선 최적화
                /Oy                # 프레임 포인터 생략
                /GL                # 전체 프로그램 최적화
                /fp:fast           # 빠른 부동소수점 연산
                /GS-               # 버퍼 보안 검사 비활성화 (성능 향상)
            )

            # CPU 아키텍처별 최적화
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
                # AVX2 지원 확인
                include(CheckCXXSourceCompiles)
                set(CMAKE_REQUIRED_FLAGS "/arch:AVX2")
                check_cxx_source_compiles("
                    #include <immintrin.h>
                    int main() {
                        __m256 a = _mm256_setzero_ps();
                        return 0;
                    }
                " HAVE_AVX2_MSVC)

                if(HAVE_AVX2_MSVC)
                    target_compile_options(${target} PRIVATE /arch:AVX2)
                    target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_AVX2)
                    message(STATUS "AVX2 최적화 활성화")
                else()
                    # AVX 지원 확인
                    set(CMAKE_REQUIRED_FLAGS "/arch:AVX")
                    check_cxx_source_compiles("
                        #include <immintrin.h>
                        int main() {
                            __m256 a = _mm256_setzero_ps();
                            return 0;
                        }
                    " HAVE_AVX_MSVC)

                    if(HAVE_AVX_MSVC)
                        target_compile_options(${target} PRIVATE /arch:AVX)
                        target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_AVX)
                        message(STATUS "AVX 최적화 활성화")
                    endif()
                endif()
                set(CMAKE_REQUIRED_FLAGS "")
            endif()

            # 링크 타임 최적화
            target_link_options(${target} PRIVATE
                /LTCG              # 링크 타임 코드 생성
                /OPT:REF           # 참조되지 않는 함수 제거
                /OPT:ICF           # 동일한 함수 병합
            )
        endif()

        # Debug 모드 설정
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE
                /Zi                # 디버그 정보 생성
                /Od                # 최적화 비활성화
                /RTC1              # 런타임 검사
                /MDd               # 멀티스레드 디버그 DLL
            )
            target_link_options(${target} PRIVATE /DEBUG:FULL)
        endif()

        # RelWithDebInfo 모드 설정
        if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_compile_options(${target} PRIVATE /Zi)
            target_link_options(${target} PRIVATE /DEBUG:FULL /INCREMENTAL:NO)
        endif()
    endif()
endfunction()

# MinGW 컴파일러 최적화 설정
function(configure_mingw_optimization target)
    if(MINGW OR CMAKE_COMPILER_IS_GNUCC)
        message(STATUS "MinGW/GCC 최적화 플래그 설정 중...")

        # 기본 설정
        target_compile_options(${target} PRIVATE
            -Wall                  # 모든 경고
            -Wextra                # 추가 경고
            -Wno-unused-parameter  # 사용하지 않는 매개변수 경고 비활성화
            -Wno-unused-variable   # 사용하지 않는 변수 경고 비활성화
            -Wno-missing-field-initializers  # 필드 초기화 경고 비활성화
            -D_WIN32_WINNT=0x0A00  # Windows 10 이상 타겟
            -DWINVER=0x0A00        # Windows 버전 정의
        )

        # Release 모드 최적화
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_compile_options(${target} PRIVATE
                -O3                    # 최대 최적화
                -ffast-math           # 빠른 수학 연산
                -funroll-loops        # 루프 언롤링
                -fomit-frame-pointer  # 프레임 포인터 생략
                -flto                 # 링크 타임 최적화
            )

            # CPU 아키텍처별 최적화
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
                # AVX2 지원 확인
                include(CheckCXXSourceCompiles)
                set(CMAKE_REQUIRED_FLAGS "-mavx2")
                check_cxx_source_compiles("
                    #include <immintrin.h>
                    int main() {
                        __m256 a = _mm256_setzero_ps();
                        return 0;
                    }
                " HAVE_AVX2_GCC)

                if(HAVE_AVX2_GCC)
                    target_compile_options(${target} PRIVATE -mavx2 -mfma)
                    target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_AVX2)
                    message(STATUS "AVX2 최적화 활성화")
                else()
                    # AVX 지원 확인
                    set(CMAKE_REQUIRED_FLAGS "-mavx")
                    check_cxx_source_compiles("
                        #include <immintrin.h>
                        int main() {
                            __m256 a = _mm256_setzero_ps();
                            return 0;
                        }
                    " HAVE_AVX_GCC)

                    if(HAVE_AVX_GCC)
                        target_compile_options(${target} PRIVATE -mavx)
                        target_compile_definitions(${target} PRIVATE LIBETUDE_HAVE_AVX)
                        message(STATUS "AVX 최적화 활성화")
                    endif()
                endif()
                set(CMAKE_REQUIRED_FLAGS "")
            endif()

            target_link_options(${target} PRIVATE -flto)
        endif()

        # Debug 모드 설정
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE -g -O0)
        endif()
    endif()
endfunction()

# Windows 특화 라이브러리 링크 설정
function(link_windows_libraries target)
    message(STATUS "Windows 라이브러리 링크 설정 중...")

    # 기본 Windows 시스템 라이브러리
    target_link_libraries(${target} PRIVATE
        kernel32    # 커널 함수
        user32      # 사용자 인터페이스 함수
        gdi32       # 그래픽 디바이스 인터페이스
        ole32       # OLE 함수
        oleaut32    # OLE 자동화 함수
        uuid        # UUID 함수
        advapi32    # 고급 API 함수
        shell32     # 셸 함수
        comdlg32    # 공통 대화상자 함수
        winspool    # 인쇄 스풀러 함수
    )

    # 오디오 관련 라이브러리
    target_link_libraries(${target} PRIVATE
        winmm       # Windows 멀티미디어 API
        dsound      # DirectSound API
        ksuser      # 커널 스트리밍 사용자 모드 라이브러리
    )

    # WASAPI 지원을 위한 추가 라이브러리
    target_link_libraries(${target} PRIVATE
        mmdevapi    # 멀티미디어 디바이스 API
        audioses    # 오디오 세션 API
        avrt        # 오디오/비디오 실시간 API
    )

    # 네트워킹 라이브러리 (필요시)
    target_link_libraries(${target} PRIVATE
        ws2_32      # Winsock 2.0
        iphlpapi    # IP Helper API
    )

    # 보안 관련 라이브러리
    target_link_libraries(${target} PRIVATE
        crypt32     # 암호화 API
        secur32     # 보안 API
    )

    # 성능 카운터 라이브러리
    target_link_libraries(${target} PRIVATE
        pdh         # 성능 데이터 헬퍼
    )
endfunction()

# Windows 특화 컴파일 정의 설정
function(set_windows_definitions target)
    message(STATUS "Windows 컴파일 정의 설정 중...")

    target_compile_definitions(${target} PRIVATE
        # 플랫폼 식별
        LIBETUDE_PLATFORM_WINDOWS=1

        # Windows 버전 정의
        _WIN32_WINNT=0x0A00        # Windows 10 이상
        WINVER=0x0A00              # Windows 10 이상
        NTDDI_VERSION=0x0A000000   # Windows 10 이상

        # Windows 헤더 최적화
        WIN32_LEAN_AND_MEAN        # Windows 헤더 최소화
        NOMINMAX                   # min/max 매크로 비활성화
        NOSERVICE                  # 서비스 관련 정의 제외
        NOMCX                      # Modem Configuration Extensions 제외
        NOIME                      # IME 관련 정의 제외

        # CRT 보안 경고 비활성화
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_WARNINGS

        # Unicode 지원
        UNICODE
        _UNICODE

        # 스레딩 관련 정의
        LIBETUDE_USE_WINDOWS_THREADING=1
        _REENTRANT

        # 오디오 관련 정의
        LIBETUDE_AUDIO_WASAPI=1
        LIBETUDE_AUDIO_DIRECTSOUND=1

        # 성능 최적화 관련
        LIBETUDE_WINDOWS_OPTIMIZATION=1
    )

    # Debug 모드에서만 활성화할 정의
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${target} PRIVATE
            _DEBUG
            LIBETUDE_DEBUG_WINDOWS=1
        )
    endif()
endfunction()

# Windows 특화 헤더 파일 생성
function(generate_windows_headers)
    message(STATUS "Windows 특화 헤더 파일 생성 중...")

    # Windows 설정 헤더 파일 경로
    set(WINDOWS_CONFIG_HEADER "${CMAKE_BINARY_DIR}/include/libetude/platform/windows_config.h")

    # 디렉토리 생성
    get_filename_component(WINDOWS_CONFIG_DIR ${WINDOWS_CONFIG_HEADER} DIRECTORY)
    file(MAKE_DIRECTORY ${WINDOWS_CONFIG_DIR})

    # Windows 설정 헤더 파일 내용 생성
    file(WRITE ${WINDOWS_CONFIG_HEADER} "
/* LibEtude Windows 플랫폼 설정 헤더 파일 */
/* 자동 생성됨 - 수정하지 마세요 */

#ifndef LIBETUDE_WINDOWS_CONFIG_H
#define LIBETUDE_WINDOWS_CONFIG_H

/* Windows 버전 정의 */
#define LIBETUDE_WINDOWS_VERSION_MAJOR ${CMAKE_SYSTEM_VERSION_MAJOR}
#define LIBETUDE_WINDOWS_VERSION_MINOR ${CMAKE_SYSTEM_VERSION_MINOR}

/* Windows SDK 버전 */
#define LIBETUDE_WINDOWS_SDK_VERSION \"${WINDOWS_SDK_VERSION}\"

/* 컴파일러 정보 */
#ifdef _MSC_VER
    #define LIBETUDE_COMPILER_MSVC 1
    #define LIBETUDE_COMPILER_VERSION _MSC_VER
#elif defined(__MINGW32__) || defined(__MINGW64__)
    #define LIBETUDE_COMPILER_MINGW 1
    #define LIBETUDE_COMPILER_VERSION __GNUC__
#endif

/* CPU 아키텍처 */
#ifdef _M_X64
    #define LIBETUDE_ARCH_X64 1
#elif defined(_M_IX86)
    #define LIBETUDE_ARCH_X86 1
#elif defined(_M_ARM64)
    #define LIBETUDE_ARCH_ARM64 1
#elif defined(_M_ARM)
    #define LIBETUDE_ARCH_ARM 1
#endif

/* SIMD 지원 */
#ifdef LIBETUDE_HAVE_AVX2
    #define LIBETUDE_WINDOWS_AVX2_SUPPORT 1
#endif
#ifdef LIBETUDE_HAVE_AVX
    #define LIBETUDE_WINDOWS_AVX_SUPPORT 1
#endif

/* 빌드 설정 */
#ifdef _DEBUG
    #define LIBETUDE_WINDOWS_DEBUG_BUILD 1
#else
    #define LIBETUDE_WINDOWS_RELEASE_BUILD 1
#endif

/* 기능 지원 플래그 */
#define LIBETUDE_WINDOWS_WASAPI_SUPPORT 1
#define LIBETUDE_WINDOWS_DIRECTSOUND_SUPPORT 1
#define LIBETUDE_WINDOWS_ETW_SUPPORT 1
#define LIBETUDE_WINDOWS_LARGE_PAGES_SUPPORT 1

#endif /* LIBETUDE_WINDOWS_CONFIG_H */
")

    message(STATUS "Windows 설정 헤더 파일 생성 완료: ${WINDOWS_CONFIG_HEADER}")
endfunction()

# Windows 빌드 시스템 전체 설정 함수
function(configure_windows_build target)
    message(STATUS "Windows 빌드 시스템 설정 시작...")

    # Windows SDK 감지
    detect_windows_sdk()

    # 컴파일러별 최적화 설정
    if(MSVC)
        configure_msvc_optimization(${target})
    elseif(MINGW OR CMAKE_COMPILER_IS_GNUCC)
        configure_mingw_optimization(${target})
    endif()

    # Windows 라이브러리 링크
    link_windows_libraries(${target})

    # Windows 컴파일 정의 설정
    set_windows_definitions(${target})

    # Windows 특화 헤더 파일 생성
    generate_windows_headers()

    # 생성된 헤더 파일 포함 경로 추가
    target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/include)

    message(STATUS "Windows 빌드 시스템 설정 완료")
endfunction()

# Windows 특화 설치 설정
function(configure_windows_install target)
    message(STATUS "Windows 설치 설정 구성 중...")

    # 런타임 라이브러리 설치
    install(TARGETS ${target}
        RUNTIME DESTINATION bin COMPONENT Runtime
        LIBRARY DESTINATION lib COMPONENT Runtime
        ARCHIVE DESTINATION lib COMPONENT Development
    )

    # 헤더 파일 설치
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Development
        FILES_MATCHING PATTERN "*.h"
    )

    # Windows 특화 헤더 파일 설치
    install(FILES ${CMAKE_BINARY_DIR}/include/libetude/platform/windows_config.h
        DESTINATION include/libetude/platform
        COMPONENT Development
    )

    # CMake 설정 파일 설치
    install(FILES
        ${CMAKE_SOURCE_DIR}/cmake/WindowsConfig.cmake
        DESTINATION lib/cmake/LibEtude
        COMPONENT Development
    )

    message(STATUS "Windows 설치 설정 완료")
endfunction()