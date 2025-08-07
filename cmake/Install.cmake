# LibEtude 설치 및 패키징 설정
# Copyright (c) 2025 LibEtude Project

# 설치 경로 설정
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # 사용자가 설치 경로를 지정한 경우
    message(STATUS "사용자 지정 설치 경로: ${CMAKE_INSTALL_PREFIX}")
else()
    # 기본 설치 경로 설정
    if(WIN32)
        set(CMAKE_INSTALL_PREFIX "C:/Program Files/LibEtude" CACHE PATH "설치 경로" FORCE)
    elseif(APPLE)
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "설치 경로" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "설치 경로" FORCE)
    endif()
endif()

# 설치 구성 요소 정의
set(LIBETUDE_INSTALL_COMPONENTS
    Runtime
    Development
    Documentation
    Examples
    Tools
)

# 플랫폼 추상화 레이어 헤더 설치
function(install_platform_headers)
    # 공통 플랫폼 헤더
    install(FILES
        include/libetude/platform/common.h
        include/libetude/platform/factory.h
        DESTINATION include/libetude/platform
        COMPONENT Development
    )

    # 기능별 헤더
    if(LIBETUDE_ENABLE_AUDIO_ABSTRACTION)
        install(FILES include/libetude/platform/audio.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_SYSTEM_ABSTRACTION)
        install(FILES include/libetude/platform/system.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_THREADING_ABSTRACTION)
        install(FILES include/libetude/platform/threading.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_MEMORY_ABSTRACTION)
        install(FILES include/libetude/platform/memory.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_FILESYSTEM_ABSTRACTION)
        install(FILES include/libetude/platform/filesystem.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_NETWORK_ABSTRACTION)
        install(FILES include/libetude/platform/network.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    if(LIBETUDE_ENABLE_DYNLIB_ABSTRACTION)
        install(FILES include/libetude/platform/dynlib.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()

    # 최적화 및 고급 기능 헤더
    install(FILES
        include/libetude/platform/optimization.h
        include/libetude/platform/runtime_adaptation.h
        DESTINATION include/libetude/platform
        COMPONENT Development
    )

    # 플랫폼별 호환성 헤더
    if(APPLE)
        install(FILES include/libetude/platform/macos_compat.h
            DESTINATION include/libetude/platform
            COMPONENT Development
        )
    endif()
endfunction()

# 라이브러리 설치
function(install_libetude_libraries TARGET_NAME)
    # 런타임 라이브러리 설치
    install(TARGETS ${TARGET_NAME}
        RUNTIME DESTINATION bin COMPONENT Runtime
        LIBRARY DESTINATION lib COMPONENT Runtime
        ARCHIVE DESTINATION lib COMPONENT Development
        PUBLIC_HEADER DESTINATION include COMPONENT Development
    )

    # 플랫폼별 런타임 의존성 설치
    if(WIN32)
        # Windows DLL 의존성
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            # 디버그 런타임 설치
            install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}>
                DESTINATION bin
                COMPONENT Runtime
                OPTIONAL
            )
        endif()

        # Visual C++ 재배포 가능 패키지 포함
        include(InstallRequiredSystemLibraries)

    elseif(APPLE)
        # macOS 번들 설정
        set_target_properties(${TARGET_NAME} PROPERTIES
            MACOSX_RPATH ON
            INSTALL_RPATH "@loader_path/../lib"
        )

    elseif(UNIX)
        # Linux RPATH 설정
        set_target_properties(${TARGET_NAME} PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
        )
    endif()
endfunction()

# pkg-config 파일 생성 및 설치
function(install_pkgconfig_file)
    # pkg-config 템플릿 파일 확인
    if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/libetude.pc.in")
        # 변수 치환
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/libetude.pc.in"
            "${CMAKE_BINARY_DIR}/libetude.pc"
            @ONLY
        )

        # pkg-config 파일 설치
        install(FILES "${CMAKE_BINARY_DIR}/libetude.pc"
            DESTINATION lib/pkgconfig
            COMPONENT Development
        )

        message(STATUS "pkg-config 파일이 생성됩니다")
    endif()
endfunction()

# CMake 설정 파일 생성 및 설치
function(install_cmake_config)
    include(CMakePackageConfigHelpers)

    # 버전 파일 생성
    write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/LibEtudeConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # 설정 파일 생성
    configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/LibEtudeConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/LibEtudeConfig.cmake"
        INSTALL_DESTINATION lib/cmake/LibEtude
        PATH_VARS CMAKE_INSTALL_PREFIX
    )

    # CMake 설정 파일 설치
    install(FILES
        "${CMAKE_BINARY_DIR}/LibEtudeConfig.cmake"
        "${CMAKE_BINARY_DIR}/LibEtudeConfigVersion.cmake"
        DESTINATION lib/cmake/LibEtude
        COMPONENT Development
    )

    # 타겟 내보내기
    install(EXPORT LibEtudeTargets
        FILE LibEtudeTargets.cmake
        NAMESPACE LibEtude::
        DESTINATION lib/cmake/LibEtude
        COMPONENT Development
    )
