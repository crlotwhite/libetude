# iOS CMake 툴체인 파일
# Copyright (c) 2025 LibEtude Project
# Based on ios-cmake by leetal (https://github.com/leetal/ios-cmake)

# 플랫폼 설정
set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS deployment version")

# 아키텍처 설정
if(NOT DEFINED PLATFORM)
    set(PLATFORM "OS64")
endif()

# 플랫폼별 설정
if(PLATFORM STREQUAL "OS64")
    # iOS 디바이스 (arm64)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
    set(LIBETUDE_IOS_DEVICE ON)
elseif(PLATFORM STREQUAL "SIMULATOR64")
    # iOS 시뮬레이터 (x86_64)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
    set(LIBETUDE_IOS_SIMULATOR ON)
elseif(PLATFORM STREQUAL "SIMULATORARM64")
    # iOS 시뮬레이터 (arm64)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
    set(LIBETUDE_IOS_SIMULATOR ON)
else()
    message(FATAL_ERROR "지원하지 않는 iOS 플랫폼: ${PLATFORM}")
endif()

# Xcode 경로 찾기
execute_process(
    COMMAND xcode-select -print-path
    OUTPUT_VARIABLE XCODE_DEVELOPER_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT EXISTS ${XCODE_DEVELOPER_DIR})
    message(FATAL_ERROR "Xcode를 찾을 수 없습니다. xcode-select --install을 실행하세요.")
endif()

# SDK 경로 설정
if(LIBETUDE_IOS_DEVICE)
    set(CMAKE_OSX_SYSROOT "${XCODE_DEVELOPER_DIR}/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk")
else()
    set(CMAKE_OSX_SYSROOT "${XCODE_DEVELOPER_DIR}/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk")
endif()

# SDK 존재 확인
if(NOT EXISTS ${CMAKE_OSX_SYSROOT})
    message(FATAL_ERROR "iOS SDK를 찾을 수 없습니다: ${CMAKE_OSX_SYSROOT}")
endif()

# 컴파일러 설정
set(CMAKE_C_COMPILER "${XCODE_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang")
set(CMAKE_CXX_COMPILER "${XCODE_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++")

# 컴파일러 플래그
set(CMAKE_C_FLAGS_INIT "-fvisibility=hidden")
set(CMAKE_CXX_FLAGS_INIT "-fvisibility=hidden -fvisibility-inlines-hidden")

# 링커 플래그
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-dead_strip")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-Wl,-dead_strip")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-dead_strip")

# 비트코드 설정 (기본적으로 비활성화)
if(NOT DEFINED ENABLE_BITCODE)
    set(ENABLE_BITCODE OFF)
endif()

if(ENABLE_BITCODE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fembed-bitcode")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fembed-bitcode")
endif()

# ARC 설정
if(NOT DEFINED ENABLE_ARC)
    set(ENABLE_ARC ON)
endif()

if(ENABLE_ARC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc")
endif()

# 크로스 컴파일 설정
set(CMAKE_CROSSCOMPILING ON)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# iOS 특화 정의
add_definitions(-DLIBETUDE_PLATFORM_IOS)
if(LIBETUDE_IOS_DEVICE)
    add_definitions(-DLIBETUDE_IOS_DEVICE)
else()
    add_definitions(-DLIBETUDE_IOS_SIMULATOR)
endif()

# NEON 지원 (디바이스에서만)
if(LIBETUDE_IOS_DEVICE)
    add_definitions(-DLIBETUDE_HAVE_NEON)
endif()

# 프레임워크 찾기 함수
function(find_ios_framework FRAMEWORK_NAME FRAMEWORK_PATH)
    find_library(${FRAMEWORK_NAME}_FRAMEWORK
        NAMES ${FRAMEWORK_NAME}
        PATHS ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks
        NO_DEFAULT_PATH
    )
    if(${FRAMEWORK_NAME}_FRAMEWORK)
        set(${FRAMEWORK_PATH} ${${FRAMEWORK_NAME}_FRAMEWORK} PARENT_SCOPE)
    endif()
endfunction()

# 필수 iOS 프레임워크 찾기
find_ios_framework(Foundation FOUNDATION_FRAMEWORK)
find_ios_framework(UIKit UIKIT_FRAMEWORK)
find_ios_framework(AVFoundation AVFOUNDATION_FRAMEWORK)
find_ios_framework(AudioToolbox AUDIOTOOLBOX_FRAMEWORK)

# 프레임워크 존재 확인
if(NOT FOUNDATION_FRAMEWORK OR NOT UIKIT_FRAMEWORK OR NOT AVFOUNDATION_FRAMEWORK OR NOT AUDIOTOOLBOX_FRAMEWORK)
    message(FATAL_ERROR "필수 iOS 프레임워크를 찾을 수 없습니다.")
endif()

message(STATUS "iOS 툴체인 설정 완료")
message(STATUS "플랫폼: ${PLATFORM}")
message(STATUS "아키텍처: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "SDK: ${CMAKE_OSX_SYSROOT}")
message(STATUS "최소 배포 버전: ${CMAKE_OSX_DEPLOYMENT_TARGET}")