# LibEtude Windows 개발 도구 통합 예제

cmake_minimum_required(VERSION 3.16)

# Windows에서만 빌드
if(NOT WIN32)
    message(STATUS "Windows 개발 도구 예제는 Windows에서만 빌드됩니다.")
    return()
endif()

# 예제 실행 파일 생성
add_executable(windows_development_tools_example
    main.c
)

# LibEtude 라이브러리 링크 (가정)
if(TARGET libetude)
    target_link_libraries(windows_development_tools_example PRIVATE libetude)
else()
    # 라이브러리가 없는 경우 필요한 소스 파일들을 직접 포함
    target_sources(windows_development_tools_example PRIVATE
        ../../src/platform/windows/windows_etw.c
        ../../src/platform/windows/windows_debug.c
    )

    # 헤더 파일 경로 추가
    target_include_directories(windows_development_tools_example PRIVATE
        ../../include
    )
endif()

# Windows 특화 빌드 설정 적용
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/WindowsConfig.cmake")
    include(${CMAKE_SOURCE_DIR}/cmake/WindowsConfig.cmake)
    configure_windows_build(windows_development_tools_example)
endif()

# 컴파일러별 설정
if(MSVC)
    target_compile_options(windows_development_tools_example PRIVATE
        /W4
        /wd4996  # deprecated 함수 경고 비활성화
        /D_CRT_SECURE_NO_WARNINGS
        /DWIN32_LEAN_AND_MEAN
        /DNOMINMAX
        /utf-8
    )

    # Debug 모드에서 PDB 파일 생성
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(windows_development_tools_example PRIVATE /Zi)
        target_link_options(windows_development_tools_example PRIVATE /DEBUG:FULL)
    endif()
elseif(MINGW OR CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(windows_development_tools_example PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -D_WIN32_WINNT=0x0A00
    )
endif()

# Windows 시스템 라이브러리 링크
target_link_libraries(windows_development_tools_example PRIVATE
    # 기본 Windows 라이브러리
    kernel32
    user32
    advapi32

    # ETW 관련 라이브러리
    tdh

    # 디버깅 관련 라이브러리
    dbghelp
    psapi
    version
    imagehlp

    # 오디오 관련 라이브러리 (시뮬레이션용)
    winmm
    dsound
    mmdevapi
)

# 실행 파일 출력 디렉토리 설정
set_target_properties(windows_development_tools_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
    OUTPUT_NAME "windows_dev_tools_example"
)

# 설치 설정
install(TARGETS windows_development_tools_example
    RUNTIME DESTINATION examples
    COMPONENT Examples
)

# 예제 설명 파일 설치
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION examples/windows_development_tools
        COMPONENT Examples
    )
endif()

# Visual Studio에서 프로젝트 폴더 설정
if(MSVC)
    set_target_properties(windows_development_tools_example PROPERTIES
        FOLDER "Examples/Windows"
    )
endif()

message(STATUS "Windows 개발 도구 통합 예제 설정 완료")