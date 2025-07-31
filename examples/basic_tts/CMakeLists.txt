# LibEtude 기본 TTS 데모 애플리케이션

cmake_minimum_required(VERSION 3.12)

# 데모 실행 파일 생성
add_executable(libetude_basic_tts_demo
    main.c
)

# 헤더 파일 경로 설정
target_include_directories(libetude_basic_tts_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# LibEtude 라이브러리 링크
target_link_libraries(libetude_basic_tts_demo PRIVATE
    libetude_core
    libetude_audio
    libetude_runtime
)

# 플랫폼별 라이브러리 링크
if(WIN32)
    target_link_libraries(libetude_basic_tts_demo PRIVATE
        winmm
        dsound
    )
elseif(APPLE)
    target_link_libraries(libetude_basic_tts_demo PRIVATE
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
elseif(UNIX)
    target_link_libraries(libetude_basic_tts_demo PRIVATE
        asound
        pthread
    )
endif()

# 컴파일러 플래그 설정
target_compile_features(libetude_basic_tts_demo PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libetude_basic_tts_demo PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# 디버그 빌드에서 추가 정보 포함
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(libetude_basic_tts_demo PRIVATE
        DEBUG=1
        LIBETUDE_DEBUG=1
    )
endif()

# 설치 설정
install(TARGETS libetude_basic_tts_demo
    RUNTIME DESTINATION bin/examples
)

# 예제 모델 파일 복사 (있는 경우)
if(EXISTS "${CMAKE_SOURCE_DIR}/models")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/models"
        DESTINATION share/libetude/examples/basic_tts
        FILES_MATCHING PATTERN "*.lef"
    )
endif()

# README 파일 설치
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/libetude/examples/basic_tts
    )
endif()