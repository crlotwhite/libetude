# LibEtude 실시간 스트리밍 데모 애플리케이션

cmake_minimum_required(VERSION 3.12)

# 데모 실행 파일 생성
add_executable(libetude_streaming_demo
    main.c
)

# 헤더 파일 경로 설정
target_include_directories(libetude_streaming_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# LibEtude 라이브러리 링크
target_link_libraries(libetude_streaming_demo PRIVATE
    libetude_core
    libetude_audio
    libetude_runtime
)

# 스레드 라이브러리 링크
find_package(Threads REQUIRED)
target_link_libraries(libetude_streaming_demo PRIVATE
    Threads::Threads
)

# 플랫폼별 라이브러리 링크
if(WIN32)
    target_link_libraries(libetude_streaming_demo PRIVATE
        winmm
        dsound
        ws2_32
    )
elseif(APPLE)
    target_link_libraries(libetude_streaming_demo PRIVATE
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework AudioUnit"
    )
elseif(UNIX)
    target_link_libraries(libetude_streaming_demo PRIVATE
        asound
        pulse
        rt
    )
endif()

# 컴파일러 플래그 설정
target_compile_features(libetude_streaming_demo PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libetude_streaming_demo PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -pthread
    )
endif()

# 실시간 처리를 위한 최적화
target_compile_definitions(libetude_streaming_demo PRIVATE
    LIBETUDE_STREAMING=1
    LIBETUDE_REALTIME=1
)

# 디버그 빌드에서 추가 정보 포함
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(libetude_streaming_demo PRIVATE
        DEBUG=1
        LIBETUDE_DEBUG=1
        STREAMING_DEBUG=1
    )
endif()

# 릴리스 빌드에서 최적화
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(libetude_streaming_demo PRIVATE
        -O3
        -DNDEBUG
    )
endif()

# 설치 설정
install(TARGETS libetude_streaming_demo
    RUNTIME DESTINATION bin/examples
)

# 예제 설정 파일 복사
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/streaming_config.json")
    install(FILES streaming_config.json
        DESTINATION share/libetude/examples/streaming
    )
endif()

# README 파일 설치
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/libetude/examples/streaming
    )
endif()