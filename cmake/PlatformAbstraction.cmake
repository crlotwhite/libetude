# LibEtude 플랫폼 추상화 레이어 CMake 설정
# Copyright (c) 2025 LibEtude Project

# 플랫폼 추상화 레이어 옵션
option(LIBETUDE_ENABLE_PLATFORM_ABSTRACTION "Enable platform abstraction layer" ON)
option(LIBETUDE_PLATFORM_STATIC_DISPATCH "Use static dispatch for better performance" ON)
option(LIBETUDE_PLATFORM_RUNTIME_DETECTION "Enable runtime platform feature detection" ON)

# 플랫폼별 기능 옵션
option(LIBETUDE_ENABLE_AUDIO_ABSTRACTION "Enable audio I/O abstraction" ON)
option(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION "Enable system info abstraction" ON)
option(LIBETUDE_ENABLE_THREADING_ABSTRACTION "Enable threading abstraction" ON)
option(LIBETUDE_ENABLE_MEMORY_ABSTRACTION "Enable memory management abstraction" ON)
option(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION "Enable filesystem abstraction" ON)
option(LIBETUDE_ENABLE_NETWORK_ABSTRACTION "Enable network abstraction" ON)
option(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION "Enable dynamic library abstraction" ON)

# 플랫폼 감지 및 설정
function(configure_platform_abstraction)
    if(NOT LIBETUDE_ENABLE_PLATFORM_ABSTRACTION)
        message(STATUS "플랫폼 추상화 레이어가 비활성화되었습니다")
        return()
    endif()

    message(STATUS "플랫폼 추상화 레이어 설정 중...")

    # 플랫폼별 매크로 정의
    if(WIN32)
        add_definitions(-DLIBETUDE_PLATFORM_WINDOWS=1)
        set(PLATFORM_NAME "Windows")
        set(PLATFORM_SOURCES_DIR "windows")

        # Windows 특화 설정
        add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 이상
        add_definitions(-DWINVER=0x0601)
        add_definitions(-DNOMINMAX)  # min/max 매크로 충돌 방지

    elseif(APPLE)
        if(IOS)
            add_definitions(-DLIBETUDE_PLATFORM_IOS=1)
            set(PLATFORM_NAME "iOS")
            set(PLATFORM_SOURCES_DIR "ios")
        else()
            add_definitions(-DLIBETUDE_PLATFORM_MACOS=1)
            set(PLATFORM_NAME "macOS")
            set(PLATFORM_SOURCES_DIR "macos")

            # macOS 특화 설정
            find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
            find_library(COREAUDIO_FRAMEWORK CoreAudio)
            find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
            find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
        endif()

    elseif(ANDROID)
        add_definitions(-DLIBETUDE_PLATFORM_ANDROID=1)
        set(PLATFORM_NAME "Android")
        set(PLATFORM_SOURCES_DIR "android")

    elseif(UNIX)
        add_definitions(-DLIBETUDE_PLATFORM_LINUX=1)
        set(PLATFORM_NAME "Linux")
        set(PLATFORM_SOURCES_DIR "linux")

        # Linux 특화 설정
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(ALSA alsa)
        endif()
    endif()

    # 아키텍처 감지
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_definitions(-DLIBETUDE_ARCH_X64=1)
        set(ARCH_NAME "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
        add_definitions(-DLIBETUDE_ARCH_X86=1)
        set(ARCH_NAME "x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        add_definitions(-DLIBETUDE_ARCH_ARM64=1)
        set(ARCH_NAME "ARM64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        add_definitions(-DLIBETUDE_ARCH_ARM=1)
        set(ARCH_NAME "ARM")
    endif()

    # 플랫폼 추상화 기능별 매크로 정의
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
        add_definitions(-DLIBETUDE_AUDIO_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
        add_definitions(-DLIBETUDE_SYSTEM_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
        add_definitions(-DLIBETUDE_THREADING_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
        add_definitions(-DLIBETUDE_MEMORY_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
        add_definitions(-DLIBETUDE_FILESYSTEM_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
        add_definitions(-DLIBETUDE_NETWORK_ABSTRACTION_ENABLED=1)
    endif()

    if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
        add_definitions(-DLIBETUDE_DYNLIB_ABSTRACTION_ENABLED=1)
    endif()

    # 정적/동적 디스패치 설정
    if(LIBETUDE_PLATFORM_STATIC_DISPATCH)
        add_definitions(-DLIBETUDE_PLATFORM_STATIC_DISPATCH=1)
        message(STATUS "정적 디스패치 활성화 (성능 최적화)")
    else()
        add_definitions(-DLIBETUDE_PLATFORM_STATIC_DISPATCH=0)
        message(STATUS "동적 디스패치 활성화 (유연성 우선)")
    endif()

    # 런타임 기능 감지 설정
    if(LIBETUDE_PLATFORM_RUNTIME_DETECTION)
        add_definitions(-DLIBETUDE_PLATFORM_RUNTIME_DETECTION=1)
        message(STATUS "런타임 기능 감지 활성화")
    else()
        add_definitions(-DLIBETUDE_PLATFORM_RUNTIME_DETECTION=0)
    endif()

    message(STATUS "플랫폼: ${PLATFORM_NAME} (${ARCH_NAME})")

    # 전역 변수로 설정 저장
    set(LIBETUDE_PLATFORM_NAME ${PLATFORM_NAME} PARENT_SCOPE)
    set(LIBETUDE_PLATFORM_SOURCES_DIR ${PLATFORM_SOURCES_DIR} PARENT_SCOPE)
    set(LIBETUDE_ARCH_NAME ${ARCH_NAME} PARENT_SCOPE)
endfunction()

# 플랫폼별 소스 파일 수집 함수
function(collect_platform_sources OUTPUT_VAR)
    set(PLATFORM_SOURCES)

    # 공통 플랫폼 소스
    list(APPEND PLATFORM_SOURCES
        src/platform/common.c
        src/platform/factory.c
        src/platform/platform.c
        src/platform/platform_core.c
        src/platform/platform_utils.c
    )

    # 기능별 공통 소스
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/audio_common.c)
    endif()

    if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/system_common.c)
    endif()

    if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/threading_common.c)
    endif()

    if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/memory_common.c)
    endif()

    if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/filesystem_common.c)
    endif()

    if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES src/platform/network_common.c)
    endif()

    if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
        list(APPEND PLATFORM_SOURCES
            src/platform/dynlib_common.c
            src/platform/dynlib_error_mapping.c
        )
    endif()

    # 플랫폼별 소스 파일 추가
    if(WIN32)
        # Windows 특화 소스
        list(APPEND PLATFORM_SOURCES
            src/platform/windows/factory_windows.c
            src/platform/windows/platform_init.c
            src/platform/windows/windows_utils.c
        )

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/windows_audio.c)
        endif()

        if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/windows_system.c)
        endif()

        if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/threading_windows.c)
        endif()

        if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/memory_windows.c)
        endif()

        if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/filesystem_windows.c)
        endif()

        if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/network_windows.c)
        endif()

        if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/windows/dynlib_windows.c)
        endif()

    elseif(APPLE)
        # macOS/iOS 특화 소스
        list(APPEND PLATFORM_SOURCES
            src/platform/macos/factory_macos.c
            src/platform/macos/platform_init.c
            src/platform/macos/macos_utils.c
            src/platform/macos/macos_compat.c
        )

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/macos/macos_audio.c)
        endif()

        if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/macos/macos_system.c)
        endif()

        if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/macos/network_macos.c)
        endif()

        # POSIX 공통 소스 추가
        if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/threading_posix.c)
        endif()

        if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/memory_posix.c)
        endif()

        if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/filesystem_posix.c)
        endif()

        if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/dynlib_posix.c)
        endif()

    elseif(UNIX)
        # Linux 특화 소스
        list(APPEND PLATFORM_SOURCES
            src/platform/linux/factory_linux.c
            src/platform/linux/platform_init.c
            src/platform/linux/linux_utils.c
        )

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/linux/linux_audio.c)
        endif()

        if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/linux/linux_system.c)
        endif()

        if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/linux/network_linux.c)
        endif()

        # POSIX 공통 소스 추가
        if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/threading_posix.c)
        endif()

        if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/memory_posix.c)
        endif()

        if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/filesystem_posix.c)
        endif()

        if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
            list(APPEND PLATFORM_SOURCES src/platform/posix/dynlib_posix.c)
        endif()
    endif()

    # 최적화 및 고급 기능 소스
    list(APPEND PLATFORM_SOURCES
        src/platform/optimization.c
        src/platform/runtime_adaptation.c
        src/platform/desktop_optimization.c
        src/platform/embedded_optimization.c
        src/platform/mobile_optimization.c
        src/platform/mobile_power_management.c
        src/platform/thermal_management.c
    )

    set(${OUTPUT_VAR} ${PLATFORM_SOURCES} PARENT_SCOPE)
