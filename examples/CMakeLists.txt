# LibEtude 예제 CMake 설정

# 기본 TTS 예제
add_executable(basic_tts
    basic_tts/main.c
)
target_link_libraries(basic_tts libetude_static)
target_include_directories(basic_tts PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 스트리밍 예제
add_executable(streaming_example
    streaming/main.c
)
target_link_libraries(streaming_example libetude_static)
target_include_directories(streaming_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# C++ 바인딩 예제
if(LIBETUDE_BUILD_BINDINGS)
    add_executable(cpp_example
        cpp_binding/main.cpp
    )
    target_link_libraries(cpp_example libetude_static)
    target_include_directories(cpp_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
endif()

# 플러그인 예제
add_executable(plugin_example
    plugins/main.c
)
target_link_libraries(plugin_example libetude_static)
target_include_directories(plugin_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 데스크톱 최적화 예제
add_executable(desktop_optimization_example
    desktop_optimization/main.c
)
target_link_libraries(desktop_optimization_example libetude_static)
target_include_directories(desktop_optimization_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 임베디드 최적화 예제
add_executable(embedded_optimization_example
    embedded_optimization/main.c
)
target_link_libraries(embedded_optimization_example libetude_static)
target_include_directories(embedded_optimization_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 플랫폼별 링크 라이브러리 추가
if(WIN32)
    target_link_libraries(desktop_optimization_example winmm dsound psapi)
    target_link_libraries(embedded_optimization_example winmm psapi)
elseif(APPLE)
    target_link_libraries(desktop_optimization_example "-framework AudioToolbox" "-framework CoreAudio")
    target_link_libraries(embedded_optimization_example "-framework AudioToolbox" "-framework CoreAudio")
elseif(UNIX)
    target_link_libraries(desktop_optimization_example asound pthread m)
    target_link_libraries(embedded_optimization_example pthread m)
endif()

# 예제 설치
install(TARGETS basic_tts streaming_example plugin_example desktop_optimization_example embedded_optimization_example
    RUNTIME DESTINATION bin/examples
)

if(LIBETUDE_BUILD_BINDINGS)
    install(TARGETS cpp_example
        RUNTIME DESTINATION bin/examples
    )
endif()

# 예제 데이터 파일 복사
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)