# LibEtude 버전 관리 및 호환성 검사 시스템
# Copyright (c) 2025 LibEtude Project

# 버전 정보 파싱
function(parse_version_string VERSION_STRING MAJOR_VAR MINOR_VAR PATCH_VAR)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH ${VERSION_STRING})
    if(VERSION_MATCH)
        set(${MAJOR_VAR} ${CMAKE_MATCH_1} PARENT_SCOPE)
        set(${MINOR_VAR} ${CMAKE_MATCH_2} PARENT_SCOPE)
        set(${PATCH_VAR} ${CMAKE_MATCH_3} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "잘못된 버전 형식: ${VERSION_STRING}")
    endif()
endfunction()

# 버전 비교 함수
function(compare_versions VERSION1 VERSION2 RESULT_VAR)
    parse_version_string(${VERSION1} V1_MAJOR V1_MINOR V1_PATCH)
    parse_version_string(${VERSION2} V2_MAJOR V2_MINOR V2_PATCH)

    # 주 버전 비교
    if(V1_MAJOR GREATER V2_MAJOR)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
        return()
    elseif(V1_MAJOR LESS V2_MAJOR)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
        return()
    endif()

    # 부 버전 비교
    if(V1_MINOR GREATER V2_MINOR)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
        return()
    elseif(V1_MINOR LESS V2_MINOR)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
        return()
    endif()

    # 패치 버전 비교
    if(V1_PATCH GREATER V2_PATCH)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
    elseif(V1_PATCH LESS V2_PATCH)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
    else()
        set(${RESULT_VAR} 0 PARENT_SCOPE)
    endif()
endfunction()

