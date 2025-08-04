cmake_minimum_required(VERSION 3.16)
project(WindowsLargePagesTest C)

# C99 표준 설정
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows에서만 빌드
if(WIN32)
    # 헤더 파일 경로 설정
    include_directories(../../../include)

    # Large Page 테스트 실행 파일
    add_executable(test_windows_large_pages
        test_windows_large_pages.c
        ../../../src/platform/windows/windows_large_pages.c
    )

    # Windows 라이브러리 링크
    target_link_libraries(test_windows_large_pages kernel32 user32)

    # 컴파일러 설정
    if(MSVC)
        target_compile_options(test_windows_large_pages PRIVATE /W3)
        target_compile_definitions(test_windows_large_pages PRIVATE _WIN32 WIN32)
    endif()

    # 디버그 정보 포함
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(MSVC)
            target_compile_options(test_windows_large_pages PRIVATE /Zi)
        endif()
    endif()

else()
    message(STATUS "This test is only for Windows platform")
endif()