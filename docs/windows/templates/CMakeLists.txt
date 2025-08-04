cmake_minimum_required(VERSION 3.16)
project($safeprojectname$ VERSION 1.0.0 LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 빌드 타입 기본값 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# LibEtude 패키지 찾기
find_package(LibEtude REQUIRED)

# 실행 파일 생성
add_executable(${PROJECT_NAME} main.cpp)

# LibEtude 링크
target_link_libraries(${PROJECT_NAME} PRIVATE LibEtude::LibEtude)

# Windows 특화 설정
if(WIN32)
    # Windows 서브시스템 설정 (콘솔 애플리케이션)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )

    # Windows API 라이브러리 링크
    target_link_libraries(${PROJECT_NAME} PRIVATE
        kernel32    # Windows 커널 API
        user32      # Windows 사용자 API
        ole32       # OLE/COM API
        oleaut32    # OLE Automation API
        uuid        # UUID 라이브러리
        dsound      # DirectSound API
        winmm       # Windows 멀티미디어 API
        ksuser      # Kernel Streaming API
    )

    # MSVC 컴파일러 최적화 설정
    if(MSVC)
        # Release 빌드 최적화
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /O2         # 최대 속도 최적화
                /Oi         # 내장 함수 사용
                /Ot         # 속도 우선 최적화
                /Oy         # 프레임 포인터 생략
                /GL         # 전체 프로그램 최적화
                /arch:AVX2  # AVX2 명령어 사용
                /fp:fast    # 빠른 부동소수점 연산
            >
            $<$<CONFIG:Debug>:
                /Zi         # 디버그 정보 생성
                /Od         # 최적화 비활성화
            >
        )

        # 링커 최적화 설정
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /LTCG       # 링크 타임 코드 생성
                /OPT:REF    # 참조되지 않는 함수 제거
                /OPT:ICF    # 동일한 함수 병합
            >
            $<$<CONFIG:Debug>:
                /DEBUG:FULL # 전체 디버그 정보
            >
        )

        # 경고 레벨 설정
        target_compile_options(${PROJECT_NAME} PRIVATE /W4)

        # 특정 경고 비활성화
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            _CRT_SECURE_NO_WARNINGS     # CRT 보안 경고 비활성화
            WIN32_LEAN_AND_MEAN         # Windows 헤더 최소화
            NOMINMAX                    # min/max 매크로 비활성화
        )
    endif()

    # MinGW 컴파일러 설정
    if(MINGW)
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                -O3                     # 최고 수준 최적화
                -march=native           # 현재 CPU에 최적화
                -mtune=native           # 현재 CPU 튜닝
                -ffast-math             # 빠른 수학 연산
                -funroll-loops          # 루프 언롤링
                -fomit-frame-pointer    # 프레임 포인터 생략
            >
            $<$<CONFIG:Debug>:
                -g                      # 디버그 정보 생성
                -O0                     # 최적화 비활성화
            >
        )

        # 경고 설정
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()

    # Windows 버전 정의
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WINVER=0x0A00           # Windows 10
        _WIN32_WINNT=0x0A00     # Windows 10
    )

    # 유니코드 지원
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE _UNICODE
    )
endif()

# 일반적인 컴파일러 설정
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# 디버그 빌드에서 추가 검사 활성화
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        DEBUG _DEBUG
    )

    # MSVC에서 런타임 검사 활성화
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            /RTC1       # 런타임 오류 검사
        )
    endif()
endif()

# Release 빌드에서 NDEBUG 정의
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NDEBUG
    )
endif()

# 프로젝트 정보 설정
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "LibEtude Windows Application"
)

# 설치 설정 (선택사항)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# CPack 설정 (패키징용, 선택사항)
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LibEtude Windows Application")
set(CPACK_PACKAGE_VENDOR "LibEtude Project")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")
endif()

include(CPack)

# 빌드 정보 출력
message(STATUS "프로젝트: ${PROJECT_NAME}")
message(STATUS "버전: ${PROJECT_VERSION}")
message(STATUS "빌드 타입: ${CMAKE_BUILD_TYPE}")
message(STATUS "컴파일러: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ 표준: ${CMAKE_CXX_STANDARD}")

if(WIN32)
    message(STATUS "Windows 플랫폼 감지됨")
    if(MSVC)
        message(STATUS "MSVC 컴파일러 사용")
    elseif(MINGW)
        message(STATUS "MinGW 컴파일러 사용")
    endif()
endif()