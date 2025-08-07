# LibEtude 크로스 컴파일 지원 설정
# Copyright (c) 2025 LibEtude Project

# 크로스 컴파일 환경 감지 및 설정
function(configure_cross_compilation)
    if(NOT CMAKE_CROSSCOMPILING)
        return()
    endif()

    message(STATUS "크로스 컴파일 환경 감지됨")
    message(STATUS "호스트 시스템: ${CMAKE_HOST_SYSTEM_NAME}")
    message(STATUS "타겟 시스템: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "타겟 프로세서: ${CMAKE_SYSTEM_PROCESSOR}")

    # 크로스 컴파일 플래그 설정
    add_definitions(-DLIBETUDE_CROSS_COMPILING=1)

    # 타겟별 설정
    configure_target_specific_settings()

    # 의존성 경로 조정
    adjust_dependency_paths()

    # 테스트 실행 비활성화 (크로스 컴파일 시)
    set(CMAKE_CROSSCOMPILING_EMULATOR "" CACHE STRING "크로스 컴파일 에뮬레이터")
    if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
        message(STATUS "크로스 컴파일 환경에서 테스트 실행이 비활성화됩니다")
        set(LIBETUDE_ENABLE_TESTS OFF CACHE BOOL "테스트 비활성화" FORCE)
    endif()
endfunction()

# 타겟별 특화 설정
function(configure_target_specific_settings)
    # Android 크로스 컴파일
    if(ANDROID)
        configure_android_cross_compile()
    # iOS 크로스 컴파일
    elseif(IOS)
        configure_ios_cross_compile()
    # ARM Linux 크로스 컴파일
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        configure_arm_linux_cross_compile()
    # Windows ARM 크로스 컴파일
    elseif(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        configure_windows_arm_cross_compile()
    # 임베디드 시스템
    elseif(CMAKE_SYSTEM_NAME MATCHES "Generic|FreeRTOS|Zephyr")
        configure_embedded_cross_compile()
    endif()
endfunction()

# Android 크로스 컴파일 설정
function(configure_android_cross_compile)
    message(STATUS "Android 크로스 컴파일 설정 중...")

    # Android NDK 확인
    if(NOT ANDROID_NDK)
        message(FATAL_ERROR "ANDROID_NDK 경로가 설정되지 않았습니다")
    endif()

    # Android API 레벨 확인
    if(NOT ANDROID_PLATFORM)
        set(ANDROID_PLATFORM android-21)  # 기본값
        message(STATUS "Android API 레벨을 ${ANDROID_PLATFORM}로 설정")
    endif()

    # Android ABI 확인
    if(NOT ANDROID_ABI)
        set(ANDROID_ABI arm64-v8a)  # 기본값
        message(STATUS "Android ABI를 ${ANDROID_ABI}로 설정")
    endif()

    # Android 특화 컴파일 플래그
    add_definitions(-DLIBETUDE_PLATFORM_ANDROID=1)
    add_definitions(-DANDROID_NDK_VERSION=${ANDROID_NDK_REVISION})

    # 최소 기능으로 빌드 (모바일 최적화)
    set(LIBETUDE_MINIMAL ON CACHE BOOL "Android 최소 빌드" FORCE)
    set(LIBETUDE_ENABLE_GPU OFF CACHE BOOL "Android GPU 비활성화" FORCE)

    message(STATUS "Android 설정 완료: API ${ANDROID_PLATFORM}, ABI ${ANDROID_ABI}")
endfunction()

# iOS 크로스 컴파일 설정
function(configure_ios_cross_compile)
    message(STATUS "iOS 크로스 컴파일 설정 중...")

    # iOS 플랫폼 확인
    if(NOT PLATFORM)
        set(PLATFORM OS64)  # 기본값: iOS 64비트
        message(STATUS "iOS 플랫폼을 ${PLATFORM}으로 설정")
    endif()

    # iOS 배포 타겟 확인
    if(NOT IPHONEOS_DEPLOYMENT_TARGET)
        set(IPHONEOS_DEPLOYMENT_TARGET 12.0)
        message(STATUS "iOS 배포 타겟을 ${IPHONEOS_DEPLOYMENT_TARGET}으로 설정")
    endif()

    # iOS 특화 설정
    add_definitions(-DLIBETUDE_PLATFORM_IOS=1)

    # 코드 서명 설정 (필요시)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
    endif()

    # 최소 기능으로 빌드
    set(LIBETUDE_MINIMAL ON CACHE BOOL "iOS 최소 빌드" FORCE)

    message(STATUS "iOS 설정 완료: ${PLATFORM}, 배포 타겟 ${IPHONEOS_DEPLOYMENT_TARGET}")
endfunction()

# ARM Linux 크로스 컴파일 설정
function(configure_arm_linux_cross_compile)
    message(STATUS "ARM Linux 크로스 컴파일 설정 중...")

    # ARM 아키텍처별 설정
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        add_definitions(-DLIBETUDE_ARCH_ARM64=1)
        set(ARM_ARCH "ARM64")

        # ARM64 최적화 플래그
        if(NOT LIBETUDE_MINIMAL)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a -mtune=cortex-a72")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -mtune=cortex-a72")
        endif()

    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        add_definitions(-DLIBETUDE_ARCH_ARM=1)
        set(ARM_ARCH "ARM32")

        # ARM32 최적화 플래그
        if(NOT LIBETUDE_MINIMAL)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
        endif()
    endif()

    # NEON SIMD 지원 활성화
    if(LIBETUDE_ENABLE_SIMD)
        add_definitions(-DLIBETUDE_HAVE_NEON=1)
        message(STATUS "ARM NEON SIMD 지원 활성화")
    endif()

    message(STATUS "ARM Linux 설정 완료: ${ARM_ARCH}")
endfunction()

# Windows ARM 크로스 컴파일 설정
function(configure_windows_arm_cross_compile)
    message(STATUS "Windows ARM 크로스 컴파일 설정 중...")

    # Windows ARM 특화 설정
    add_definitions(-DLIBETUDE_PLATFORM_WINDOWS=1)

    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        add_definitions(-DLIBETUDE_ARCH_ARM64=1)
        set(WIN_ARM_ARCH "ARM64")
    else()
        add_definitions(-DLIBETUDE_ARCH_ARM=1)
        set(WIN_ARM_ARCH "ARM32")
    endif()

    # Windows ARM 런타임 라이브러리
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    message(STATUS "Windows ARM 설정 완료: ${WIN_ARM_ARCH}")
endfunction()

# 임베디드 시스템 크로스 컴파일 설정
function(configure_embedded_cross_compile)
    message(STATUS "임베디드 시스템 크로스 컴파일 설정 중...")

    # 임베디드 특화 설정
    add_definitions(-DLIBETUDE_PLATFORM_EMBEDDED=1)

    # 최소 기능으로 강제 설정
    set(LIBETUDE_MINIMAL ON CACHE BOOL "임베디드 최소 빌드" FORCE)
    set(LIBETUDE_ENABLE_GPU OFF CACHE BOOL "임베디드 GPU 비활성화" FORCE)
    set(LIBETUDE_ENABLE_NETWORK_ABSTRACTION OFF CACHE BOOL "임베디드 네트워크 비활성화" FORCE)
    set(LIBETUDE_BUILD_EXAMPLES OFF CACHE BOOL "임베디드 예제 비활성화" FORCE)
    set(LIBETUDE_BUILD_TOOLS OFF CACHE BOOL "임베디드 도구 비활성화" FORCE)

    # 메모리 제약 설정
    add_definitions(-DLIBETUDE_MAX_MEMORY_MB=64)  # 64MB 제한
    add_definitions(-DLIBETUDE_STACK_SIZE_KB=32)  # 32KB 스택

    # 부동소수점 설정 확인
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        # ARM Cortex-M 시리즈는 하드웨어 FPU가 없을 수 있음
        option(LIBETUDE_USE_SOFT_FLOAT "소프트웨어 부동소수점 사용" OFF)
        if(LIBETUDE_USE_SOFT_FLOAT)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=soft")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=soft")
            add_definitions(-DLIBETUDE_SOFT_FLOAT=1)
        endif()
    endif()

    message(STATUS "임베디드 시스템 설정 완료")
