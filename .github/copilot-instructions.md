
# Copilot Instructions for LibEtude

## 프로젝트 개요 및 아키텍처
- **LibEtude**는 음성 합성(voice synthesis)에 특화된 고성능 AI 추론 엔진입니다.
- **모듈 구조**: `src/`에 엔진 코어, `bindings/`에 언어별 바인딩(C++, Java/JNI, ObjC/Swift), `tools/`에 개발 도구, `examples/`에 예제, `tests/`에 테스트가 위치합니다.
- **주요 컴포넌트**:
  - `core/`: 엔진, 메모리, 커널, 그래프, 수학, 하드웨어 감지 등 핵심 로직
  - `lef/`: 모델 포맷(LEF/LEFX) 및 확장, 압축
  - `runtime/`: 스케줄러, 프로파일러, 플러그인 시스템
  - `audio/`: 오디오 입출력, DSP, vocoder, 이펙트
  - `platform/`: 플랫폼 추상화 및 최적화(모바일/임베디드/데스크탑)
- **확장성**: 플러그인 및 확장 모델(`lef/extensions/`, `runtime/plugin/`)
- **플랫폼 추상화**: `platform/` 하위에 공통/플랫폼별 구현 분리. ETAudioInterface, ETSystemInterface, ETThreadInterface, ETMemoryInterface 등 C 구조체 기반 추상화 계층 사용. (각 인터페이스 예시는 `include/libetude/platform/` 참고)

## 빌드 및 테스트 워크플로우
- **CMake 기반 빌드**: 모든 플랫폼에서 CMake로 빌드. 예시:
  - `cmake -B build -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build --config Release`
  - `cmake --install build`
- **플랫폼별 빌드**: Android/iOS/임베디드 등은 toolchain 파일 지정 필요(`cmake/ios.toolchain.cmake` 등)
- **테스트 실행**:
  - 전체: `./run_tests.sh` 또는 `ctest` (빌드 디렉토리)
  - 개별/라벨: `ctest -L unit`, `ctest -L performance`
  - 성능: `./performance_tests`, 벤치마크: `./bin/libetude_benchmarks`
  - 메모리 검사: `valgrind --leak-check=full ./unit_tests`
- **테스트 프레임워크**: C(UNITY), C++(GoogleTest), 각 tests/README.md 참고

## 코드/패턴 및 컨벤션
- **C11/C++17 표준**: C는 C11, C++ 바인딩은 C++17을 엄격히 준수
- **RAII/예외 처리**: C++ 바인딩은 RAII, 예외 기반 오류 처리, 스레드 안전성 주의
- **엔진 객체 재사용**: 생성 비용이 높으므로 재사용 권장
- **비동기/스트리밍**: std::future, 콜백 기반 실시간 합성 지원
- **품질/성능 모드**: `QualityMode`로 품질/속도 조정, GPU 가속 지원
- **모바일 최적화**: `bindings/mobile_optimization.h` 및 관련 구조체/설정 활용
- **플랫폼 추상화**: ETAudioInterface, ETSystemInterface 등 구조체 기반 인터페이스로 OS별 구현을 감춤

## 주요 파일/디렉토리
- `include/libetude/`: 공개 API 헤더 및 플랫폼 추상화 인터페이스
- `src/`: 엔진 및 플랫폼별 구현
- `bindings/`: 언어 바인딩(C++, JNI, ObjC/Swift)
- `tests/`: 단위/통합/성능 테스트, 테스트 데이터
- `examples/`: 실제 사용 예제
- `docs/`: 문서 및 API 설명
- `.kiro/specs/`: 아키텍처, 요구사항, 구현계획 등 상세 설계 문서 (agent 참고용)

## 확장/통합 및 주요 설계 패턴
- **플랫폼 추상화**: ETAudioInterface, ETSystemInterface, ETThreadInterface, ETMemoryInterface, ETFilesystemInterface, ETNetworkInterface 등 구조체 기반 C 인터페이스로 OS별 차이를 감춤. 각 인터페이스는 함수 포인터 테이블로 구현.
- **GPU 가속**: CUDA/Metal/OpenCL 백엔드 자동 감지 및 선택, CPU 폴백. GPU 가속은 기존 API와 투명하게 통합.
- **모델 변환**: ONNX to LEF 변환기(onnx_to_lef_converter/)는 음성합성 특화 최적화, 양자화, 압축, 검증 등 지원.
- **WORLD/UTAU 호환**: world4utau-example/은 UTAU 파라미터 파싱, WORLD 알고리즘(F0, 스펙트럼, 비주기성 분석)과 libetude DSP/메모리/커널 통합 예시 제공.

## 기타
- **API 문서**: Doxygen 스타일 주석, 주요 헤더 참조
- **기여/PR**: feature 브랜치, 단위 테스트 필수, C11/C++17 표준 준수
- **문제 해결**: 빌드/테스트 오류는 README.md, tests/README.md, bindings/README.md, .kiro/specs/ 참고

---
이 문서는 AI 코딩 에이전트가 LibEtude에서 즉시 생산적으로 작업할 수 있도록 핵심 구조, 워크플로우, 컨벤션, 플랫폼 추상화, GPU 가속, 모델 변환, 예제 통합 패턴을 요약합니다. 추가로 명확히 해야 할 부분이 있으면 피드백을 남겨주세요.
