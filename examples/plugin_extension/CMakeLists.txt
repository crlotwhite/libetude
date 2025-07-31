# LibEtude 플러그인 확장 데모 애플리케이션

cmake_minimum_required(VERSION 3.12)

# 데모 실행 파일 생성
add_executable(libetude_plugin_extension_demo
    main.c
)

# 헤더 파일 경로 설정
target_include_directories(libetude_plugin_extension_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# LibEtude 라이브러리 링크
target_link_libraries(libetude_plugin_extension_demo PRIVATE
    libetude_core
    libetude_audio
    libetude_runtime
    libetude_lef
)

# 동적 로딩 라이브러리 링크
if(WIN32)
    # Windows에서는 LoadLibrary 사용
    target_link_libraries(libetude_plugin_extension_demo PRIVATE
        kernel32
    )
else()
    # Unix 계열에서는 dlopen 사용
    target_link_libraries(libetude_plugin_extension_demo PRIVATE
        dl
    )
endif()

# 플랫폼별 라이브러리 링크
if(WIN32)
    target_link_libraries(libetude_plugin_extension_demo PRIVATE
        winmm
        dsound
    )
elseif(APPLE)
    target_link_libraries(libetude_plugin_extension_demo PRIVATE
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
elseif(UNIX)
    target_link_libraries(libetude_plugin_extension_demo PRIVATE
        asound
        pthread
    )
endif()

# 컴파일러 플래그 설정
target_compile_features(libetude_plugin_extension_demo PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libetude_plugin_extension_demo PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
endif()

# 플러그인 지원을 위한 정의
target_compile_definitions(libetude_plugin_extension_demo PRIVATE
    LIBETUDE_PLUGIN_SUPPORT=1
    LIBETUDE_EXTENSION_SUPPORT=1
)

# 디버그 빌드에서 추가 정보 포함
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(libetude_plugin_extension_demo PRIVATE
        DEBUG=1
        LIBETUDE_DEBUG=1
        PLUGIN_DEBUG=1
    )
endif()

# 설치 설정
install(TARGETS libetude_plugin_extension_demo
    RUNTIME DESTINATION bin/examples
)

# 예제 플러그인 디렉토리 생성
install(DIRECTORY DESTINATION share/libetude/examples/plugin_extension/plugins)
install(DIRECTORY DESTINATION share/libetude/examples/plugin_extension/extensions)

# 예제 플러그인 파일 복사 (있는 경우)
if(EXISTS "${CMAKE_SOURCE_DIR}/examples/plugins")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/examples/plugins/"
        DESTINATION share/libetude/examples/plugin_extension/plugins
        FILES_MATCHING
        PATTERN "*.so"
        PATTERN "*.dll"
        PATTERN "*.dylib"
    )
endif()

# 예제 확장 모델 파일 복사 (있는 경우)
if(EXISTS "${CMAKE_SOURCE_DIR}/extensions")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/extensions/"
        DESTINATION share/libetude/examples/plugin_extension/extensions
        FILES_MATCHING PATTERN "*.lefx"
    )
endif()

# README 파일 설치
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/libetude/examples/plugin_extension
    )
endif()

# 샘플 플러그인 설정 파일
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin_config.json")
    install(FILES plugin_config.json
        DESTINATION share/libetude/examples/plugin_extension
    )
endif()