endfunction()

# 의존성 경로 조정
function(adjust_dependency_paths)
    # 크로스 컴파일 시 호스트 도구와 타겟 라이브러리 분리
    if(CMAKE_CROSSCOMPILING)
        # 타겟 시스템의 라이브러리 검색 경로 설정
        if(CMAKE_FIND_ROOT_PATH)
            set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
            set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
            set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
            set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

            message(STATUS "크로스 컴파일 루트 경로: ${CMAKE_FIND_ROOT_PATH}")
        endif()

        # pkg-config 경로 조정
        if(PKG_CONFIG_EXECUTABLE)
            set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_FIND_ROOT_PATH}/lib/pkgconfig:${CMAKE_FIND_ROOT_PATH}/share/pkgconfig")
            set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_FIND_ROOT_PATH}")
        endif()
    endif()
endfunction()

# 크로스 컴파일 검증
function(validate_cross_compile_setup)
    if(NOT CMAKE_CROSSCOMPILING)
        return()
    endif()

    message(STATUS "크로스 컴파일 설정 검증 중...")

    # 필수 도구 확인
    if(NOT CMAKE_C_COMPILER)
        message(FATAL_ERROR "C 컴파일러가 설정되지 않았습니다")
    endif()

    if(NOT CMAKE_CXX_COMPILER)
        message(FATAL_ERROR "C++ 컴파일러가 설정되지 않았습니다")
    endif()

    # 컴파일러 테스트
    try_compile(COMPILER_TEST_RESULT
        ${CMAKE_BINARY_DIR}/compiler_test
        ${CMAKE_SOURCE_DIR}/cmake/test_compiler.c
        OUTPUT_VARIABLE COMPILER_TEST_OUTPUT
    )

    if(NOT COMPILER_TEST_RESULT)
        message(FATAL_ERROR "크로스 컴파일러 테스트 실패: ${COMPILER_TEST_OUTPUT}")
    endif()

    message(STATUS "크로스 컴파일 설정 검증 완료")
