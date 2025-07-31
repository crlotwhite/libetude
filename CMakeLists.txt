# LibEtude - AI 음성 합성 추론 엔진
# Copyright (c) 2025 LibEtude Project
cmake_minimum_required(VERSION 3.16)

project(LibEtude
    VERSION 1.0.0
    DESCRIPTION "AI inference engine specialized for voice synthesis"
    LANGUAGES C CXX
)

# C/C++ 표준 설정
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 빌드 타입 기본값 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 컴파일러 플래그 설정
if(MSVC)
    # Windows MSVC 컴파일러
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
else()
    # GCC/Clang 컴파일러
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

# 플랫폼별 설정
if(WIN32)
    add_definitions(-DLIBETUDE_PLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DLIBETUDE_PLATFORM_MACOS)
elseif(ANDROID)
    add_definitions(-DLIBETUDE_PLATFORM_ANDROID)
elseif(UNIX)
    add_definitions(-DLIBETUDE_PLATFORM_LINUX)
endif()

# 하드웨어 최적화 옵션
option(LIBETUDE_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(LIBETUDE_ENABLE_GPU "Enable GPU acceleration" ON)
option(LIBETUDE_MINIMAL "Build minimal version for embedded systems" OFF)

# SIMD 지원 감지
if(LIBETUDE_ENABLE_SIMD)
    include(CheckCSourceCompiles)

    # SSE 지원 확인
    check_c_source_compiles("
        #include <xmmintrin.h>
        int main() { __m128 a = _mm_setzero_ps(); return 0; }
    " HAVE_SSE)

    # AVX 지원 확인
    check_c_source_compiles("
        #include <immintrin.h>
        int main() { __m256 a = _mm256_setzero_ps(); return 0; }
    " HAVE_AVX)

    # NEON 지원 확인 (ARM)
    check_c_source_compiles("
        #include <arm_neon.h>
        int main() { float32x4_t a = vdupq_n_f32(0.0f); return 0; }
    " HAVE_NEON)

    if(HAVE_SSE)
        add_definitions(-DLIBETUDE_HAVE_SSE)
    endif()
    if(HAVE_AVX)
        add_definitions(-DLIBETUDE_HAVE_AVX)
    endif()
    if(HAVE_NEON)
        add_definitions(-DLIBETUDE_HAVE_NEON)
    endif()
endif()

# 의존성 관리 모듈 포함
include(cmake/Dependencies.cmake)

# 포함 디렉토리 설정
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 서브디렉토리 추가
add_subdirectory(src)

# 테스트 활성화
enable_testing()
add_subdirectory(tests)

# 예제 빌드 (선택적)
option(LIBETUDE_BUILD_EXAMPLES "Build example applications" ON)
if(LIBETUDE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 도구 빌드 (선택적)
option(LIBETUDE_BUILD_TOOLS "Build development tools" ON)
if(LIBETUDE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# 바인딩 빌드 (선택적)
option(LIBETUDE_BUILD_BINDINGS "Build language bindings" ON)
if(LIBETUDE_BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()

# 설치 설정
install(FILES README.md LICENSE DESTINATION share/libetude COMPONENT Documentation)

# 문서 설치
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/docs)
    install(DIRECTORY docs/ DESTINATION share/libetude/docs COMPONENT Documentation)
endif()

# 예제 소스 코드 설치
if(LIBETUDE_BUILD_EXAMPLES)
    install(DIRECTORY examples/ DESTINATION share/libetude/examples
        COMPONENT Examples
        PATTERN "build" EXCLUDE
        PATTERN "*.o" EXCLUDE
        PATTERN "*.exe" EXCLUDE
    )
endif()

# 패키지 설정
set(CPACK_PACKAGE_NAME "LibEtude")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VENDOR "LibEtude Project")
set(CPACK_PACKAGE_CONTACT "info@libetude.org")

# 플랫폼별 패키지 설정
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "LibEtude ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "LibEtude")
    set(CPACK_NSIS_HELP_LINK "https://github.com/libetude/libetude")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/libetude/libetude")
    set(CPACK_NSIS_CONTACT "info@libetude.org")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "LibEtude ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")

    # Debian 패키지 설정
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LibEtude Project <info@libetude.org>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17)")

    # RPM 패키지 설정
    set(CPACK_RPM_PACKAGE_GROUP "System/Libraries")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.17")
endif()

# 컴포넌트 설정
set(CPACK_COMPONENTS_ALL Runtime Development Documentation Examples)

set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Libraries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "LibEtude runtime libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "LibEtude development headers and libraries")
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)

set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "LibEtude documentation")

set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Examples")
set(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "LibEtude example applications")
set(CPACK_COMPONENT_EXAMPLES_DEPENDS Runtime)

include(CPack)