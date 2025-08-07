# LibEtude 설치 가이드

LibEtude는 Windows, macOS, Linux를 지원하는 크로스 플랫폼 음성 합성 엔진입니다. 이 가이드는 각 플랫폼에서 LibEtude를 설치하고 사용하는 방법을 설명합니다.

## 시스템 요구사항

### 최소 요구사항
- **메모리**: 4GB RAM
- **저장공간**: 500MB 여유 공간
- **프로세서**: x86_64 또는 ARM64

### 플랫폼별 요구사항

#### Windows
- Windows 7 이상 (x64)
- Windows 10 이상 (ARM64)
- Visual C++ 재배포 가능 패키지 2019 이상

#### macOS
- macOS 12.0 (Monterey) 이상
- Intel x64 또는 Apple Silicon (M1/M2) 프로세서

#### Linux
- Ubuntu 18.04 이상 / CentOS 7 이상
- glibc 2.17 이상
- ALSA 또는 PulseAudio (오디오 기능 사용 시)

## 설치 방법

### 1. 사전 빌드된 패키지 설치 (권장)

#### Windows

**NSIS 설치 프로그램 사용:**
1. [릴리스 페이지](https://github.com/libetude/libetude/releases)에서 최신 `libetude-x.x.x-win64.exe` 다운로드
2. 설치 프로그램 실행
3. 설치 마법사 지시에 따라 설치

**ZIP 패키지 사용:**
1. [릴리스 페이지](https://github.com/libetude/libetude/releases)에서 최신 `libetude-x.x.x-win64.zip` 다운로드
2. 원하는 위치에 압축 해제
3. 환경 변수 PATH에 `bin` 디렉토리 추가

#### macOS

**DMG 패키지 사용:**
1. [릴리스 페이지](https://github.com/libetude/libetude/releases)에서 최신 `libetude-x.x.x-Darwin.dmg` 다운로드
2. DMG 파일을 더블클릭하여 마운트
3. LibEtude.app을 Applications 폴더로 드래그

**Homebrew 사용 (예정):**
```bash
brew install libetude/tap/libetude
```

**수동 설치:**
```bash
# TGZ 패키지 다운로드 및 설치
curl -L https://github.com/libetude/libetude/releases/download/vx.x.x/libetude-x.x.x-Darwin.tar.gz | tar xz
sudo cp -r libetude-x.x.x/* /usr/local/
```

#### Linux

**Ubuntu/Debian (DEB 패키지):**
```bash
# DEB 패키지 다운로드 및 설치
wget https://github.com/libetude/libetude/releases/download/vx.x.x/libetude_x.x.x_amd64.deb
sudo dpkg -i libetude_x.x.x_amd64.deb
sudo apt-get install -f  # 의존성 해결
```

**CentOS/RHEL/Fedora (RPM 패키지):**
```bash
# RPM 패키지 다운로드 및 설치
wget https://github.com/libetude/libetude/releases/download/vx.x.x/libetude-x.x.x-1.x86_64.rpm
sudo rpm -i libetude-x.x.x-1.x86_64.rpm
```

**범용 Linux (TGZ 패키지):**
```bash
# TGZ 패키지 다운로드 및 설치
wget https://github.com/libetude/libetude/releases/download/vx.x.x/libetude-x.x.x-Linux.tar.gz
tar xzf libetude-x.x.x-Linux.tar.gz
sudo cp -r libetude-x.x.x/* /usr/local/
sudo ldconfig  # 라이브러리 캐시 업데이트
```

### 2. 소스에서 빌드

#### 의존성 설치

**Windows:**
- Visual Studio 2019 이상 또는 Build Tools
- CMake 3.16 이상
- Git

**macOS:**
```bash
# Xcode Command Line Tools 설치
xcode-select --install

# Homebrew로 의존성 설치
brew install cmake git
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt install build-essential cmake git libasound2-dev
```

**Linux (CentOS/RHEL):**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake3 git alsa-lib-devel
```

#### 빌드 과정

```bash
# 소스 코드 클론
git clone https://github.com/libetude/libetude.git
cd libetude

# 빌드 디렉토리 생성
mkdir build && cd build

# CMake 설정
cmake -DCMAKE_BUILD_TYPE=Release \
      -DLIBETUDE_ENABLE_PLATFORM_ABSTRACTION=ON \
      -DLIBETUDE_ENABLE_SIMD=ON \
      -DLIBETUDE_BUILD_EXAMPLES=ON \
      ..

# 빌드 실행
cmake --build . --config Release --parallel

# 테스트 실행 (선택적)
ctest --output-on-failure

# 설치
sudo cmake --install .
```

#### 빌드 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `LIBETUDE_ENABLE_PLATFORM_ABSTRACTION` | ON | 플랫폼 추상화 레이어 활성화 |
| `LIBETUDE_ENABLE_SIMD` | OFF | SIMD 최적화 활성화 |
| `LIBETUDE_ENABLE_GPU` | OFF | GPU 가속 활성화 |
| `LIBETUDE_MINIMAL` | OFF | 최소 빌드 (임베디드용) |
| `LIBETUDE_BUILD_EXAMPLES` | OFF | 예제 애플리케이션 빌드 |
| `LIBETUDE_BUILD_TOOLS` | OFF | 개발 도구 빌드 |
| `LIBETUDE_BUILD_BINDINGS` | ON | 언어 바인딩 빌드 |

## 설치 확인

설치가 완료되면 다음 명령으로 설치를 확인할 수 있습니다:

```bash
# 라이브러리 버전 확인
libetude --version

# 플랫폼 추상화 레이어 기능 확인
libetude --platform-info

# 예제 실행 (예제가 설치된 경우)
libetude-example-basic
```

## 개발 환경 설정

### C/C++ 프로젝트에서 사용

#### pkg-config 사용 (Linux/macOS)
```bash
# 컴파일 플래그 확인
pkg-config --cflags libetude

# 링크 플래그 확인
pkg-config --libs libetude

# 예제 컴파일
gcc -o my_app my_app.c $(pkg-config --cflags --libs libetude)
```

#### CMake 사용
```cmake
# CMakeLists.txt
find_package(LibEtude REQUIRED)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE LibEtude::libetude)

# 플랫폼 추상화 레이어 기능 확인
libetude_require_platform_feature(AUDIO_ABSTRACTION)
```

#### 수동 설정
```bash
# 헤더 파일 위치
/usr/local/include/libetude/

# 라이브러리 파일 위치
/usr/local/lib/libetude.a      # 정적 라이브러리
/usr/local/lib/libetude.so     # 동적 라이브러리 (Linux)
/usr/local/lib/libetude.dylib  # 동적 라이브러리 (macOS)
```

### 언어별 바인딩

#### C++
```cpp
#include <libetude/cpp/engine.hpp>

LibEtude::Engine engine;
engine.initialize();
```

#### Java (Android)
```java
import com.libetude.Engine;

Engine engine = new Engine();
engine.initialize();
```

#### Objective-C (iOS)
```objc
#import <LibEtude/LibEtudeEngine.h>

LibEtudeEngine *engine = [[LibEtudeEngine alloc] init];
[engine initialize];
```

## 문제 해결

### 일반적인 문제

#### "라이브러리를 찾을 수 없음" 오류
```bash
# Linux에서 라이브러리 경로 확인
ldconfig -p | grep libetude

# 라이브러리 경로 추가 (필요시)
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# macOS에서 라이브러리 경로 확인
otool -L /usr/local/bin/libetude-example-basic
```

#### 플랫폼 추상화 레이어 오류
```bash
# 플랫폼 지원 확인
libetude --platform-info

# 디버그 모드로 실행
LIBETUDE_DEBUG=1 your_application
```

#### 오디오 관련 문제

**Linux:**
```bash
# ALSA 설치 확인
aplay -l

# PulseAudio 설치 확인
pulseaudio --check
```

**macOS:**
```bash
# 오디오 권한 확인 (macOS 10.14+)
# 시스템 환경설정 > 보안 및 개인 정보 보호 > 마이크
```

### 로그 및 디버깅

#### 디버그 로그 활성화
```bash
# 환경 변수로 디버그 로그 활성화
export LIBETUDE_LOG_LEVEL=DEBUG
export LIBETUDE_PLATFORM_DEBUG=1

# 애플리케이션 실행
your_application
```

#### 성능 프로파일링
```bash
# 성능 분석 도구 사용 (도구가 설치된 경우)
libetude-profiler --model your_model.lef --output profile.json
```

## 언인스톨

### Windows
- 제어판 > 프로그램 추가/제거에서 LibEtude 제거
- 또는 설치 디렉토리에서 `uninstall.exe` 실행

### macOS
```bash
# Homebrew로 설치한 경우
brew uninstall libetude

# 수동 설치한 경우
sudo rm -rf /usr/local/include/libetude
sudo rm -f /usr/local/lib/libetude*
sudo rm -f /usr/local/bin/libetude*
```

### Linux
```bash
# DEB 패키지로 설치한 경우
sudo apt remove libetude

# RPM 패키지로 설치한 경우
sudo rpm -e libetude

# 수동 설치한 경우
sudo make uninstall  # 빌드 디렉토리에서
```

## 지원 및 문의

- **문서**: [https://libetude.github.io/docs](https://libetude.github.io/docs)
- **이슈 트래커**: [https://github.com/libetude/libetude/issues](https://github.com/libetude/libetude/issues)
- **토론**: [https://github.com/libetude/libetude/discussions](https://github.com/libetude/libetude/discussions)
- **이메일**: support@libetude.org

## 라이선스

LibEtude는 MIT 라이선스 하에 배포됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.