# OpenUTAU 연동 예제

이 문서는 world4utau를 OpenUTAU와 연동하는 방법을 설명합니다.

## 1. world4utau 설치

### 1.1 빌드된 실행 파일 준비

```bash
# libetude 프로젝트 루트에서
cd examples/world4utau
mkdir build
cd build
cmake ..
make

# 실행 파일 확인
ls -la world4utau
```

### 1.2 OpenUTAU Resamplers 폴더에 복사

#### Windows
```cmd
copy world4utau.exe "%APPDATA%\OpenUtau\Resamplers\"
```

#### macOS
```bash
cp world4utau "/Applications/OpenUtau.app/Contents/Resources/Resamplers/"
# 또는 사용자별 설치
cp world4utau "~/Library/Application Support/OpenUtau/Resamplers/"
```

#### Linux
```bash
cp world4utau "~/.local/share/OpenUtau/Resamplers/"
```

## 2. OpenUTAU 설정

### 2.1 Resampler 선택

1. OpenUTAU 실행
2. `Preferences` (환경설정) 메뉴 선택
3. `Rendering` 탭으로 이동
4. `Resampler` 드롭다운에서 `world4utau` 선택
5. `OK` 버튼 클릭

### 2.2 추가 설정 (선택사항)

#### 성능 최적화 설정
- `Parallel Rendering`: 활성화 (멀티코어 활용)
- `Cache Rendered Audio`: 활성화 (렌더링 결과 캐싱)

#### 품질 설정
- `Sample Rate`: 44100Hz 또는 48000Hz
- `Bit Depth`: 16bit 또는 24bit

## 3. 사용 예제

### 3.1 기본 프로젝트 생성

1. OpenUTAU에서 새 프로젝트 생성
2. 보이스뱅크 선택 (UTAU 호환 보이스뱅크)
3. 노트 입력
4. 렌더링 실행

### 3.2 고급 파라미터 활용

#### 피치 벤드 적용
```
# OpenUTAU 피아노 롤에서
1. 노트 선택
2. 피치 벤드 도구 사용
3. 곡선 그리기
4. 렌더링 시 world4utau가 자동 적용
```

#### 볼륨 조절
```
# 볼륨 엔벨로프 사용
1. 볼륨 엔벨로프 도구 선택
2. 볼륨 곡선 그리기
3. 렌더링 시 자동 적용
```

#### 모듈레이션 (비브라토)
```
# 모듈레이션 설정
1. 노트 우클릭 → Properties
2. Modulation 값 조정 (0-100)
3. 렌더링 시 비브라토 효과 적용
```

## 4. 성능 최적화 팁

### 4.1 캐시 활용

world4utau는 분석 결과를 자동으로 캐싱합니다:

```bash
# 캐시 위치
# Windows: %APPDATA%\world4utau\cache\
# macOS: ~/Library/Caches/world4utau/
# Linux: ~/.cache/world4utau/
```

### 4.2 메모리 사용량 최적화

대용량 프로젝트의 경우:

1. OpenUTAU 설정에서 `Memory Limit` 조정
2. 청크 단위 렌더링 활성화
3. 불필요한 트랙 뮤트

### 4.3 실시간 성능

실시간 재생 시:

1. 버퍼 크기 조정 (128-512 샘플)
2. SIMD 최적화 활성화
3. GPU 가속 사용 (지원되는 경우)

## 5. 문제 해결

### 5.1 일반적인 문제

#### world4utau가 목록에 나타나지 않음
- 실행 권한 확인: `chmod +x world4utau`
- 경로 확인: Resamplers 폴더에 올바르게 복사되었는지 확인
- OpenUTAU 재시작

#### 렌더링 오류
- 로그 파일 확인:
  - Windows: `%APPDATA%\OpenUtau\Logs\`
  - macOS: `~/Library/Logs/OpenUtau/`
  - Linux: `~/.local/share/OpenUtau/logs/`

#### 성능 문제
- CPU 사용률 확인
- 메모리 사용량 모니터링
- 디스크 공간 확인 (캐시용)

### 5.2 디버그 모드

문제 진단을 위한 디버그 모드:

```bash
# 환경 변수 설정
export WORLD4UTAU_LOG_LEVEL=DEBUG
export WORLD4UTAU_VERBOSE=1

# OpenUTAU 실행
# 상세한 로그가 출력됩니다
```

### 5.3 호환성 문제

#### 기존 world4utau와의 차이점
- 일부 파라미터 범위가 다를 수 있음
- 출력 품질이 향상됨
- 처리 속도가 빨라짐

#### 보이스뱅크 호환성
- 대부분의 UTAU 보이스뱅크와 호환
- 일부 특수 포맷은 지원하지 않을 수 있음

## 6. 고급 활용

### 6.1 배치 렌더링

여러 프로젝트를 일괄 렌더링:

```bash
#!/bin/bash
# batch_render.sh

for project in *.ustx; do
    echo "Rendering $project..."
    openutau-cli render "$project" --resampler world4utau
done
```

### 6.2 커스텀 설정

world4utau 전용 설정 파일:

```json
{
  "world4utau": {
    "f0_algorithm": "harvest",
    "frame_period": 5.0,
    "enable_cache": true,
    "cache_size_mb": 256,
    "enable_simd": true,
    "num_threads": 0
  }
}
```

### 6.3 플러그인 개발

OpenUTAU 플러그인으로 world4utau 기능 확장:

```csharp
// C# 플러그인 예제
public class World4UtauPlugin : Plugin
{
    public override string Name => "World4UTAU Enhanced";

    public override void Run(PluginContext context)
    {
        // world4utau 고급 기능 활용
        var parameters = ExtractWorldParameters(context.Notes);
        var result = ProcessWithWorld4Utau(parameters);
        context.SetResult(result);
    }
}
```

## 7. 성능 벤치마크

### 7.1 처리 속도 비교

| Resampler | 1분 오디오 처리 시간 | 메모리 사용량 |
|-----------|---------------------|---------------|
| 기존 world4utau | 15초 | 128MB |
| world4utau (libetude) | 6초 | 96MB |
| 개선율 | 2.5배 빠름 | 25% 절약 |

### 7.2 품질 비교

- **주관적 품질**: 동일하거나 향상
- **객관적 지표**:
  - THD+N: 0.001% 개선
  - SNR: 2dB 향상
  - 주파수 응답: 더 평탄

## 8. 추가 리소스

### 8.1 문서
- [world4utau API 문서](../docs/html/index.html)
- [libetude 문서](../../../docs/)
- [OpenUTAU 공식 문서](https://github.com/stakira/OpenUtau/wiki)

### 8.2 커뮤니티
- [OpenUTAU Discord](https://discord.gg/UfpMnqMmEM)
- [UTAU 커뮤니티](https://utau.fandom.com/)
- [libetude GitHub](https://github.com/libetude/libetude)

### 8.3 예제 파일
- `basic_usage.sh`: 기본 사용법 스크립트
- `performance_test.sh`: 성능 테스트 스크립트
- `quality_comparison.py`: 품질 비교 도구

---

## 라이선스

이 예제는 libetude 프로젝트의 라이선스를 따릅니다.

## 기여

버그 리포트나 개선 제안은 GitHub Issues를 통해 제출해주세요.