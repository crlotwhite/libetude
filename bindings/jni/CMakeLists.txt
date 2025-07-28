# LibEtude Android JNI 바인딩

cmake_minimum_required(VERSION 3.12)

# JNI 라이브러리 찾기
find_package(JNI REQUIRED)

# JNI 바인딩 소스 파일들
set(JNI_SOURCES
    src/libetude_jni.cpp
    src/engine_jni.cpp
    src/audio_jni.cpp
    src/utils_jni.cpp
)

# JNI 바인딩 라이브러리 생성
add_library(libetude_jni SHARED ${JNI_SOURCES})

# 컴파일 옵션 설정
target_compile_features(libetude_jni PRIVATE cxx_std_17)
target_compile_options(libetude_jni PRIVATE
    -Wall -Wextra
    -fvisibility=hidden
    -fno-exceptions
    -fno-rtti
)

# Android 특화 최적화
if(ANDROID)
    target_compile_definitions(libetude_jni PRIVATE
        ANDROID_PLATFORM
        LIBETUDE_MOBILE_OPTIMIZED
    )

    # Android NDK 로그 라이브러리 링크
    target_link_libraries(libetude_jni PRIVATE log)

    # ARM NEON 최적화 활성화
    if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
        target_compile_definitions(libetude_jni PRIVATE LIBETUDE_ARM_NEON)
    endif()
endif()

# 헤더 파일 경로 설정
target_include_directories(libetude_jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${JNI_INCLUDE_DIRS}
)

# LibEtude 코어 라이브러리 링크
target_link_libraries(libetude_jni PRIVATE
    libetude_core
    ${JNI_LIBRARIES}
)

# 출력 이름 설정
set_target_properties(libetude_jni PROPERTIES
    OUTPUT_NAME "etude"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/android/libs/${ANDROID_ABI}"
)

# 디버그 정보 제거 (릴리스 빌드)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(libetude_jni PRIVATE -s)
endif()

# 메모리 최적화 플래그
target_compile_definitions(libetude_jni PRIVATE
    LIBETUDE_MEMORY_OPTIMIZED
    LIBETUDE_MOBILE_MEMORY_LIMIT=128  # 128MB 메모리 제한
)