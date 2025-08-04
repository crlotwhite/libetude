# world4utau 예제 및 스크립트

이 디렉토리는 world4utau의 다양한 사용 예제와 유틸리티 스크립트를 포함합니다.

## 📁 파일 목록

### 🚀 기본 사용 예제
- **`basic_usage.sh`** - world4utau의 기본적인 사용법을 보여주는 스크립트
- **`openutau_integration.md`** - OpenUTAU와의 연동 방법 상세 가이드

### 📊 성능 및 품질 분석
- **`performance_test.sh`** - 다양한 조건에서의 성능 측정 스크립트
- **`quality_comparison.py`** - 음성 품질 분석 및 비교 도구

## 🎯 사용 방법

### 1. 기본 사용 예제 실행

```bash
cd examples
./basic_usage.sh
```

이 스크립트는 다음과 같은 예제들을 실행합니다:
- 기본 음성 합성
- 피치 변경 (높음/낮음)
- 볼륨 및 모듈레이션 조절
- UTAU 파라미터 적용
- 고품질 출력
- 디버그 모드
- 피치 벤드 적용

### 2. 성능 테스트 실행

```bash
cd examples
./performance_test.sh
```

성능 테스트는 다음을 측정합니다:
- 처리 시간
- 메모리 사용량
- 파일 크기
- 다양한 옵션별 성능 차이

### 3. 품질 분석 실행

```bash
cd examples
pip install librosa soundfile scipy matplotlib  # 의존성 설치
./quality_comparison.py
```

품질 분석은 다음 지표를 계산합니다:
- 신호 대 잡음비 (SNR)
- 전고조파 왜곡 + 잡음 (THD+N)
- 주파수 응답
- 상관관계
- 다이나믹 레인지

## 📋 전제 조건

### 필수 요구사항
- world4utau가 빌드되어 있어야 함
- Bash 셸 (Linux/macOS/WSL)

### 선택적 요구사항
- **오디오 생성용**: `ffmpeg` 또는 `sox`
- **품질 분석용**: Python 3.6+ 및 다음 패키지들:
  ```bash
  pip install librosa soundfile scipy matplotlib numpy
  ```

## 🔧 설정 및 커스터마이징

### world4utau 경로 설정

스크립트들은 기본적으로 `../build/world4utau` 경로를 사용합니다. 다른 경로에 있다면:

```bash
# 환경 변수로 설정
export WORLD4UTAU_PATH="/path/to/world4utau"

# 또는 스크립트 내에서 직접 수정
WORLD4UTAU="/your/custom/path/world4utau"
```

### 테스트 파라미터 커스터마이징

각 스크립트의 상단에서 테스트 파라미터를 수정할 수 있습니다:

```bash
# basic_usage.sh에서
DEFAULT_PITCH=440.0
DEFAULT_VELOCITY=100
TEST_DURATIONS=(1 5 10)  # 초 단위

# performance_test.sh에서
TEST_PITCHES=(220 440 880)
TEST_VELOCITIES=(50 100 150)
```

## 📊 결과 해석

### 성능 테스트 결과

성능 테스트 후 생성되는 파일들:
- `performance_results.txt` - 상세한 텍스트 결과
- `performance_results.csv` - 스프레드시트용 CSV 데이터

**좋은 성능 지표:**
- 처리 시간: 실시간 비율 < 0.1 (10% 미만)
- 메모리 사용량: < 100MB (일반적인 사용)
- 성공률: 100%

### 품질 분석 결과

품질 분석 후 생성되는 파일들:
- `quality_results.json` - 상세한 JSON 결과
- `quality_report.txt` - 텍스트 요약 리포트
- `correlation_comparison.png` - 상관관계 그래프
- `thd_comparison.png` - THD+N 비교 그래프

**좋은 품질 지표:**
- 상관관계: > 0.95 (원본과 95% 이상 유사)
- THD+N: < 1% (왜곡 1% 미만)
- 다이나믹 레인지: 원본과 유사

## 🐛 문제 해결

### 일반적인 문제

#### "world4utau를 찾을 수 없습니다"
```bash
# world4utau가 빌드되었는지 확인
ls -la ../build/world4utau
ls -la ../world4utau

# 빌드가 안 되어 있다면
cd ..
mkdir build && cd build
cmake ..
make
```

#### "ffmpeg 또는 sox가 없습니다"
```bash
# Ubuntu/Debian
sudo apt-get install ffmpeg sox

# macOS
brew install ffmpeg sox

# 또는 테스트용 WAV 파일을 직접 준비
```

#### Python 의존성 오류
```bash
# 가상환경 사용 권장
python -m venv venv
source venv/bin/activate  # Linux/macOS
# venv\Scripts\activate   # Windows

pip install librosa soundfile scipy matplotlib numpy
```

### 성능 문제

#### 처리가 너무 느림
- SIMD 최적화가 활성화되었는지 확인
- 캐시가 활성화되었는지 확인
- 시스템 리소스 사용량 확인

#### 메모리 사용량이 많음
- 입력 파일 크기 확인
- 메모리 풀 크기 조정
- 동시 실행 프로세스 수 제한

## 🔍 고급 사용법

### 배치 처리 예제

```bash
#!/bin/bash
# 여러 파일을 일괄 처리

for input_file in *.wav; do
    output_file="processed_${input_file}"
    ./world4utau "$input_file" "$output_file" 440.0 100 --verbose
done
```

### 커스텀 테스트 케이스 추가

`performance_test.sh`에 새로운 테스트 추가:

```bash
# 커스텀 테스트 추가
run_test "custom_test" "test_medium.wav" 440 100 "--custom-option value"
```

### 품질 분석 확장

`quality_comparison.py`에 새로운 지표 추가:

```python
def calculate_custom_metric(self, audio):
    """커스텀 품질 지표 계산"""
    # 여기에 커스텀 분석 로직 추가
    return custom_value
```

## 📚 추가 리소스

- [world4utau API 문서](../docs/html/index.html)
- [libetude 문서](../../../docs/)
- [OpenUTAU 공식 문서](https://github.com/stakira/OpenUtau/wiki)
- [WORLD 보코더 논문](https://www.jstage.jst.go.jp/article/transinf/E99.D/7/E99.D_2015EDP7457/_article)

## 🤝 기여하기

새로운 예제나 개선사항이 있다면:

1. 이슈 생성 또는 풀 리퀘스트 제출
2. 코드 스타일 가이드 준수
3. 적절한 주석 및 문서화 포함
4. 테스트 케이스 추가

## 📄 라이선스

이 예제들은 libetude 프로젝트의 라이선스를 따릅니다.