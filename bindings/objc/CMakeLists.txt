# LibEtude iOS/macOS Objective-C 바인딩

cmake_minimum_required(VERSION 3.12)

# Objective-C 바인딩 소스 파일들
set(OBJC_SOURCES
    src/LibEtudeEngine.mm
    src/LibEtudeAudioStream.mm
    src/LibEtudePerformanceStats.mm
    src/LibEtudeUtils.mm
)

# Objective-C 헤더 파일들
set(OBJC_HEADERS
    include/LibEtudeEngine.h
    include/LibEtudeAudioStream.h
    include/LibEtudePerformanceStats.h
    include/LibEtudeUtils.h
)

# Objective-C 바인딩 라이브러리 생성
add_library(libetude_objc STATIC ${OBJC_SOURCES})

# Objective-C++ 컴파일 설정
set_target_properties(libetude_objc PROPERTIES
    COMPILE_FLAGS "-fobjc-arc -fmodules"
    LINKER_LANGUAGE CXX
)

# iOS/macOS 특화 최적화
if(IOS)
    target_compile_definitions(libetude_objc PRIVATE
        IOS_PLATFORM
        LIBETUDE_MOBILE_OPTIMIZED
    )

    # iOS 프레임워크 링크
    target_link_libraries(libetude_objc PRIVATE
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework AudioToolbox"
        "-framework Accelerate"
    )

    # ARM64 최적화
    target_compile_definitions(libetude_objc PRIVATE LIBETUDE_ARM_NEON)

elseif(APPLE)
    target_compile_definitions(libetude_objc PRIVATE
        MACOS_PLATFORM
    )

    # macOS 프레임워크 링크
    target_link_libraries(libetude_objc PRIVATE
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework AudioUnit"
        "-framework CoreAudio"
        "-framework Accelerate"
    )
endif()

# 헤더 파일 경로 설정
target_include_directories(libetude_objc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

# LibEtude 코어 라이브러리 링크
target_link_libraries(libetude_objc PRIVATE
    libetude_core
)

# 컴파일 옵션 설정
target_compile_options(libetude_objc PRIVATE
    -Wall -Wextra
    -fvisibility=hidden
)

# 메모리 최적화 플래그
target_compile_definitions(libetude_objc PRIVATE
    LIBETUDE_MEMORY_OPTIMIZED
    LIBETUDE_MOBILE_MEMORY_LIMIT=256  # iOS는 256MB 메모리 제한
)

# 헤더 파일 설치
install(FILES ${OBJC_HEADERS}
    DESTINATION include/libetude
    COMPONENT Development
)

# 라이브러리 설치
install(TARGETS libetude_objc
    ARCHIVE DESTINATION lib
    COMPONENT Development
)