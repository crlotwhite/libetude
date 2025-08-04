# LibEtude Windows 개발 도구 통합 예제

이 예제는 LibEtude의 Windows 특화 개발 도구 통합 기능을 보여줍니다.

## 기능 소개

### 1. ETW (Event Tracing for Windows) 지원
- 성능 이벤트 로깅
- 오류 이벤트 추적
- 메모리 할당/해제 추적
- 오디오 시스템 이벤트
- 스레드 생명주기 추적

### 2. Visual Studio 디버깅 지원
- PDB 파일 자동 생성
- 상세한 스택 트레이스
- Windows 이벤트 로그 통합
- 성능 타이머 및 프로파일링
- 메모리 사용량 모니터링
- 시스템 정보 수집

## 빌드 방법

### Visual Studio 사용
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Debug
```

### MinGW 사용
```cmd
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build .
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