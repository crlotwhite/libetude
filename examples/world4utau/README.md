# world4utau - UTAU 호환 음성 합성 엔진

world4utau는 libetude 라이브러리를 기반으로 한 UTAU 호환 음성 합성 엔진입니다. WORLD 보코더 알고리즘을 libetude의 최적화된 DSP 및 오디오 처리 기능과 통합하여 고성능 UTAU resampler를 제공합니다.

## 특징

- **UTAU 완전 호환**: 기존 UTAU 도구들과 완전히 호환되는 명령줄 인터페이스
- **WORLD 보코더**: 고품질 음성 분석 및 합성을 위한 WORLD 알고리즘 구현
- **libetude 통합**: SIMD 최적화, GPU 가속, 크로스 플랫폼 지원
- **실시간 성능**: 최적화된 메모리 관리와 병렬 처리로 실시간 성능 달성
- **캐싱 시스템**: 분석 결과 캐싱으로 반복 처리 시간 단축

## 빌드 방법

### 전제 조건

- CMake 3.16 이상
- C11 호환 컴파일러 (GCC, Clang, MSVC)
- libetude 라이브러리

### 빌드 명령

```bash
# 프로젝트 루트에서
mkdir build
cd build
cmake ..
make world4utau

# 또는 전체 빌드
cmake --build . --target world4utau
```

### 플랫폼별 빌드

#### Windows
```cmd
cmake -B build -G "Visual Studio 16 2019"
cmake --build build --config Release --target world4utau
```

#### macOS
```bash
cmake -B build -G Xcode
cmake --build build --config Release --target world4utau
```

#### Linux
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target world4utau
```

## 사용법

### 기본 사용법

```bash
world4utau <입력.wav> <출력.wav> <목표_피치> <벨로시티>
```

### 예제

```bash
# 기본 사용
./world4utau voice.wav output.wav 440.0 100

# 피치 벤드 적용
./world4utau voice.wav output.wav 440.0 100 --pitch-bend pitch.txt

# 볼륨 및 모듈레이션 조절
./world4utau voice.wav output.wav 440.0 100 --volume 0.8 --modulation 0.2

# 상세 출력 모드
./world4utau voice.wav output.wav 440.0 100 --verbose
```

### 명령줄 옵션

| 옵션 | 설명 | 기본값 |
|------|------|--------|
| `-p, --pitch-bend FILE` | 피치 벤드 파일 경로 | - |
| `-v, --volume FLOAT` | 볼륨 (0.0-2.0) | 1.0 |
| `-m, --modulation FLOAT` | 모듈레이션 강도 (0.0-1.0) | 0.0 |
| `-c, --consonant INT` | 자음 벨로시티 (0-100) | 100 |
| `-u, --pre-utterance FLOAT` | 선행발성 (ms) | 0.0 |
| `-o, --overlap FLOAT` | 오버랩 (ms) | 0.0 |
| `-s, --start-point FLOAT` | 시작점 (ms) | 0.0 |
| `-r, --sample-rate INT` | 샘플링 레이트 (Hz) | 44100 |
| `-b, --bit-depth INT` | 비트 깊이 (16/24/32) | 16 |
| `-n, --no-cache` | 캐시 비활성화 | false |
| `-x, --no-optimization` | 최적화 비활성화 | false |
| `-V, --verbose` | 상세 출력 모드 | false |
| `-h, --help` | 도움말 출력 | - |
| `-w, --version` | 버전 정보 출력 | - |

## UTAU 도구 연동

### OpenUTAU

OpenUTAU에서 world4utau를 resampler로 사용하려면:

1. world4utau 실행 파일을 OpenUTAU의 `Resamplers` 폴더에 복사
2. OpenUTAU 설정에서 resampler로 선택
3. 기존 world4utau와 동일한 방식으로 사용

### 기존 UTAU

기존 UTAU에서도 world4utau를 resampler로 사용할 수 있습니다:

1. world4utau 실행 파일을 UTAU 설치 폴더에 복사
2. `tool2.exe`로 이름 변경 (또는 설정에서 지정)
3. 기존과 동일한 방식으로 사용

## 성능 최적화

### SIMD 최적화

world4utau는 다음 SIMD 명령어 세트를 지원합니다:

- **x86/x64**: SSE, SSE2, AVX, AVX2
- **ARM**: NEON

### GPU 가속

지원되는 GPU 가속:

- **NVIDIA**: CUDA
- **AMD**: OpenCL
- **Apple**: Metal
- **Intel**: OpenCL

### 메모리 최적화

- 메모리 풀 기반 할당으로 메모리 단편화 방지
- 캐시 친화적 데이터 구조
- 스트리밍 처리로 메모리 사용량 최소화

## 개발자 정보

### 프로젝트 구조

```
examples/world4utau/
├── CMakeLists.txt          # CMake 빌드 설정
├── README.md               # 이 파일
├── main.c                  # 메인 애플리케이션
├── include/                # 헤더 파일
│   ├── utau_interface.h    # UTAU 호환 인터페이스
│   └── world_engine.h      # WORLD 엔진 인터페이스
└── src/                    # 소스 파일
    ├── utau_interface.c    # UTAU 인터페이스 구현
    └── world_engine.c      # WORLD 엔진 구현
```

### API 문서

자세한 API 문서는 헤더 파일의 Doxygen 주석을 참조하세요.

### 기여하기

1. 이슈 리포트: GitHub Issues 사용
2. 코드 기여: Pull Request 제출
3. 문서 개선: README 및 코드 주석 개선

## 라이선스

이 프로젝트는 libetude 프로젝트의 라이선스를 따릅니다.

## 변경 이력

### v1.0.0 (개발 중)

- 초기 구현
- UTAU 호환 인터페이스
- WORLD 보코더 기본 구조
- libetude 통합

## 문제 해결

### 일반적인 문제

1. **빌드 오류**: libetude 라이브러리가 올바르게 설치되었는지 확인
2. **성능 문제**: SIMD 최적화가 활성화되었는지 확인
3. **호환성 문제**: UTAU 도구의 버전과 설정 확인

### 디버그 모드

상세한 디버그 정보를 보려면 `--verbose` 옵션을 사용하세요:

```bash
./world4utau voice.wav output.wav 440.0 100 --verbose
```

### 로그 파일

로그 파일은 다음 위치에 저장됩니다:

- **Windows**: `%APPDATA%/world4utau/logs/`
- **macOS**: `~/Library/Logs/world4utau/`
- **Linux**: `~/.local/share/world4utau/logs/`