# LibEtude Windows 개발 도구 통합 예제

이 예제는 LibEtude의 Windows 특화 개발 도구 통합 기능을 보여줍니다. Windows 환경에서 LibEtude를 효과적으로 개발하고 디버깅하는 방법을 학습할 수 있습니다.

## 개요

이 예제는 다음과 같은 Windows 특화 기능들을 실제로 사용해볼 수 있습니다:

- **ETW (Event Tracing for Windows)**: 성능 분석 및 디버깅
- **Visual Studio 통합**: 완전한 디버깅 환경
- **Windows 보안 기능**: DEP, ASLR, UAC 호환성
- **성능 최적화**: SIMD, Thread Pool, Large Pages
- **오디오 시스템**: WASAPI 및 DirectSound 통합

## 주요 기능

### 1. ETW (Event Tracing for Windows) 지원
- 성능 이벤트 로깅 및 실시간 모니터링
- 오류 이벤트 추적 및 분석
- 메모리 할당/해제 패턴 추적
- 오디오 시스템 이벤트 모니터링
- 스레드 생명주기 및 동기화 추적
- Windows Performance Toolkit과 완전 호환

### 2. Visual Studio 디버깅 지원
- PDB 파일 자동 생성 및 최적화
- 상세한 스택 트레이스 및 호출 그래프
- Windows 이벤트 로그 통합
- 실시간 성능 타이머 및 프로파일링
- 메모리 사용량 실시간 모니터링
- CPU 및 시스템 리소스 정보 수집

### 3. Windows 보안 기능 데모
- DEP (Data Execution Prevention) 호환성 테스트
- ASLR (Address Space Layout Randomization) 지원
- UAC (User Account Control) 권한 관리
- 보안 메모리 할당 및 관리

### 4. 성능 최적화 기능
- SIMD 명령어 (AVX2/AVX-512) 활용 데모
- Windows Thread Pool API 사용 예제
- Large Page 메모리 할당 및 성능 측정
- CPU 기능 감지 및 최적화 적용

## 시스템 요구사항

### 최소 요구사항
- Windows 10 (버전 1903 이상) 또는 Windows 11
- Visual Studio 2019 또는 2022 (Community/Professional/Enterprise)
- CMake 3.16 이상
- Windows 10 SDK (최신 버전 권장)

### 권장 요구사항
- Intel/AMD 프로세서 (AVX2 지원)
- 8GB 이상 RAM
- 관리자 권한 (Large Page 및 ETW 기능 사용 시)
- Windows Performance Toolkit (성능 분석 시)

## 빌드 방법

### Visual Studio 2022 사용 (권장)
```cmd
# 프로젝트 루트에서
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug

# Release 빌드 (성능 테스트용)
cmake --build . --config Release
```

### Visual Studio 2019 사용
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```

### MinGW 사용
```cmd
mkdir build-mingw
cd build-mingw
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### 빌드 옵션

다음 CMake 옵션을 사용하여 특정 기능을 활성화/비활성화할 수 있습니다:

```cmd
# ETW 로깅 활성화
cmake .. -DENABLE_ETW_LOGGING=ON

# Large Page 지원 활성화
cmake .. -DENABLE_LARGE_PAGES=ON

# 성능 최적화 활성화
cmake .. -DENABLE_PERFORMANCE_OPTIMIZATION=ON
```

## 실행 방법

```cmd
# 빌드 디렉토리에서
.\examples\windows_dev_tools_example.exe
```

## ETW 이벤트 수집

Windows Performance Toolkit을 사용하여 ETW 이벤트를 수집할 수 있습니다:

```cmd
# ETW 세션 시작
wpr -start GeneralProfile

# 예제 실행
.\examples\windows_dev_tools_example.exe

# ETW 세션 중지 및 로그 저장
wpr -stop libetude_trace.etl

# Windows Performance Analyzer로 분석
wpa libetude_trace.etl
```

## Visual Studio에서 디버깅

1. Visual Studio에서 프로젝트 열기
2. Debug 모드로 빌드
3. F5를 눌러 디버깅 시작
4. 브레이크포인트 설정 및 스택 트레이스 확인

## 로그 파일

예제 실행 시 다음 로그 파일이 생성됩니다:
- `libetude_example.log`: 상세한 디버그 로그
- Windows 이벤트 로그: "LibEtude" 소스로 이벤트 기록

## 주요 출력 정보

### 성능 측정
- 작업 실행 시간 측정
- ETW 및 디버그 타이머 비교

### 오류 처리
- 구조화된 오류 정보
- 스택 트레이스 포함

### 메모리 추적
- 할당/해제 이벤트
- 현재 메모리 사용량

### 시스템 정보
- OS 버전
- CPU 정보
- 메모리 상태

## 문제 해결

### ETW 이벤트가 보이지 않는 경우
- 관리자 권한으로 실행
- Windows Performance Toolkit 설치 확인

### PDB 파일이 생성되지 않는 경우
- Debug 모드로 빌드 확인
- Visual Studio 프로젝트 설정 확인

### 이벤트 로그 등록 실패
- 관리자 권한으로 실행
- Windows 이벤트 로그 서비스 상태 확인

## 참고 자료

- [ETW (Event Tracing for Windows)](https://docs.microsoft.com/en-us/windows/win32/etw/event-tracing-portal)
- [Windows Performance Toolkit](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/)
- [Visual Studio 디버깅](https://docs.microsoft.com/en-us/visualstudio/debugger/)
- [PDB 파일](https://docs.microsoft.com/en-us/windows/win32/debug/symbol-files)
#
# 추가 문서

이 예제와 관련된 자세한 문서들:

- **[Windows 지원 가이드](../../docs/windows/README.md)**: 전체적인 Windows 지원 개요
- **[성능 최적화 가이드](../../docs/windows/performance_optimization.md)**: 성능 튜닝 방법
- **[문제 해결 가이드](../../docs/windows/troubleshooting.md)**: 일반적인 문제 해결
- **[Visual Studio 템플릿](../../docs/windows/visual_studio_template.md)**: 새 프로젝트 생성 방법

## 학습 순서

이 예제를 효과적으로 학습하려면 다음 순서를 권장합니다:

1. **기본 실행**: 예제를 빌드하고 실행하여 기본 동작 확인
2. **ETW 로깅**: Windows Performance Toolkit으로 성능 데이터 수집
3. **Visual Studio 디버깅**: 브레이크포인트와 스택 트레이스 활용
4. **성능 최적화**: 다양한 최적화 옵션 테스트
5. **보안 기능**: Windows 보안 기능과의 호환성 확인

## 성능 벤치마크

이 예제를 통해 다음과 같은 성능 지표를 측정할 수 있습니다:

| 기능 | 측정 항목 | 예상 결과 |
|------|-----------|-----------|
| SIMD 최적화 | 행렬 연산 속도 | 2-4배 향상 |
| Thread Pool | 병렬 처리 효율 | CPU 코어 수에 비례 |
| Large Pages | 메모리 접근 속도 | 5-15% 향상 |
| WASAPI | 오디오 지연 시간 | 10ms 이하 |

## 커뮤니티 및 지원

- **GitHub Issues**: [버그 리포트 및 기능 요청](https://github.com/your-org/libetude/issues)
- **토론**: [GitHub Discussions](https://github.com/your-org/libetude/discussions)
- **문서**: [전체 문서 사이트](https://libetude.readthedocs.io/)

## 라이선스

이 예제는 LibEtude 프로젝트의 라이선스를 따릅니다.