# LibEtude C++ 바인딩 빌드 설정

cmake_minimum_required(VERSION 3.12)

# C++ 바인딩 라이브러리 생성
add_library(libetude_cpp
    engine.cpp
)

# C++17 표준 사용
target_compile_features(libetude_cpp PUBLIC cxx_std_17)

# 헤더 파일 경로 설정
target_include_directories(libetude_cpp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/libetude/cpp
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# 핵심 라이브러리와 링크
target_link_libraries(libetude_cpp
    PUBLIC
        libetude_core
)

# 플랫폼별 설정
if(WIN32)
    target_compile_definitions(libetude_cpp PRIVATE LIBETUDE_EXPORTS)
endif()

# 컴파일러별 최적화 설정
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(libetude_cpp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(libetude_cpp PRIVATE -O3 -DNDEBUG)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libetude_cpp PRIVATE
        /W4
        /wd4100  # 사용되지 않는 매개변수 경고 비활성화
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(libetude_cpp PRIVATE /O2 /DNDEBUG)
    endif()
endif()

# 설치 설정
install(TARGETS libetude_cpp
    EXPORT LibEtudeTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 헤더 파일 설치
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/libetude/cpp/
    DESTINATION include/libetude/cpp
    FILES_MATCHING PATTERN "*.hpp"
)

# pkg-config 파일 생성
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libetude-cpp.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libetude-cpp.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libetude-cpp.pc
    DESTINATION lib/pkgconfig
)