# LibEtude 플러그인 확장 데모

이 데모 애플리케이션은 LibEtude 엔진의 플러그인 시스템과 확장 모델 기능을 보여줍니다.

## 주요 기능

- **플러그인 로딩 및 사용**: 동적 라이브러리 형태의 플러그인을 런타임에 로드/언로드
- **사용자 정의 효과 구현**: 오디오 효과 플러그인을 체인 형태로 연결하여 적용
- **확장 모델 적용**: LEFX 형식의 확장 모델을 기본 모델에 동적으로 적용
- **의존성 관리**: 플러그인 간의 의존성 확인 및 관리
- **실시간 파라미터 조정**: 플러그인 파라미터를 실시간으로 조정

## 아키텍처

### 플러그인 시스템
```
애플리케이션
    ↓
플러그인 매니저
    ↓
동적 라이브러리 로더 (dlopen/LoadLibrary)
    ↓
플러그인 인스턴스
    ↓
효과 체인 / 모델 확장
```

### 지원하는 플러그인 타입
1. **오디오 효과 플러그인**: 리버브, 이퀄라이저, 컴프레서 등
2. **음성 모델 플러그인**: 화자별 음성 특성 모델
3. **언어팩 플러그인**: 다국어 지원 확장
4. **사용자 정의 필터**: 커스텀 오디오 처리 필터

## 빌드 방법

```bash
# 프로젝트 루트에서
mkdir build && cd build
cmake ..
make libetude_plugin_extension_demo
```

## 실행 방법

```bash
# 기본 모델로 실행
./libetude_plugin_extension_demo

# 특정 모델 파일로 실행
./libetude_plugin_extension_demo path/to/model.lef
```

## 디렉토리 구조

```
plugin_extension/
├── main.c                    # 메인 애플리케이션
├── plugins/                  # 플러그인 디렉토리
│   ├── reverb_plugin.so      # 리버브 효과 플러그인
│   ├── equalizer_plugin.so   # 이퀄라이저 플러그인
│   ├── compressor_plugin.so  # 컴프레서 플러그인
│   └── voice_model.so        # 음성 모델 플러그인
├── extensions/               # 확장 모델 디렉토리
│   ├── female_speaker.lefx   # 여성 화자 확장
│   ├── male_speaker.lefx     # 남성 화자 확장
│   └── child_voice.lefx      # 아동 음성 확장
└── README.md
```

## 사용법

### 기본 명령어

- `help` - 도움말 표시
- `scan` - 플러그인 및 확장 모델 스캔
- `plugins` - 사용 가능한 플러그인 목록
- `extensions` - 사용 가능한 확장 모델 목록
- `quit` - 프로그램 종료

### 플러그인 관리

- `load <name>` - 플러그인 로드
- `unload <name>` - 플러그인 언로드
- `enable <name>` - 플러그인 활성화
- `disable <name>` - 플러그인 비활성화
- `info <name>` - 플러그인 상세 정보

### 확장 모델 관리

- `extension <name>` - 확장 모델 적용

### 효과 체인 관리

- `chain` - 현재 효과 체인 표시
- `add_effect <name>` - 효과를 체인에 추가
- `remove_effect <n>` - 체인에서 효과 제거 (인덱스)
- `effects on/off` - 효과 체인 활성화/비활성화

### 테스트

- `test <text>` - 텍스트로 음성 합성 테스트

## 사용 예시

### 플러그인 스캔 및 로드

```
> scan
플러그인 스캔 중: plugins
발견된 플러그인: 4개
확장 모델 스캔 중: extensions
발견된 확장 모델: 3개

> plugins
=== 사용 가능한 플러그인 ===
 1. reverb_plugin [언로드됨]
    타입: 오디오 효과
    버전: 1.0.0, 제작자: Unknown
    설명: 오디오 효과 플러그인

 2. equalizer_plugin [언로드됨]
    타입: 오디오 효과
    버전: 1.0.0, 제작자: Unknown
    설명: 오디오 효과 플러그인

> load reverb_plugin
플러그인 로드 중: reverb_plugin
플러그인 로드 완료: reverb_plugin

> enable reverb_plugin
플러그인 reverb_plugin: 활성화
```

### 확장 모델 적용

```
> extensions
=== 사용 가능한 확장 모델 ===
 1. female_speaker [언로드됨]
    기본 모델: base_female
    경로: extensions/female_speaker.lefx
    설명: 여성 음성 확장 모델

 2. male_speaker [언로드됨]
    기본 모델: base_male
    경로: extensions/male_speaker.lefx
    설명: 남성 음성 확장 모델

> extension female_speaker
확장 모델 적용 중: female_speaker
확장 모델 로드 중: extensions/female_speaker.lefx
확장 모델 적용 완료: female_speaker
기본 모델: base_female
```