# 호환성 검사 함수
function(check_version_compatibility REQUIRED_VERSION CURRENT_VERSION COMPATIBLE_VAR)
    parse_version_string(${REQUIRED_VERSION} REQ_MAJOR REQ_MINOR REQ_PATCH)
    parse_version_string(${CURRENT_VERSION} CUR_MAJOR CUR_MINOR CUR_PATCH)

    # 주 버전이 다르면 호환되지 않음
    if(NOT REQ_MAJOR EQUAL CUR_MAJOR)
        set(${COMPATIBLE_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # 현재 버전이 요구 버전보다 낮으면 호환되지 않음
    compare_versions(${CURRENT_VERSION} ${REQUIRED_VERSION} COMPARISON)
    if(COMPARISON LESS 0)
        set(${COMPATIBLE_VAR} FALSE PARENT_SCOPE)
    else()
        set(${COMPATIBLE_VAR} TRUE PARENT_SCOPE)
    endif()
endfunction()

# Git 정보 수집
function(get_git_info)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # Git 커밋 해시
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        # Git 브랜치
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        # Git 태그
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        # 변경사항 확인
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --quiet
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE GIT_DIRTY
            ERROR_QUIET
        )

        # 전역 변수로 설정
        set(LIBETUDE_GIT_COMMIT_HASH ${GIT_COMMIT_HASH} PARENT_SCOPE)
        set(LIBETUDE_GIT_BRANCH ${GIT_BRANCH} PARENT_SCOPE)
        set(LIBETUDE_GIT_TAG ${GIT_TAG} PARENT_SCOPE)
        set(LIBETUDE_GIT_DIRTY ${GIT_DIRTY} PARENT_SCOPE)

        message(STATUS "Git 정보:")
        message(STATUS "  커밋: ${GIT_COMMIT_HASH}")
        message(STATUS "  브랜치: ${GIT_BRANCH}")
        if(GIT_TAG)
            message(STATUS "  태그: ${GIT_TAG}")
        endif()
        if(GIT_DIRTY)
            message(STATUS "  상태: 변경사항 있음")
        else()
            message(STATUS "  상태: 깨끗함")
        endif()
    else()
        set(LIBETUDE_GIT_COMMIT_HASH "unknown" PARENT_SCOPE)
        set(LIBETUDE_GIT_BRANCH "unknown" PARENT_SCOPE)
        set(LIBETUDE_GIT_TAG "" PARENT_SCOPE)
        set(LIBETUDE_GIT_DIRTY 0 PARENT_SCOPE)
    endif()
endfunction()

# 빌드 정보 생성
function(generate_build_info)
    # 빌드 시간
    string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

    # 컴파일러 정보
    set(COMPILER_INFO "${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

    # 플랫폼 정보
    set(PLATFORM_INFO "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} (${CMAKE_SYSTEM_PROCESSOR})")

    # 빌드 설정
    set(BUILD_CONFIG "${CMAKE_BUILD_TYPE}")

    # 전역 변수로 설정
    set(LIBETUDE_BUILD_TIMESTAMP ${BUILD_TIMESTAMP} PARENT_SCOPE)
    set(LIBETUDE_COMPILER_INFO ${COMPILER_INFO} PARENT_SCOPE)
    set(LIBETUDE_PLATFORM_INFO ${PLATFORM_INFO} PARENT_SCOPE)
    set(LIBETUDE_BUILD_CONFIG ${BUILD_CONFIG} PARENT_SCOPE)

    message(STATUS "빌드 정보:")
    message(STATUS "  시간: ${BUILD_TIMESTAMP}")
    message(STATUS "  컴파일러: ${COMPILER_INFO}")
    message(STATUS "  플랫폼: ${PLATFORM_INFO}")
    message(STATUS "  설정: ${BUILD_CONFIG}")
endfunction()

# 버전 헤더 파일 생성
function(generate_version_header)
    # Git 정보 수집
    get_git_info()

    # 빌드 정보 생성
    generate_build_info()

    # 버전 헤더 파일 내용 생성
    set(VERSION_HEADER_CONTENT "/* LibEtude 버전 정보 헤더 파일 */
/* 자동 생성됨 - 수정하지 마세요 */

#ifndef LIBETUDE_VERSION_H
#define LIBETUDE_VERSION_H

#ifdef __cplusplus
extern \"C\" {
#endif

/* 버전 정보 */
#define LIBETUDE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR}
#define LIBETUDE_VERSION_MINOR ${PROJECT_VERSION_MINOR}
#define LIBETUDE_VERSION_PATCH ${PROJECT_VERSION_PATCH}
#define LIBETUDE_VERSION_STRING \"${PROJECT_VERSION}\"

/* Git 정보 */
#define LIBETUDE_GIT_COMMIT_HASH \"${LIBETUDE_GIT_COMMIT_HASH}\"
#define LIBETUDE_GIT_BRANCH \"${LIBETUDE_GIT_BRANCH}\"
#define LIBETUDE_GIT_TAG \"${LIBETUDE_GIT_TAG}\"
#define LIBETUDE_GIT_DIRTY ${LIBETUDE_GIT_DIRTY}

/* 빌드 정보 */
#define LIBETUDE_BUILD_TIMESTAMP \"${LIBETUDE_BUILD_TIMESTAMP}\"
#define LIBETUDE_COMPILER_INFO \"${LIBETUDE_COMPILER_INFO}\"
#define LIBETUDE_PLATFORM_INFO \"${LIBETUDE_PLATFORM_INFO}\"
#define LIBETUDE_BUILD_CONFIG \"${LIBETUDE_BUILD_CONFIG}\"

/* 플랫폼 추상화 레이어 버전 */
#define LIBETUDE_PLATFORM_ABI_VERSION 1
#define LIBETUDE_PLATFORM_API_VERSION \"1.0.0\"

/* 호환성 매크로 */
#define LIBETUDE_VERSION_CHECK(major, minor, patch) \\
    ((LIBETUDE_VERSION_MAJOR > (major)) || \\
     (LIBETUDE_VERSION_MAJOR == (major) && LIBETUDE_VERSION_MINOR > (minor)) || \\
     (LIBETUDE_VERSION_MAJOR == (major) && LIBETUDE_VERSION_MINOR == (minor) && LIBETUDE_VERSION_PATCH >= (patch)))

/* 런타임 버전 정보 함수 */
const char* libetude_get_version_string(void);
const char* libetude_get_git_commit_hash(void);
const char* libetude_get_build_timestamp(void);
int libetude_check_version_compatibility(int required_major, int required_minor, int required_patch);

#ifdef __cplusplus
}
#endif

#endif /* LIBETUDE_VERSION_H */
")

    # 버전 헤더 파일 작성
    file(WRITE "${CMAKE_BINARY_DIR}/include/libetude/version.h" ${VERSION_HEADER_CONTENT})

    # 설치 대상에 추가
    install(FILES "${CMAKE_BINARY_DIR}/include/libetude/version.h"
        DESTINATION include/libetude
        COMPONENT Development
    )

    message(STATUS "버전 헤더 파일 생성: ${CMAKE_BINARY_DIR}/include/libetude/version.h")
endfunction()

# 버전 소스 파일 생성
function(generate_version_source)
    set(VERSION_SOURCE_CONTENT "/* LibEtude 버전 정보 구현 파일 */
/* 자동 생성됨 - 수정하지 마세요 */

#include \"libetude/version.h\"
#include <string.h>

const char* libetude_get_version_string(void) {
    return LIBETUDE_VERSION_STRING;
}

const char* libetude_get_git_commit_hash(void) {
    return LIBETUDE_GIT_COMMIT_HASH;
}

const char* libetude_get_build_timestamp(void) {
    return LIBETUDE_BUILD_TIMESTAMP;
}

int libetude_check_version_compatibility(int required_major, int required_minor, int required_patch) {
    /* 주 버전이 다르면 호환되지 않음 */
    if (LIBETUDE_VERSION_MAJOR != required_major) {
        return 0;
    }

    /* 현재 버전이 요구 버전보다 낮으면 호환되지 않음 */
    if (LIBETUDE_VERSION_MINOR < required_minor) {
        return 0;
    }

    if (LIBETUDE_VERSION_MINOR == required_minor && LIBETUDE_VERSION_PATCH < required_patch) {
        return 0;
    }

    return 1;
}
")

    # 버전 소스 파일 작성
    file(WRITE "${CMAKE_BINARY_DIR}/src/version.c" ${VERSION_SOURCE_CONTENT})

    message(STATUS "버전 소스 파일 생성: ${CMAKE_BINARY_DIR}/src/version.c")
endfunction()

# 패키지 메타데이터 생성
function(generate_package_metadata)
    # 패키지 설명 파일 생성
    set(PACKAGE_DESCRIPTION "LibEtude ${PROJECT_VERSION}

LibEtude는 음성 합성에 특화된 AI 추론 엔진입니다.

주요 기능:
- 플랫폼 추상화 레이어를 통한 크로스 플랫폼 지원
- SIMD 최적화를 통한 고성능 처리
- 실시간 음성 합성
- 모듈러 아키텍처

빌드 정보:
- 버전: ${PROJECT_VERSION}
- 커밋: ${LIBETUDE_GIT_COMMIT_HASH}
- 빌드 시간: ${LIBETUDE_BUILD_TIMESTAMP}
- 플랫폼: ${LIBETUDE_PLATFORM_INFO}
- 컴파일러: ${LIBETUDE_COMPILER_INFO}

플랫폼 추상화 레이어 기능:
- 오디오 I/O 추상화: ${LIBETUDE_ENABLE_AUDIO_ABSTRACTION}
- 시스템 정보 추상화: ${LIBETUDE_ENABLE_SYSTEM_ABSTRACTION}
- 스레딩 추상화: ${LIBETUDE_ENABLE_THREADING_ABSTRACTION}
- 메모리 관리 추상화: ${LIBETUDE_ENABLE_MEMORY_ABSTRACTION}
- 파일 시스템 추상화: ${LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION}
- 네트워크 추상화: ${LIBETUDE_ENABLE_NETWORK_ABSTRACTION}
- 동적 라이브러리 추상화: ${LIBETUDE_ENABLE_DYNLIB_ABSTRACTION}
")

    # 패키지 설명 파일 작성
    file(WRITE "${CMAKE_BINARY_DIR}/PACKAGE_INFO.txt" ${PACKAGE_DESCRIPTION})

    # 설치 대상에 추가
    install(FILES "${CMAKE_BINARY_DIR}/PACKAGE_INFO.txt"
        DESTINATION share/libetude/doc
        COMPONENT Documentation
    )

    message(STATUS "패키지 메타데이터 생성: ${CMAKE_BINARY_DIR}/PACKAGE_INFO.txt")
endfunction()

# 호환성 매트릭스 생성
function(generate_compatibility_matrix)
    set(COMPATIBILITY_MATRIX "# LibEtude 호환성 매트릭스

## 플랫폼 지원

| 플랫폼 | 아키텍처 | 지원 여부 | 최소 버전 |
|--------|----------|-----------|-----------|
| Windows | x86_64 | ✅ | Windows 7 |
| Windows | ARM64 | ✅ | Windows 10 |
| macOS | x86_64 | ✅ | macOS 12.0 |
| macOS | ARM64 | ✅ | macOS 12.0 |
| Linux | x86_64 | ✅ | Ubuntu 18.04+ |
| Linux | ARM64 | ✅ | Ubuntu 20.04+ |
| Android | ARM64 | ✅ | API 21+ |
| iOS | ARM64 | ✅ | iOS 12.0+ |

## 컴파일러 지원

| 컴파일러 | 최소 버전 | 권장 버전 |
|----------|-----------|-----------|
| MSVC | 2019 (16.0) | 2022 (17.0) |
| GCC | 7.0 | 11.0+ |
| Clang | 10.0 | 14.0+ |
| Apple Clang | 12.0 | 14.0+ |

## API 호환성

### 플랫폼 추상화 레이어 ABI 버전: ${LIBETUDE_PLATFORM_ABI_VERSION}

- ABI 버전이 동일하면 바이너리 호환성 보장
- API 버전이 동일하면 소스 호환성 보장

### 버전 호환성 정책

- 주 버전(Major): 호환되지 않는 API 변경
- 부 버전(Minor): 하위 호환 가능한 기능 추가
- 패치 버전(Patch): 버그 수정 및 성능 개선

현재 버전: ${PROJECT_VERSION}
")

    # 호환성 매트릭스 파일 작성
    file(WRITE "${CMAKE_BINARY_DIR}/COMPATIBILITY.md" ${COMPATIBILITY_MATRIX})

    # 설치 대상에 추가
    install(FILES "${CMAKE_BINARY_DIR}/COMPATIBILITY.md"
        DESTINATION share/libetude/doc
        COMPONENT Documentation
    )

    message(STATUS "호환성 매트릭스 생성: ${CMAKE_BINARY_DIR}/COMPATIBILITY.md")
endfunction()

# 전체 버전 관리 시스템 설정
function(setup_version_management)
    message(STATUS "버전 관리 시스템 설정 중...")

    # 버전 정보 수집 및 파일 생성
    generate_version_header()
    generate_version_source()
    generate_package_metadata()
    generate_compatibility_matrix()

    # 빌드에 버전 소스 추가
    set(LIBETUDE_VERSION_SOURCE "${CMAKE_BINARY_DIR}/src/version.c" PARENT_SCOPE)

    # 포함 디렉토리 추가
    include_directories("${CMAKE_BINARY_DIR}/include")

    message(STATUS "버전 관리 시스템 설정 완료")
endfunction()

# 릴리스 검증 함수
function(validate_release_build)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "릴리스 빌드 검증 중...")

        # Git 상태 확인
        if(LIBETUDE_GIT_DIRTY)
            message(WARNING "릴리스 빌드에 커밋되지 않은 변경사항이 있습니다")
        endif()

        # 태그 확인
        if(NOT LIBETUDE_GIT_TAG)
            message(WARNING "릴리스 빌드에 Git 태그가 없습니다")
        endif()

        # 버전 일치 확인
        if(LIBETUDE_GIT_TAG AND NOT LIBETUDE_GIT_TAG STREQUAL "v${PROJECT_VERSION}")
            message(WARNING "Git 태그(${LIBETUDE_GIT_TAG})와 프로젝트 버전(v${PROJECT_VERSION})이 일치하지 않습니다")
        endif()

        message(STATUS "릴리스 빌드 검증 완료")
    endif()
endfunction()