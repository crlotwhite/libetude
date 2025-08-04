# LibEtude Windows 빌드 시스템 및 배포 테스트

이 디렉토리는 LibEtude의 Windows 빌드 시스템 및 배포 관련 테스트를 포함합니다.

## 작업 9.3: 빌드 시스템 및 배포 테스트 작성

### 개요

이 작업은 다음 요구사항을 만족하는 테스트를 구현합니다:

- **요구사항 1.1**: Windows 특화 컴파일러 플래그와 최적화 적용
- **요구사항 1.2**: Visual Studio 또는 MinGW 컴파일러에 맞는 최적화 설정 자동 선택
- **요구사항 5.2**: NuGet 패키지로 배포
- **요구사항 5.3**: CMake find_package 지원

### 테스트 파일

#### 1. `test_windows_build_system_integration.c`

Visual Studio 2019/2022 및 MinGW 빌드 시스템의 통합 테스트를 수행합니다.

**테스트 내용:**
- 컴파일러 감지 (MSVC, MinGW)
- Windows SDK 감지 및 호환성 확인
- CMake 빌드 시스템 테스트
- 컴파일러 최적화 플래그 테스트
- Windows 라이브러리 링크 테스트

**실행 방법:**
```bash
# CMake 빌드 후
./tests/unit/windows/Release/windows_build_system_integration_test.exe
```

#### 2. `test_windows_deployment_validation.c`

NuGet 패키지 생성 및 CMake 통합 테스트를 수행합니다.

**테스트 내용:**
- NuGet 도구 가용성 확인
- NuGet 패키지 구조 검증
- CMake find_package 통합 테스트
- 배포 패키지 검증
- 멀티 플랫폼 지원 테스트

**실행 방법:**
```bash
# CMake 빌드 후
./tests/unit/windows/Release/windows_deployment_validation_test.exe
```

### 자동화 스크립트

#### 1. `scripts/test_windows_build_automation.bat`

전체 Windows 빌드 프로세스를 자동으로 테스트하는 스크립트입니다.

**기능:**
- 개발 환경 검증
- Visual Studio 2019/2022 빌드 테스트
- MinGW 빌드 테스트
- 멀티 플랫폼 빌드 테스트 (x64, Win32, ARM64)
- 단위 테스트 실행
- NuGet 패키지 생성 테스트
- CMake 통합 테스트
- 성능 벤치마크 (선택사항)
- HTML 보고서 생성

**실행 방법:**
```bash
scripts/test_windows_build_automation.bat
```

#### 2. `scripts/run_build_system_tests.bat`

작업 9.3의 모든 빌드 시스템 테스트를 실행하는 통합 스크립트입니다.

**기능:**
- 환경 검증
- 프로젝트 빌드
- 단위 테스트 실행
- 통합 스크립트 테스트
- CTest 통합 테스트
- 결과 요약 및 보고서 생성

**실행 방법:**
```bash
scripts/run_build_system_tests.bat
```

### CMake 통합

테스트는 CMake 빌드 시스템에 통합되어 있습니다:

```cmake
# Windows 빌드 시스템 통합 테스트
add_executable(windows_build_system_integration_test
    unit/windows/test_windows_build_system_integration.c)

# Windows 배포 검증 테스트
add_executable(windows_deployment_validation_test
    unit/windows/test_windows_deployment_validation.c)
```

### CTest 실행

```bash
# 모든 빌드 시스템 테스트 실행
ctest -L "build"

# 모든 배포 테스트 실행
ctest -L "deployment"

# Windows 특화 테스트 실행
ctest -L "windows"

# 특정 테스트 실행
ctest -R "WindowsBuildSystemIntegrationTest"
ctest -R "WindowsDeploymentValidationTest"
```

### 테스트 결과

테스트 실행 후 다음 위치에서 결과를 확인할 수 있습니다:

- **로그 파일**: `test_results/build_system_tests.log`
- **결과 요약**: `test_results/build_system_test_results.txt`
- **HTML 보고서**: `build_test_results/build_test_report.html` (자동화 스크립트 실행 시)

### 요구사항 검증

#### 요구사항 1.1: Windows 특화 컴파일러 플래그와 최적화

- ✅ MSVC 최적화 플래그 테스트 (`/O2 /Oi /Ot /Oy /arch:AVX2`)
- ✅ MinGW 최적화 플래그 테스트 (`-O3 -march=native -mavx2`)
- ✅ Windows 특화 정의 테스트 (`WIN32_LEAN_AND_MEAN`, `_CRT_SECURE_NO_WARNINGS`)

#### 요구사항 1.2: 컴파일러별 최적화 설정 자동 선택

- ✅ MSVC 컴파일러 자동 감지 및 설정
- ✅ MinGW 컴파일러 자동 감지 및 설정
- ✅ 컴파일러별 최적화 플래그 자동 적용

#### 요구사항 5.2: NuGet 패키지 배포

- ✅ NuGet 패키지 구조 검증
- ✅ MSBuild targets/props 파일 생성
- ✅ 멀티 플랫폼 라이브러리 패키징
- ✅ NuGet 패키지 생성 테스트

#### 요구사항 5.3: CMake find_package 지원

- ✅ LibEtudeConfig.cmake 파일 생성
- ✅ find_package 통합 테스트
- ✅ 가져온 타겟 생성 테스트
- ✅ 플랫폼별 라이브러리 설정

### 문제 해결

#### 일반적인 문제

1. **Visual Studio를 찾을 수 없음**
   - Visual Studio 2019 또는 2022가 설치되어 있는지 확인
   - C++ 개발 도구가 설치되어 있는지 확인

2. **CMake를 찾을 수 없음**
   - CMake 3.16 이상이 설치되어 있는지 확인
   - PATH 환경 변수에 CMake가 포함되어 있는지 확인

3. **NuGet 도구를 찾을 수 없음**
   - NuGet CLI 또는 .NET SDK가 설치되어 있는지 확인
   - Visual Studio에 NuGet 패키지 관리자가 설치되어 있는지 확인

4. **Windows SDK를 찾을 수 없음**
   - Windows 10/11 SDK가 설치되어 있는지 확인
   - Visual Studio Installer를 통해 Windows SDK 설치

#### 테스트 실패 시

1. **빌드 실패**
   - 로그 파일에서 구체적인 오류 메시지 확인
   - 필요한 라이브러리와 헤더 파일이 있는지 확인

2. **테스트 실행 실패**
   - 실행 파일이 올바르게 생성되었는지 확인
   - 필요한 DLL 파일이 PATH에 있는지 확인

3. **패키지 생성 실패**
   - NuGet 도구가 올바르게 설치되어 있는지 확인
   - 패키지 구조와 메타데이터가 올바른지 확인

### 기여 방법

새로운 빌드 시스템 테스트를 추가하려면:

1. `tests/unit/windows/` 디렉토리에 테스트 파일 추가
2. `tests/CMakeLists.txt`에 테스트 타겟 추가
3. 적절한 라벨과 속성 설정
4. 문서 업데이트

### 참고 자료

- [CMake 문서](https://cmake.org/documentation/)
- [NuGet 패키지 생성 가이드](https://docs.microsoft.com/en-us/nuget/create-packages/)
- [Visual Studio 빌드 도구](https://docs.microsoft.com/en-us/cpp/build/)
- [MinGW-w64 문서](https://www.mingw-w64.org/)