# 플러그인 의존성 관리 예제

add_executable(plugin_dependency_example main.c)

target_link_libraries(plugin_dependency_example
    libetude_static
)

target_include_directories(plugin_dependency_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 플랫폼별 라이브러리 링크
if(WIN32)
    target_link_libraries(plugin_dependency_example winmm dsound psapi)
elseif(APPLE)
    target_link_libraries(plugin_dependency_example "-framework AudioToolbox" "-framework CoreAudio" dl)
elseif(UNIX)
    target_link_libraries(plugin_dependency_example asound pthread m dl uuid curl json-c ssl crypto)
endif()

# 설치 설정
install(TARGETS plugin_dependency_example
    RUNTIME DESTINATION bin/examples
)

# 예제 실행 타겟
add_custom_target(run_plugin_dependency_example
    COMMAND plugin_dependency_example
    DEPENDS plugin_dependency_example
    COMMENT "플러그인 의존성 관리 예제 실행"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)