# 벤치마크 도구 CMake 설정

# 벤치마크 도구 실행 파일
add_executable(libetude_benchmark
    benchmark_tool.c
)

# 링크 라이브러리
target_link_libraries(libetude_benchmark
    libetude_static
)

# 컴파일 옵션
target_compile_options(libetude_benchmark PRIVATE
    -O3
    -DNDEBUG
)

# 설치
install(TARGETS libetude_benchmark
    RUNTIME DESTINATION bin
)