endfunction()

# 문서 설치
function(install_documentation)
    # 기본 문서
    install(FILES
        README.md
        LICENSE
        RELEASE_NOTES.md
        DESTINATION share/libetude/doc
        COMPONENT Documentation
    )

    # API 문서
    if(EXISTS "${CMAKE_SOURCE_DIR}/docs")
        install(DIRECTORY docs/
            DESTINATION share/libetude/doc
            COMPONENT Documentation
            PATTERN "*.md"
            PATTERN "*.html"
            PATTERN "*.pdf"
        )
    endif()

    # 플랫폼별 문서
    install(FILES
        docs/api/platform_abstraction.md
        docs/api/platform_migration_guide.md
        docs/api/platform_optimization_guide.md
        DESTINATION share/libetude/doc/platform
        COMPONENT Documentation
        OPTIONAL
    )
endfunction()

# 예제 설치
function(install_examples)
    if(LIBETUDE_BUILD_EXAMPLES)
        # 플랫폼 추상화 예제
        install(DIRECTORY examples/platform_abstraction/
            DESTINATION share/libetude/examples/platform_abstraction
            COMPONENT Examples
            PATTERN "build" EXCLUDE
            PATTERN "*.o" EXCLUDE
            PATTERN "*.exe" EXCLUDE
        )

        # 기타 예제
        install(DIRECTORY examples/
            DESTINATION share/libetude/examples
            COMPONENT Examples
            PATTERN "build" EXCLUDE
            PATTERN "*.o" EXCLUDE
            PATTERN "*.exe" EXCLUDE
            PATTERN "platform_abstraction" EXCLUDE  # 이미 위에서 설치됨
        )
    endif()
endfunction()

# 개발 도구 설치
function(install_tools)
    if(LIBETUDE_BUILD_TOOLS)
        # 벤치마크 도구
        if(TARGET libetude_benchmark)
            install(TARGETS libetude_benchmark
                RUNTIME DESTINATION bin
                COMPONENT Tools
            )
        endif()

        # 성능 분석 도구
        if(TARGET libetude_profiler)
            install(TARGETS libetude_profiler
                RUNTIME DESTINATION bin
                COMPONENT Tools
            )
        endif()

        # 플랫폼 테스트 스크립트
        install(PROGRAMS
            scripts/run_platform_tests.sh
            scripts/run_platform_tests.bat
            DESTINATION bin
            COMPONENT Tools
        )
    endif()
endfunction()

# 언인스톨 스크립트 생성
function(create_uninstall_script)
    # 언인스톨 스크립트 템플릿
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY
    )

    # 언인스톨 타겟 추가
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "LibEtude 언인스톨 중..."
    )
endfunction()

# 설치 후 스크립트 설정
function(configure_post_install)
    if(UNIX AND NOT APPLE)
        # Linux에서 ldconfig 실행
        install(CODE "execute_process(COMMAND ldconfig)")
    endif()

    # 설치 완료 메시지
    install(CODE "message(STATUS \"LibEtude 설치가 완료되었습니다.\")")
    install(CODE "message(STATUS \"설치 경로: ${CMAKE_INSTALL_PREFIX}\")")

    # 플랫폼별 설치 후 작업
    if(WIN32)
        # Windows 환경 변수 설정 안내
        install(CODE "message(STATUS \"PATH에 ${CMAKE_INSTALL_PREFIX}/bin을 추가하세요.\")")
    elseif(APPLE)
        # macOS 프레임워크 서명 (필요시)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            install(CODE "message(STATUS \"릴리스 빌드가 설치되었습니다.\")")
        endif()
    endif()
endfunction()

# 전체 설치 설정 함수
function(configure_libetude_install TARGET_NAME)
    message(STATUS "LibEtude 설치 설정 구성 중...")

    # 각 설치 구성 요소 설정
    install_platform_headers()
    install_libetude_libraries(${TARGET_NAME})
    install_pkgconfig_file()
    install_cmake_config()
    install_documentation()
    install_examples()
    install_tools()
    create_uninstall_script()
    configure_post_install()

    message(STATUS "설치 설정 완료")
    message(STATUS "설치 경로: ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "설치 구성 요소: ${LIBETUDE_INSTALL_COMPONENTS}")
endfunction()

# 개발자 설치 (헤더와 라이브러리만)
function(install_development_only TARGET_NAME)
    install_platform_headers()
    install_libetude_libraries(${TARGET_NAME})
    install_pkgconfig_file()
    install_cmake_config()

    message(STATUS "개발자 전용 설치 설정 완료")
endfunction()

# 런타임 전용 설치
function(install_runtime_only TARGET_NAME)
    # 런타임 라이브러리만 설치
    install(TARGETS ${TARGET_NAME}
        RUNTIME DESTINATION bin COMPONENT Runtime
        LIBRARY DESTINATION lib COMPONENT Runtime
    )

    # 기본 문서
    install(FILES README.md LICENSE
        DESTINATION share/libetude/doc
        COMPONENT Runtime
    )

    message(STATUS "런타임 전용 설치 설정 완료")
endfunction()