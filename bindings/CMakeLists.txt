# LibEtude 언어 바인딩 빌드 설정

cmake_minimum_required(VERSION 3.12)

# C++ 바인딩 빌드
option(LIBETUDE_BUILD_CPP_BINDINGS "Build C++ bindings" ON)
if(LIBETUDE_BUILD_CPP_BINDINGS)
    add_subdirectory(cpp)
endif()

# JNI 바인딩 빌드 (Android)
option(LIBETUDE_BUILD_JNI_BINDINGS "Build JNI bindings for Android" OFF)
if(LIBETUDE_BUILD_JNI_BINDINGS)
    find_package(JNI REQUIRED)
    if(JNI_FOUND)
        add_subdirectory(jni)
    else()
        message(WARNING "JNI not found, skipping JNI bindings")
    endif()
endif()

# Objective-C 바인딩 빌드 (iOS/macOS)
option(LIBETUDE_BUILD_OBJC_BINDINGS "Build Objective-C bindings for iOS/macOS" OFF)
if(LIBETUDE_BUILD_OBJC_BINDINGS AND APPLE)
    add_subdirectory(objc)
endif()

# Python 바인딩 빌드 (선택적)
option(LIBETUDE_BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
if(LIBETUDE_BUILD_PYTHON_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    add_subdirectory(python)
endif()

# 바인딩 테스트
if(BUILD_TESTING)
    # C++ 바인딩 테스트는 tests/unit에서 처리
endif()