endfunction()

# 플랫폼별 라이브러리 링크 함수
function(link_platform_libraries TARGET_NAME)
    if(WIN32)
        # Windows 시스템 라이브러리
        target_link_libraries(${TARGET_NAME} PRIVATE
            kernel32
            user32
            winmm
            ws2_32
            psapi
            pdh
        )

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE
                dsound
                ole32
                oleaut32
            )
        endif()

    elseif(APPLE)
        # macOS/iOS 프레임워크
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${COREFOUNDATION_FRAMEWORK}
        )

        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE
                ${COREAUDIO_FRAMEWORK}
                ${AUDIOTOOLBOX_FRAMEWORK}
                ${AUDIOUNIT_FRAMEWORK}
            )
        endif()

        if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE pthread)
        endif()

        if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE dl)
        endif()

    elseif(UNIX)
        # Linux 시스템 라이브러리
        if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION AND ALSA_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE ${ALSA_LIBRARIES})
            target_include_directories(${TARGET_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
        endif()

        if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE pthread)
        endif()

        if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE dl)
        endif()

        if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
            target_link_libraries(${TARGET_NAME} PRIVATE rt)
        endif()

        # 수학 라이브러리
        target_link_libraries(${TARGET_NAME} PRIVATE m)
    endif()
endfunction()