### 효과 체인 구성

```
> add_effect reverb_plugin
효과를 체인에 추가했습니다: reverb_plugin (위치: 1)

> load equalizer_plugin
플러그인 로드 중: equalizer_plugin
플러그인 로드 완료: equalizer_plugin

> add_effect equalizer_plugin
효과를 체인에 추가했습니다: equalizer_plugin (위치: 2)

> chain
=== 현재 효과 체인 ===
효과 체인 상태: 활성화
1. reverb_plugin [활성화]
2. equalizer_plugin [활성화]
```

### 음성 합성 테스트

```
> test 안녕하세요, 플러그인 데모입니다.
음성 합성 테스트: "안녕하세요, 플러그인 데모입니다."
사용 중인 확장 모델: female_speaker
적용될 효과 체인:
  1. reverb_plugin
  2. equalizer_plugin
효과 적용 중: reverb_plugin
효과 적용 중: equalizer_plugin
음성 합성 완료:
  - 합성 시간: 123.45 ms
  - 오디오 길이: 2.50 초
  - 적용된 효과: 2개
오디오 재생 중...
재생 완료
```

### 플러그인 상세 정보 확인

```
> info reverb_plugin
=== 플러그인 상세 정보 ===
이름: reverb_plugin
버전: 1.0.0
제작자: Unknown
타입: 오디오 효과
설명: 오디오 효과 플러그인
상태: 로드됨, 활성화
메모리 사용량: 약 1.2 MB
CPU 사용률: 약 2.5%
지원 샘플 레이트: 22050, 44100, 48000 Hz
효과 파라미터:
  - 강도: 0.0 - 1.0 (기본값: 0.5)
  - 주파수: 20 - 20000 Hz (기본값: 1000)
  - 게인: -20 - +20 dB (기본값: 0)
```

## 플러그인 개발 가이드

### 플러그인 인터페이스

플러그인은 다음 함수들을 구현해야 합니다:

```c
// 플러그인 초기화
int plugin_init(PluginContext* ctx);

// 오디오 처리 (오디오 효과 플러그인의 경우)
int plugin_process_audio(float* input, float* output, int num_samples);

// 파라미터 설정
int plugin_set_parameter(const char* name, float value);

// 플러그인 정리
void plugin_cleanup();

// 플러그인 정보 반환
PluginInfo* plugin_get_info();
```

### 확장 모델 형식 (LEFX)

LEFX 파일은 다음 구조를 가집니다:

```
LEFX Header
├── Magic Number (LEEX)
├── Version
├── Base Model Hash
├── Extension Type
├── Extension Metadata
└── Extension Data
    ├── Layer Differences
    ├── Speaker Parameters
    └── Additional Resources
```

### 의존성 관리

플러그인은 `plugin_dependencies.json` 파일을 통해 의존성을 명시할 수 있습니다:

```json
{
  "name": "advanced_reverb",
  "version": "2.0.0",
  "dependencies": [
    {
      "name": "audio_utils",
      "version": ">=1.5.0",
      "required": true
    },
    {
      "name": "fft_library",
      "version": "^3.0.0",
      "required": false
    }
  ]
}
```

## 성능 고려사항

### 플러그인 로딩
- 플러그인은 필요할 때만 로드하여 메모리 사용량 최적화
- 자주 사용되는 플러그인은 미리 로드하여 지연 시간 감소

### 효과 체인 처리
- 효과 체인은 순차적으로 처리되므로 순서가 중요
- CPU 집약적인 효과는 체인의 뒤쪽에 배치 권장

### 메모리 관리
- 플러그인별 메모리 풀 사용으로 단편화 방지
- 사용하지 않는 플러그인은 자동 언로드

## 문제 해결

### 플러그인 로드 실패
1. 플러그인 파일 경로 확인
2. 플러그인 파일 권한 확인
3. 의존성 라이브러리 설치 확인
4. 플러그인 호환성 확인

### 확장 모델 적용 실패
1. LEFX 파일 무결성 확인
2. 기본 모델과의 호환성 확인
3. 메모리 부족 여부 확인

### 효과 체인 문제
1. 플러그인 활성화 상태 확인
2. 효과 파라미터 유효성 확인
3. 오디오 포맷 호환성 확인

## 확장 가능성

이 데모는 다음과 같은 확장이 가능합니다:

- **GUI 플러그인**: 시각적 파라미터 조정 인터페이스
- **네트워크 플러그인**: 원격 처리 및 클라우드 연동
- **AI 플러그인**: 머신러닝 기반 음성 향상
- **실시간 플러그인**: 스트리밍 환경에서의 실시간 효과 적용
- **플러그인 마켓플레이스**: 온라인 플러그인 다운로드 및 설치

## 라이선스

이 데모는 LibEtude 프로젝트의 일부로 제공됩니다.