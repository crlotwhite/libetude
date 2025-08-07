# 플랫폼 추상화 레이어 예제

cmake_minimum_required(VERSION 3.12)

# 예제 실행 파일들
add_executable(audio_example audio_example.c)
target_link_libraries(audio_example libetude)

add_executable(system_info_example system_info_example.c)
target_link_libraries(system_info_example libetude)

add_executable(threading_example threading_example.c)
target_link_libraries(threading_example libetude)

add_executable(memory_example memory_example.c)
target_link_libraries(memory_example libetude)

add_executable(filesystem_example filesystem_example.c)
target_link_libraries(filesystem_example libetude)

add_executable(network_example network_example.c)
target_link_libraries(network_example libetude)

add_executable(dynlib_example dynlib_example.c)
target_link_libraries(dynlib_example libetude)

add_executable(cross_platform_demo cross_platform_demo.c)
target_link_libraries(cross_platform_demo libetude)

add_executable(performance_benchmark performance_benchmark.c)
target_link_libraries(performance_benchmark libetude)

# 설치 설정
install(TARGETS
    audio_example
    system_info_example
    threading_example
    memory_example
    filesystem_example
    network_example
    dynlib_example
    cross_platform_demo
    performance_benchmark
    DESTINATION bin/examples
)

# 예제 데이터 파일 복사
install(FILES
    README.md
    sample_data.txt
    DESTINATION share/libetude/examples
)