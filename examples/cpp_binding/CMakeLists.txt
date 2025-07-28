# LibEtude C++ 바인딩 예제

cmake_minimum_required(VERSION 3.12)

# C++ 바인딩 예제 실행 파일 생성
add_executable(libetude_cpp_example
    main.cpp
)

# C++17 표준 사용
target_compile_features(libetude_cpp_example PRIVATE cxx_std_17)

# 헤더 파일 경로 설정
target_include_directories(libetude_cpp_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# 라이브러리 링크
target_link_libraries(libetude_cpp_example PRIVATE
    libetude_cpp
    libetude_core
)

# 컴파일러별 설정
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(libetude_cpp_example PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libetude_cpp_example PRIVATE /W4)
endif()

# 실행 파일 출력 디렉토리 설정
set_target_properties(libetude_cpp_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)