# 플랫폼별 컴파일 옵션 설정 함수
function(set_platform_compile_options TARGET_NAME)
    if(WIN32)
        # Windows 특화 컴파일 옵션
        target_compile_definitions(${TARGET_NAME} PRIVATE
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )

        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE
                /wd4996  # deprecated function warnings 무시
                /wd4244  # conversion warnings 무시 (필요시)
            )
        endif()

    elseif(APPLE)
        # macOS/iOS 특화 컴파일 옵션
        target_compile_definitions(${TARGET_NAME} PRIVATE
            _DARWIN_C_SOURCE
        )

        # macOS 15+ SDK 호환성
        if(CMAKE_SYSTEM_VERSION VERSION_GREATER_EQUAL "15.0")
            target_compile_definitions(${TARGET_NAME} PRIVATE
                __BLOCKS__=0
                LIBETUDE_MACOS_BLOCKS_DISABLED=1
            )
            target_compile_options(${TARGET_NAME} PRIVATE
                -Wno-nullability-completeness
            )
        endif()

    elseif(UNIX)
        # Linux 특화 컴파일 옵션
        target_compile_definitions(${TARGET_NAME} PRIVATE
            _GNU_SOURCE
            _POSIX_C_SOURCE=200809L
        )
    endif()

    # 디버그 모드 설정
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${TARGET_NAME} PRIVATE
            LIBETUDE_PLATFORM_DEBUG=1
        )
    endif()
endfunction()