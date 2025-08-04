cmake_minimum_required(VERSION 3.16)
project(WindowsSecurityTest C)

# C99 표준 설정
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows에서만 빌드
if(WIN32)
    add_executable(test_windows_security_simple test_windows_security_simple.c)

    # Windows 라이브러리 링크
    target_link_libraries(test_windows_security_simple kernel32 user32)

    # 컴파일러 설정
    if(MSVC)
        target_compile_options(test_windows_security_simple PRIVATE /W3)
    endif()
else()
    message(STATUS "This test is only for Windows platform")
endif()