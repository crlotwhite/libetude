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
                    target_compile_definitions(${target} PRIVATE
                        LIBETUDE_HAVE_AVX2
                        LIBETUDE_HAVE_AVX
                        LIBETUDE_HAVE_SSE41
                        LIBETUDE_HAVE_SSE2
                        LIBETUDE_HAVE_SSE
                    )
                    message(STATUS "AVX2 최적화 활성화 (AVX, SSE 포함)")
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
                        target_compile_definitions(${target} PRIVATE
                            LIBETUDE_HAVE_AVX
                            LIBETUDE_HAVE_SSE41
                            LIBETUDE_HAVE_SSE2
                            LIBETUDE_HAVE_SSE
                        )
                        message(STATUS "AVX 최적화 활성화 (SSE 포함)")
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
                /Zi                # 디버그 정보 생성 (PDB 파일)
                /ZI                # Edit and Continue 지원
                /Od                # 최적화 비활성화
                /RTC1              # 런타임 검사
                /MDd               # 멀티스레드 디버그 DLL
                /JMC               # Just My Code 디버깅 지원
                /bigobj            # 큰 오브젝트 파일 지원
            )
            target_link_options(${target} PRIVATE
                /DEBUG:FULL        # 전체 디버그 정보
                /INCREMENTAL       # 증분 링크 활성화
                /EDITANDCONTINUE   # Edit and Continue 지원
                /SUBSYSTEM:CONSOLE # 콘솔 서브시스템 (디버깅용)
            )

            # PDB 파일 출력 경로 설정
            set_target_properties(${target} PROPERTIES
                PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/debug"
                PDB_NAME "${target}_debug"
            )
        endif()

        # RelWithDebInfo 모드 설정 (최적화된 디버그 빌드)
        if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_compile_options(${target} PRIVATE
                /Zi                # 디버그 정보 생성
                /O2                # 최적화 활성화
                /Oi                # 내장 함수 사용
                /DNDEBUG           # NDEBUG 정의
            )
            target_link_options(${target} PRIVATE
                /DEBUG:FULL        # 전체 디버그 정보
                /INCREMENTAL:NO    # 증분 링크 비활성화 (최적화)
                /OPT:REF           # 참조되지 않는 함수 제거
                /OPT:ICF           # 동일한 함수 병합
            )

            # PDB 파일 출력 경로 설정
            set_target_properties(${target} PROPERTIES
                PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
                PDB_NAME "${target}_release"
            )
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
                    target_compile_options(${target} PRIVATE -mavx2 -mfma -mavx -msse4.1 -msse2)
                    target_compile_definitions(${target} PRIVATE
                        LIBETUDE_HAVE_AVX2
                        LIBETUDE_HAVE_AVX
                        LIBETUDE_HAVE_SSE41
                        LIBETUDE_HAVE_SSE2
                        LIBETUDE_HAVE_SSE
                    )
                    message(STATUS "AVX2 최적화 활성화 (AVX, SSE 포함)")
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
                        target_compile_options(${target} PRIVATE -mavx -msse4.1 -msse2)
                        target_compile_definitions(${target} PRIVATE
                            LIBETUDE_HAVE_AVX
                            LIBETUDE_HAVE_SSE41
                            LIBETUDE_HAVE_SSE2
                            LIBETUDE_HAVE_SSE
                        )
                        message(STATUS "AVX 최적화 활성화 (SSE 포함)")
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

    # 디버깅 및 진단 라이브러리
    target_link_libraries(${target} PRIVATE
        dbghelp     # 디버그 도우미 라이브러리 (스택 트레이스)
        psapi       # 프로세스 상태 API
        version     # 버전 정보 API
        imagehlp    # 이미지 도우미 라이브러리
    )

    # ETW (Event Tracing for Windows) 라이브러리
    target_link_libraries(${target} PRIVATE
        advapi32    # ETW 프로바이더 등록용
        tdh         # 트레이스 데이터 헬퍼
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

        # 오류 처리 시스템 관련
        LIBETUDE_WINDOWS_ERROR_HANDLING=1
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
#define LIBETUDE_WINDOWS_ERROR_HANDLING_SUPPORT 1

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
# find_pa
ckage 지원을 위한 타겟 생성 함수
function(libetude_create_imported_target)
    if(NOT TARGET LibEtude::LibEtude)
        message(STATUS "LibEtude 가져온 타겟 생성 중...")

        # 라이브러리 타입 결정
        if(EXISTS "${LIBETUDE_SHARED_LIBRARY}")
            add_library(LibEtude::LibEtude SHARED IMPORTED)
            set_target_properties(LibEtude::LibEtude PROPERTIES
                IMPORTED_LOCATION "${LIBETUDE_SHARED_LIBRARY}"
                IMPORTED_IMPLIB "${LIBETUDE_IMPORT_LIBRARY}"
            )
            message(STATUS "공유 라이브러리 타겟 생성: ${LIBETUDE_SHARED_LIBRARY}")
        elseif(EXISTS "${LIBETUDE_STATIC_LIBRARY}")
            add_library(LibEtude::LibEtude STATIC IMPORTED)
            set_target_properties(LibEtude::LibEtude PROPERTIES
                IMPORTED_LOCATION "${LIBETUDE_STATIC_LIBRARY}"
            )
            message(STATUS "정적 라이브러리 타겟 생성: ${LIBETUDE_STATIC_LIBRARY}")
        else()
            message(FATAL_ERROR "LibEtude 라이브러리를 찾을 수 없습니다.")
        endif()

        # 인터페이스 속성 설정
        set_target_properties(LibEtude::LibEtude PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBETUDE_INCLUDE_DIRS}"
            INTERFACE_COMPILE_DEFINITIONS "${LIBETUDE_DEFINITIONS}"
        )

        # Windows 시스템 라이브러리 의존성 설정
        if(WIN32)
            set_target_properties(LibEtude::LibEtude PROPERTIES
                INTERFACE_LINK_LIBRARIES "${LIBETUDE_WINDOWS_LIBRARIES}"
            )
        endif()

        message(STATUS "LibEtude 가져온 타겟 생성 완료")
    endif()