endfunction()

# 크로스 컴파일 도구체인 파일 생성
function(generate_toolchain_file PLATFORM_NAME OUTPUT_FILE)
    set(TOOLCHAIN_CONTENT "# LibEtude ${PLATFORM_NAME} 크로스 컴파일 도구체인\n")
    set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}# 자동 생성됨 - 수정하지 마세요\n\n")

    # 플랫폼별 도구체인 설정
    if(PLATFORM_NAME STREQUAL "Android")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_SYSTEM_NAME Android)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_SYSTEM_VERSION 21)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_ANDROID_NDK \$ENV{ANDROID_NDK})\n")

    elseif(PLATFORM_NAME STREQUAL "iOS")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_SYSTEM_NAME iOS)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_OSX_ARCHITECTURES arm64)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)\n")

    elseif(PLATFORM_NAME STREQUAL "ARM64-Linux")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_SYSTEM_NAME Linux)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_SYSTEM_PROCESSOR aarch64)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)\n")
        set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)\n")
    endif()

    # 공통 설정
    set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}\n# LibEtude 특화 설정\n")
    set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(LIBETUDE_CROSS_COMPILING ON)\n")
    set(TOOLCHAIN_CONTENT "${TOOLCHAIN_CONTENT}set(LIBETUDE_TARGET_PLATFORM ${PLATFORM_NAME})\n")

    # 파일 작성
    file(WRITE ${OUTPUT_FILE} ${TOOLCHAIN_CONTENT})
    message(STATUS "${PLATFORM_NAME} 도구체인 파일 생성: ${OUTPUT_FILE}")
endfunction()

# 메인 크로스 컴파일 설정 함수
function(setup_cross_compilation)
    configure_cross_compilation()
    validate_cross_compile_setup()

    # 크로스 컴파일 정보 출력
    if(CMAKE_CROSSCOMPILING)
        message(STATUS "=== 크로스 컴파일 정보 ===")
        message(STATUS "타겟 시스템: ${CMAKE_SYSTEM_NAME}")
        message(STATUS "타겟 프로세서: ${CMAKE_SYSTEM_PROCESSOR}")
        message(STATUS "C 컴파일러: ${CMAKE_C_COMPILER}")
        message(STATUS "C++ 컴파일러: ${CMAKE_CXX_COMPILER}")
        if(CMAKE_FIND_ROOT_PATH)
            message(STATUS "루트 경로: ${CMAKE_FIND_ROOT_PATH}")
        endif()
        message(STATUS "========================")
    endif()
endfunction()