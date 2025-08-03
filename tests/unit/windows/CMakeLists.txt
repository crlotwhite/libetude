# Windows 플랫폼 테스트 CMake 설정
cmake_minimum_required(VERSION 3.16)
project(WindowsPlatformTest)

# Windows에서만 빌드
if(NOT WIN32)
    return()
endif()

# 간단한 Windows 플랫폼 테스트
add_executable(windows_platform_simple_test
    test_windows_platform_simple_en.c
    ../../../src/platform/windows/windows_platform_minimal.c
)

target_include_directories(windows_platform_simple_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(windows_platform_simple_test PRIVATE
    kernel32 user32 ole32 oleaut32 uuid
)

# WASAPI 간단한 테스트
add_executable(wasapi_simple_test
    test_wasapi_simple.c
    ../../../src/platform/windows/windows_audio_wasapi.c
    ../../../src/platform/windows/windows_platform_minimal.c
)

target_include_directories(wasapi_simple_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(wasapi_simple_test PRIVATE
    kernel32 user32 ole32 oleaut32 uuid
    dsound winmm ksuser avrt
)

# 테스트 등록
add_test(NAME WindowsPlatformSimpleTest COMMAND windows_platform_simple_test)
add_test(NAME WASAPISimpleTest COMMAND wasapi_simple_test)