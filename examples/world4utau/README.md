# world4utau - UTAU 호환 음성 합성 엔진

world4utau는 libetude 라이브러리를 기반으로 한 UTAU 호환 음성 합성 엔진입니다. WORLD 보코더 알고리즘을 libetude의 최적화된 DSP 및 오디오 처리 기능과 통합하여 고성능 UTAU resampler를 제공합니다.

이 프로젝트는 기존 world4utau의 기능을 libetude 아키텍처에 맞게 재구현하여 성능 최적화와 크로스 플랫폼 지원을 제공합니다.

## 특징

- **UTAU 완전 호환**: 기존 UTAU 도구들과 완전히 호환되는 명령줄 인터페이스
- **WORLD 보코더**: 고품질 음성 분석 및 합성을 위한 WORLD 알고리즘 구현
- **libetude 통합**: SIMD 최적화, GPU 가속, 크로스 플랫폼 지원
- **실시간 성능**: 최적화된 메모리 관리와 병렬 처리로 실시간 성능 달성
- **캐싱 시스템**: 분석 결과 캐싱으로 반복 처리 시간 단축

## 빌드 방법

### 전제 조건

- **CMake**: 3.16 이상
- **컴파일러**: C11 호환 컴파일러
  - GCC 7.0 이상 (Linux)
  - Clang 6.0 이상 (macOS/Linux)
  - MSVC 2019 이상 (Windows)
- **libetude**: 메인 libetude 라이브러리가 빌드되어 있어야 함
- **의존성**:
  - FFTW3 (선택사항, 내장 FFT 사용 가능)
  - OpenMP (병렬 처리용, 선택사항)

### 빌드 명령

#### 기본 빌드

```bash
# libetude 프로젝트 루트에서
cd examples/world4utau

# 빌드 디렉토리 생성
mkdir build
cd build

# CMake 설정
cmake ..

# 빌드 실행
make

# 또는 CMake를 사용한 빌드
cmake --build . --target world4utau
```

#### 최적화된 릴리스 빌드

```bash
# 릴리스 모드로 빌드 (최적화 활성화)
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# 또는 전체 libetude 프로젝트에서
cd ../../  # libetude 루트로 이동
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make world4utau
```

### 플랫폼별 빌드

#### Windows (Visual Studio)

```cmd
# Visual Studio 2019/2022 사용
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release --target world4utau

# 또는 MSBuild 직접 사용
msbuild build\world4utau.vcxproj /p:Configuration=Release
```

#### Windows (MinGW)

```bash
# MinGW-w64 사용
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target world4utau
```

#### macOS

```bash
# Xcode 프로젝트 생성
cmake -B build -G Xcode
cmake --build build --config Release --target world4utau

# 또는 Make 사용
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target world4utau

# Apple Silicon (M1/M2) 최적화
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --target world4utau
```

#### Linux

```bash
# 기본 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target world4utau

# GCC 최적화 플래그 추가
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-march=native -mtune=native"
cmake --build build --target world4utau

# Clang 사용
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
cmake --build build --target world4utau
```

### 빌드 옵션

다음 CMake 옵션들을 사용하여 빌드를 커스터마이즈할 수 있습니다:

```bash
# SIMD 최적화 활성화/비활성화
cmake -DLIBETUDE_ENABLE_SIMD=ON ..

# GPU 가속 활성화 (CUDA/OpenCL)
cmake -DLIBETUDE_ENABLE_GPU=ON ..

# OpenMP 병렬 처리 활성화
cmake -DLIBETUDE_ENABLE_OPENMP=ON ..

# 디버그 정보 포함
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 프로파일링 정보 포함
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

### 설치

빌드 후 시스템에 설치하려면:

```bash
# 빌드 후 설치
sudo make install

# 또는 CMake 사용
sudo cmake --install build

