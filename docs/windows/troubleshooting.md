# Windows 문제 해결 가이드

이 문서는 Windows 환경에서 LibEtude 사용 시 발생할 수 있는 일반적인 문제와 해결 방법을 설명합니다.

## 목차

- [빌드 관련 문제](#빌드-관련-문제)
- [오디오 관련 문제](#오디오-관련-문제)
- [성능 관련 문제](#성능-관련-문제)
- [보안 관련 문제](#보안-관련-문제)
- [디버깅 관련 문제](#디버깅-관련-문제)
- [배포 관련 문제](#배포-관련-문제)

## 빌드 관련 문제

### 문제 1: CMake 설정 실패

**증상:**
```
CMake Error: Could not find Windows SDK
```

**원인:** Windows SDK가 설치되지 않았거나 CMake가 찾을 수 없음

**해결 방법:**
1. Visual Studio Installer에서 Windows SDK 설치 확인
2. 환경 변수 설정:
```cmd
set WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10
set WindowsSDKVersion=10.0.22000.0
```

3. CMake 재실행:
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### 문제 2: MSVC 컴파일러 오류

**증상:**
```
error C2065: 'AVX2': undeclared identifier
```

**원인:** 구형 컴파일러 또는 잘못된 아키텍처 설정

**해결 방법:**
1. Visual Studio 2019 이상 사용 확인
2. 아키텍처 플래그 확인:
```cmake
target_compile_options(libetude PRIVATE /arch:AVX2)
```

3. CPU 지원 확인:
```c
ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
if (!features.has_avx2) {
    printf("경고: AVX2가 지원되지 않습니다\n");
}
```

### 문제 3: MinGW 링크 오류

**증상:**
```
undefined reference to `__imp_CoInitialize'
```

**원인:** Windows API 라이브러리 링크 누락

**해결 방법:**
```cmake
if(MINGW)
    target_link_libraries(libetude PRIVATE
        ole32 oleaut32 uuid dsound winmm ksuser
    )
endif()
```

## 오디오 관련 문제

### 문제 1: WASAPI 초기화 실패

**증상:**
```
WASAPI 초기화 실패: 0x80070005 (Access Denied)
```

**원인:** 오디오 디바이스 접근 권한 부족 또는 디바이스 사용 중

**해결 방법:**
1. 다른 오디오 애플리케이션 종료
2. 오디오 디바이스 드라이버 업데이트
3. DirectSound 폴백 확인:
```c
ETResult result = et_audio_init_wasapi_with_fallback(ctx);
if (result != ET_SUCCESS) {
    printf("WASAPI 실패, DirectSound로 폴백\n");
}
```

### 문제 2: 오디오 지연 문제

**증상:** 음성 출력이 지연되거나 끊김

**원인:** 버퍼 크기 설정 부적절 또는 CPU 성능 부족

**해결 방법:**
1. 버퍼 크기 조정:
```c
// 지연 감소 (CPU 사용량 증가)
et_audio_set_buffer_size(ctx, 10); // 10ms

// 안정성 우선 (지연 증가)
et_audio_set_buffer_size(ctx, 50); // 50ms
```

2. 배타 모드 사용:
```c
ETAudioConfig audio_config = {
    .exclusive_mode = true,
    .buffer_size_ms = 10
};
```

### 문제 3: 오디오 디바이스 인식 실패

**증상:** 사용 가능한 오디오 디바이스가 없다고 표시

**원인:** 오디오 서비스 중단 또는 드라이버 문제

**해결 방법:**
1. Windows Audio 서비스 재시작:
```cmd
net stop audiosrv
net start audiosrv
```

2. 디바이스 수동 확인:
```c
ETWindowsAudioDevice devices[16];
int device_count = et_windows_enumerate_audio_devices(devices, 16);

for (int i = 0; i < device_count; i++) {
    wprintf(L"디바이스 %d: %s\n", i, devices[i].friendly_name);
}
```

## 성능 관련 문제

### 문제 1: 예상보다 느린 성능

**증상:** RTF (Real-Time Factor)가 1.0 이상

**원인:** 최적화 미적용 또는 하드웨어 제한

**해결 방법:**
1. Release 빌드 확인:
```cmd
cmake --build build --config Release
```

2. CPU 기능 확인:
```c
ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
printf("AVX2 지원: %s\n", features.has_avx2 ? "예" : "아니오");
printf("AVX-512 지원: %s\n", features.has_avx512 ? "예" : "아니오");
```

3. 스레드 풀 최적화:
```c
SYSTEM_INFO sys_info;
GetSystemInfo(&sys_info);
DWORD optimal_threads = sys_info.dwNumberOfProcessors;

et_windows_threadpool_init(&pool, optimal_threads/2, optimal_threads*2);
```

### 문제 2: 메모리 사용량 과다

**증상:** 메모리 사용량이 예상보다 높음

**원인:** Large Page 미사용 또는 메모리 누수

**해결 방법:**
1. Large Page 활성화:
```c
bool large_page_enabled = et_windows_enable_large_page_privilege();
if (!large_page_enabled) {
    printf("Large Page 권한이 필요합니다 (관리자 권한으로 실행)\n");
}
```

2. 메모리 누수 검사:
```c
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
```

### 문제 3: CPU 사용률 과다

**증상:** CPU 사용률이 100%에 근접

**원인:** 무한 루프 또는 비효율적인 알고리즘

**해결 방법:**
1. ETW 프로파일링:
```c
et_windows_register_etw_provider();
// 성능 병목 지점 식별
```

2. 작업 분산:
```c
// CPU 집약적 작업을 여러 스레드로 분산
et_windows_threadpool_submit_work(&pool, cpu_intensive_task, data);
```

## 보안 관련 문제

### 문제 1: DEP 호환성 오류

**증상:**
```
Application terminated due to Data Execution Prevention
```

**원인:** 실행 가능한 메모리 영역에서 데이터 실행 시도

**해결 방법:**
1. DEP 호환성 확인:
```c
bool dep_compatible = et_windows_check_dep_compatibility();
if (!dep_compatible) {
    printf("DEP 호환성 문제 감지\n");
}
```

2. 메모리 할당 방식 변경:
```c
// DEP 호환 메모리 할당
void* memory = et_windows_alloc_aslr_compatible(size);
```

### 문제 2: UAC 권한 문제

**증상:** 특정 기능이 작동하지 않음 (Large Page 등)

**원인:** 관리자 권한 부족

**해결 방법:**
1. 권한 확인:
```c
bool has_permissions = et_windows_check_uac_permissions();
if (!has_permissions) {
    printf("일부 기능을 위해 관리자 권한이 필요합니다\n");
}
```

2. 매니페스트 파일 추가:
```xml
<!-- app.manifest -->
<requestedExecutionLevel level="requireAdministrator" uiAccess="false" />
```

## 디버깅 관련 문제

### 문제 1: PDB 파일 생성 안됨

**증상:** Visual Studio에서 디버깅 정보 없음

**원인:** Debug 빌드 설정 누락

**해결 방법:**
```cmake
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(libetude PRIVATE /Zi)
    target_link_options(libetude PRIVATE /DEBUG:FULL)
endif()
```

### 문제 2: ETW 이벤트가 보이지 않음

**증상:** Windows Performance Toolkit에서 이벤트 확인 안됨

**원인:** ETW 프로바이더 등록 실패

**해결 방법:**
1. 관리자 권한으로 프로바이더 등록:
```cmd
wevtutil im LibEtudeETW.man
```

2. 프로바이더 상태 확인:
```c
ETResult etw_result = et_windows_register_etw_provider();
if (etw_result != ET_SUCCESS) {
    printf("ETW 프로바이더 등록 실패: %d\n", etw_result);
}
```

## 배포 관련 문제

### 문제 1: NuGet 패키지 생성 실패

**증상:**
```
NuGet pack failed: Missing required metadata
```

**원인:** NuSpec 파일 설정 오류

**해결 방법:**
1. NuSpec 파일 확인:
```xml
<package>
  <metadata>
    <id>LibEtude</id>
    <version>1.0.0</version>
    <authors>LibEtude Project</authors>
    <description>AI Voice Synthesis Engine</description>
  </metadata>
</package>
```

2. 빌드 스크립트 실행:
```cmd
scripts\build_nuget.bat
```

### 문제 2: DLL 의존성 문제

**증상:** 애플리케이션 실행 시 DLL 찾을 수 없음

**원인:** 런타임 라이브러리 누락

**해결 방법:**
1. Visual C++ 재배포 패키지 설치
2. 정적 링크 사용:
```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

## 일반적인 해결 단계

### 1단계: 기본 확인사항
- [ ] Windows 10 (1903 이상) 또는 Windows 11 사용
- [ ] Visual Studio 2019/2022 설치
- [ ] Windows SDK 최신 버전 설치
- [ ] 관리자 권한으로 실행 (필요시)

### 2단계: 빌드 환경 확인
- [ ] CMake 3.16 이상 설치
- [ ] 환경 변수 설정 확인
- [ ] 컴파일러 버전 확인

### 3단계: 런타임 환경 확인
- [ ] 오디오 디바이스 드라이버 업데이트
- [ ] Windows Audio 서비스 실행 상태 확인
- [ ] 바이러스 백신 소프트웨어 예외 설정

### 4단계: 로그 및 디버깅
- [ ] ETW 로깅 활성화
- [ ] Debug 빌드로 상세 정보 확인
- [ ] Windows 이벤트 로그 확인

### 5단계: 커뮤니티 지원
문제가 해결되지 않으면:
- GitHub Issues에 문제 보고
- 시스템 정보와 로그 파일 첨부
- 재현 가능한 최소 예제 제공

## 추가 리소스

- [Microsoft 문서: WASAPI](https://docs.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- [Intel 개발자 가이드: AVX 최적화](https://software.intel.com/content/www/us/en/develop/articles/introduction-to-intel-advanced-vector-extensions.html)
- [Windows Performance Toolkit](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/)
- [Visual Studio 디버깅 가이드](https://docs.microsoft.com/en-us/visualstudio/debugger/)