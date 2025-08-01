# world4utau 예제 CMake 설정
# UTAU 호환 음성 합성 엔진 예제

cmake_minimum_required(VERSION 3.16)

# world4utau 실행 파일 생성
add_executable(world4utau
    main.c
    src/utau_interface.c
    src/world_engine.c
)

# libetude와 연결
target_link_libraries(world4utau ${CMAKE_CURRENT_SOURCE_DIR}/../../build/src/liblibetude_static.a)

# 헤더 파일 경로 설정
target_include_directories(world4utau PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 컴파일러 플래그 설정
target_compile_definitions(world4utau PRIVATE
    WORLD4UTAU_VERSION="1.0.0"
    LIBETUDE_INTEGRATION=1
)

# 플랫폼별 링크 라이브러리 추가
if(WIN32)
    target_link_libraries(world4utau winmm dsound psapi)
elseif(APPLE)
    target_link_libraries(world4utau "-framework AudioToolbox" "-framework CoreAudio")
elseif(UNIX)
    target_link_libraries(world4utau asound pthread m)
endif()

# 설치 설정
install(TARGETS world4utau
    RUNTIME DESTINATION bin/examples
)

# 예제 데이터 및 문서 복사
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data/)
endif()

# 테스트 빌드 (선택적)
if(LIBETUDE_BUILD_TESTS)
    add_subdirectory(tests)
endif()