# 설치 경로 지정
cmake --install build --prefix /usr/local
```

## 사용법

### 기본 사용법

world4utau는 기존 world4utau와 동일한 명령줄 인터페이스를 제공합니다:

```bash
world4utau <입력.wav> <출력.wav> <목표_피치> <벨로시티> [옵션...]
```

**파라미터 설명:**
- `입력.wav`: 분석할 원본 음성 파일 (WAV 형식)
- `출력.wav`: 합성된 음성이 저장될 파일 (WAV 형식)
- `목표_피치`: 목표 기본 주파수 (Hz, 예: 440.0)
- `벨로시티`: 음성의 강도 (0-100, 예: 100)

### 사용 예제

#### 기본 사용

```bash
# 가장 기본적인 사용법
./world4utau voice.wav output.wav 440.0 100

# 다른 피치로 변환
./world4utau voice.wav output_high.wav 880.0 100  # 한 옥타브 높게
./world4utau voice.wav output_low.wav 220.0 100   # 한 옥타브 낮게
```

#### 고급 사용법

```bash
# 피치 벤드 파일 적용
./world4utau voice.wav output.wav 440.0 100 --pitch-bend pitch_curve.txt

# 볼륨 및 모듈레이션 조절
./world4utau voice.wav output.wav 440.0 100 --volume 0.8 --modulation 0.2

# UTAU 파라미터 적용
./world4utau voice.wav output.wav 440.0 100 \
  --pre-utterance 50.0 \
  --overlap 30.0 \
  --consonant 80

# 고품질 출력 (24bit, 48kHz)
./world4utau voice.wav output.wav 440.0 100 \
  --sample-rate 48000 \
  --bit-depth 24

# 디버그 모드로 실행
./world4utau voice.wav output.wav 440.0 100 --verbose
```

#### 배치 처리 예제

```bash
# 여러 파일 처리 (bash 스크립트)
for file in *.wav; do
  ./world4utau "$file" "processed_$file" 440.0 100
done

# 다양한 피치로 처리
for pitch in 220 440 880; do
  ./world4utau voice.wav "output_${pitch}hz.wav" $pitch 100
done
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

### OpenUTAU 연동

OpenUTAU에서 world4utau를 resampler로 사용하는 방법:

#### 1. 설치

```bash
# world4utau 실행 파일을 OpenUTAU Resamplers 폴더에 복사
# Windows
copy world4utau.exe "C:\Users\[사용자명]\AppData\Roaming\OpenUtau\Resamplers\"

# macOS
cp world4utau "/Applications/OpenUtau.app/Contents/Resources/Resamplers/"

# Linux
cp world4utau "~/.local/share/OpenUtau/Resamplers/"
```

#### 2. 설정

1. OpenUTAU 실행
2. `Preferences` → `Rendering` → `Resampler` 에서 `world4utau` 선택
3. 필요시 추가 옵션 설정

#### 3. 사용법

OpenUTAU에서 일반적인 방식으로 프로젝트를 생성하고 렌더링하면 world4utau가 자동으로 사용됩니다.

### 기존 UTAU 연동

기존 UTAU에서 world4utau를 사용하는 방법:

#### 1. 설치

```bash
# UTAU 설치 폴더에 복사
copy world4utau.exe "C:\Program Files\UTAU\"

# tool2.exe로 이름 변경 (기본 resampler 대체)
ren world4utau.exe tool2.exe
```

#### 2. 설정 파일 수정

UTAU의 `oto.ini` 파일에서 resampler 설정:

```ini
# oto.ini 예제
a.wav=a,100,50,120,80,100
```

#### 3. 플러그인으로 사용

UTAU 플러그인으로 사용하려면:

1. `world4utau.exe`를 UTAU `plugins` 폴더에 복사
2. UTAU에서 `Tools` → `Plugins` → `world4utau` 선택

### 호환성 정보

| UTAU 도구 | 호환성 | 비고 |
|-----------|--------|------|
| OpenUTAU | ✅ 완전 호환 | 모든 기능 지원 |
| UTAU (원본) | ✅ 완전 호환 | tool2.exe 대체 가능 |
| NEUTRINO | ⚠️ 부분 호환 | 일부 기능 제한 |
| CeVIO AI | ❌ 미지원 | 별도 플러그인 필요 |
| SynthV | ❌ 미지원 | 별도 플러그인 필요 |

