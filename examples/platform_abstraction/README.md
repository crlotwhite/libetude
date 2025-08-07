# 플랫폼 추상화 레이어 예제

이 디렉토리는 LibEtude 플랫폼 추상화 레이어의 사용법을 보여주는 예제들을 포함합니다.

## 예제 목록

### 기본 예제
- `audio_example.c` - 오디오 I/O 인터페이스 사용법
- `system_info_example.c` - 시스템 정보 조회 예제
- `threading_example.c` - 스레딩 및 동기화 예제
- `memory_example.c` - 메모리 관리 예제
- `filesystem_example.c` - 파일 시스템 조작 예제
- `network_example.c` - 네트워크 통신 예제
- `dynlib_example.c` - 동적 라이브러리 로딩 예제

### 고급 예제
- `cross_platform_demo.c` - 크로스 플랫폼 데모 애플리케이션
- `performance_benchmark.c` - 성능 측정 및 비교 예제

## 빌드 방법

```bash
# 프로젝트 루트에서
mkdir build && cd build
cmake ..
make

# 예제 실행
./bin/examples/audio_example
./bin/examples/system_info_example
# ... 기타 예제들
```

## 예제 설명

각 예제는 독립적으로 실행 가능하며, 해당 인터페이스의 주요 기능들을 시연합니다. 모든 예제는 플랫폼에 관계없이 동일한 코드로 동작합니다.

### 학습 순서 권장

1. `system_info_example` - 기본적인 시스템 정보 조회
2. `memory_example` - 메모리 관리 기초
3. `filesystem_example` - 파일 시스템 조작
4. `threading_example` - 스레드 및 동기화
5. `audio_example` - 오디오 I/O (가장 복잡)
6. `network_example` - 네트워크 통신
7. `dynlib_example` - 동적 라이브러리 로딩
8. `cross_platform_demo` - 통합 데모
9. `performance_benchmark` - 성능 분석

## 주의사항

- 일부 예제는 관리자 권한이 필요할 수 있습니다 (오디오, 네트워크)
- 오디오 예제는 오디오 디바이스가 있는 환경에서만 정상 동작합니다
- 네트워크 예제는 방화벽 설정을 확인해주세요