endfunction()

# Windows 특화 CMake 변수 설정
function(set_windows_cmake_variables)
    # 플랫폼 아키텍처 감지
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LIBETUDE_ARCH "x64" PARENT_SCOPE)
        set(LIBETUDE_ARCH_BITS 64 PARENT_SCOPE)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LIBETUDE_ARCH "x86" PARENT_SCOPE)
        set(LIBETUDE_ARCH_BITS 32 PARENT_SCOPE)
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
        set(LIBETUDE_ARCH "ARM64" PARENT_SCOPE)
        set(LIBETUDE_ARCH_BITS 64 PARENT_SCOPE)
    endif()

    # Windows 시스템 라이브러리 목록
    set(LIBETUDE_WINDOWS_LIBRARIES
        kernel32 user32 gdi32 ole32 oleaut32 uuid
        advapi32 shell32 comdlg32 winspool
        winmm dsound ksuser mmdevapi audioses avrt
        ws2_32 iphlpapi crypt32 secur32 pdh
        dbghelp psapi version imagehlp tdh
        PARENT_SCOPE
    )

    # Windows 특화 컴파일 정의
    set(LIBETUDE_DEFINITIONS
        LIBETUDE_PLATFORM_WINDOWS=1
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        PARENT_SCOPE
    )
endfunction()

# Windows 특화 유틸리티 함수들
function(libetude_add_windows_executable target_name)
    add_executable(${target_name} ${ARGN})

    # Windows 특화 설정 적용
    configure_windows_build(${target_name})

    # LibEtude 라이브러리 링크 (타겟이 존재하는 경우)
    if(TARGET LibEtude::LibEtude)
        target_link_libraries(${target_name} PRIVATE LibEtude::LibEtude)
    endif()

    message(STATUS "Windows 실행 파일 타겟 생성: ${target_name}")
endfunction()

function(libetude_add_windows_library target_name)
    add_library(${target_name} ${ARGN})

    # Windows 특화 설정 적용
    configure_windows_build(${target_name})

    message(STATUS "Windows 라이브러리 타겟 생성: ${target_name}")
endfunction()

# Windows 개발 환경 검증 함수
function(validate_windows_development_environment)
    message(STATUS "Windows 개발 환경 검증 중...")

    # Visual Studio 버전 확인
    if(MSVC)
        if(MSVC_VERSION GREATER_EQUAL 1930)
            message(STATUS "Visual Studio 2022 감지됨 (권장)")
        elseif(MSVC_VERSION GREATER_EQUAL 1920)
            message(STATUS "Visual Studio 2019 감지됨 (지원됨)")
        else()
            message(WARNING "Visual Studio 2019 이상을 권장합니다.")
        endif()
    endif()

    # Windows SDK 버전 확인
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_GREATER_EQUAL "10.0.19041")
            message(STATUS "Windows SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} (권장)")
        else()
            message(WARNING "Windows SDK 10.0.19041 이상을 권장합니다.")
        endif()
    endif()

    # CMake 버전 확인
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        message(STATUS "CMake ${CMAKE_VERSION} (지원됨)")
    else()
        message(WARNING "CMake 3.16 이상을 권장합니다.")
    endif()

    message(STATUS "Windows 개발 환경 검증 완료")
endfunction()

# Windows 특화 테스트 설정
function(configure_windows_testing)
    if(BUILD_TESTING)
        message(STATUS "Windows 테스트 환경 설정 중...")

        # CTest 설정
        include(CTest)

        # Windows 특화 테스트 옵션
        set(CTEST_CONFIGURATION_TYPE ${CMAKE_BUILD_TYPE})

        # 테스트 실행 환경 설정
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--tool=memcheck --leak-check=full")
        endif()

        message(STATUS "Windows 테스트 환경 설정 완료")
    endif()
endfunction()

# Windows 패키징 설정
function(configure_windows_packaging)
    message(STATUS "Windows 패키징 설정 중...")

    # CPack 설정
    set(CPACK_GENERATOR "NSIS;ZIP" PARENT_SCOPE)
    set(CPACK_PACKAGE_NAME "LibEtude" PARENT_SCOPE)
    set(CPACK_PACKAGE_VENDOR "LibEtude Project" PARENT_SCOPE)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AI Voice Synthesis Engine" PARENT_SCOPE)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION} PARENT_SCOPE)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "LibEtude" PARENT_SCOPE)

    # NSIS 특화 설정
    set(CPACK_NSIS_DISPLAY_NAME "LibEtude ${PROJECT_VERSION}" PARENT_SCOPE)
    set(CPACK_NSIS_HELP_LINK "https://github.com/libetude/libetude" PARENT_SCOPE)
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/libetude/libetude" PARENT_SCOPE)
    set(CPACK_NSIS_CONTACT "support@libetude.org" PARENT_SCOPE)
    set(CPACK_NSIS_MODIFY_PATH ON PARENT_SCOPE)

    message(STATUS "Windows 패키징 설정 완료")
endfunction()