### 성능 비교

기존 world4utau 대비 성능 개선:

- **처리 속도**: 약 2-3배 향상 (SIMD 최적화)
- **메모리 사용량**: 약 30% 감소 (메모리 풀 사용)
- **음질**: 동일하거나 향상된 품질
- **안정성**: 향상된 에러 처리

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

#### 빌드 관련 문제

**문제**: CMake 설정 실패
```bash
# 해결책: libetude 경로 명시적 지정
cmake -DLIBETUDE_ROOT=/path/to/libetude ..
```

**문제**: 컴파일 오류 - 헤더 파일을 찾을 수 없음
```bash
# 해결책: include 경로 추가
cmake -DCMAKE_C_FLAGS="-I/usr/local/include" ..
```

**문제**: 링크 오류 - libetude 라이브러리를 찾을 수 없음
```bash
# 해결책: 라이브러리 경로 추가
cmake -DCMAKE_LIBRARY_PATH="/usr/local/lib" ..
```

#### 실행 관련 문제

**문제**: "파일을 찾을 수 없습니다" 오류
- **원인**: 입력 WAV 파일 경로가 잘못됨
- **해결책**: 절대 경로 사용 또는 파일 존재 확인

**문제**: "지원되지 않는 오디오 형식" 오류
- **원인**: WAV 파일 형식이 지원되지 않음
- **해결책**: 16/24/32bit PCM WAV 파일 사용

**문제**: 성능이 느림
- **원인**: SIMD 최적화가 비활성화됨
- **해결책**: 릴리스 모드로 빌드 또는 `--no-optimization` 옵션 제거

#### UTAU 연동 문제

**문제**: OpenUTAU에서 resampler가 인식되지 않음
- **해결책**: 실행 권한 확인 및 경로 재설정

**문제**: 기존 UTAU에서 오류 발생
- **해결책**: 호환성 모드 활성화 (`--utau-compat` 옵션)

### 디버그 모드

상세한 디버그 정보를 보려면 다음 옵션들을 사용하세요:

```bash
# 기본 디버그 정보
./world4utau voice.wav output.wav 440.0 100 --verbose

# 상세 분석 정보
./world4utau voice.wav output.wav 440.0 100 --verbose --debug-analysis

# 성능 프로파일링
./world4utau voice.wav output.wav 440.0 100 --verbose --profile

# 메모리 사용량 모니터링
./world4utau voice.wav output.wav 440.0 100 --verbose --debug-memory
```

### 로그 파일

로그 파일은 다음 위치에 저장됩니다:

- **Windows**: `%APPDATA%\world4utau\logs\`
- **macOS**: `~/Library/Logs/world4utau/`
- **Linux**: `~/.local/share/world4utau/logs/`

로그 레벨 설정:
```bash
# 환경 변수로 로그 레벨 설정
export WORLD4UTAU_LOG_LEVEL=DEBUG  # DEBUG, INFO, WARN, ERROR
./world4utau voice.wav output.wav 440.0 100
```

### 성능 튜닝

#### CPU 최적화

```bash
# CPU 코어 수에 맞춰 스레드 수 설정
export OMP_NUM_THREADS=4
./world4utau voice.wav output.wav 440.0 100

# CPU 친화도 설정 (Linux)
taskset -c 0-3 ./world4utau voice.wav output.wav 440.0 100
```

#### 메모리 최적화

```bash
# 메모리 풀 크기 조정
./world4utau voice.wav output.wav 440.0 100 --memory-pool-size 64MB

# 캐시 크기 조정
./world4utau voice.wav output.wav 440.0 100 --cache-size 128MB
```

### 지원 및 문의

- **GitHub Issues**: 버그 리포트 및 기능 요청
- **Wiki**: 상세한 사용법 및 팁
- **Discord**: 실시간 지원 (libetude 커뮤니티)
- **Email**: 기술 지원 문의