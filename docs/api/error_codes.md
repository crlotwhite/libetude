# LibEtude 오류 코드 문서

## 개요

LibEtude는 체계적인 오류 처리 시스템을 제공하여 개발자가 문제를 쉽게 진단하고 해결할 수 있도록 합니다. 이 문서는 모든 오류 코드와 그 의미, 해결 방법을 상세히 설명합니다.

## 오류 코드 체계

LibEtude의 오류 코드는 다음과 같은 체계를 따릅니다:

- **성공**: `0` (LIBETUDE_SUCCESS)
- **일반 오류**: `-1` ~ `-14` (기본 오류 코드)
- **확장 오류**: `-15` ~ `-999` (도메인별 특화 오류)

## 기본 오류 코드

### LIBETUDE_SUCCESS (0)
**의미**: 작업이 성공적으로 완료됨

**발생 상황**: 모든 API 함수가 정상적으로 실행된 경우

**해결 방법**: 해결할 필요 없음

### LIBETUDE_ERROR_INVALID_ARGUMENT (-1)
**의미**: 잘못된 인수가 전달됨

**발생 상황**:
- NULL 포인터가 전달된 경우
- 범위를 벗어난 값이 전달된 경우
- 잘못된 형식의 데이터가 전달된 경우

**해결 방법**:
```c
// 잘못된 예
LibEtudeEngine* engine = libetude_create_engine(NULL); // NULL 전달

// 올바른 예
LibEtudeEngine* engine = libetude_create_engine("model.lef");
if (!engine) {
    fprintf(stderr, "엔진 생성 실패: %s\n", libetude_get_last_error());
}
```

**예방 방법**:
- 함수 호출 전 매개변수 유효성 검사
- NULL 포인터 체크
- 범위 검증

### LIBETUDE_ERROR_OUT_OF_MEMORY (-2)
**의미**: 메모리 할당 실패

**발생 상황**:
- 시스템 메모리 부족
- 메모리 풀 고갈
- 메모리 단편화

**해결 방법**:
```c
// 메모리 사용량 모니터링
PerformanceStats stats;
if (libetude_get_performance_stats(engine, &stats) == LIBETUDE_SUCCESS) {
    if (stats.memory_usage_mb > 1000.0) {
        // 메모리 사용량이 높은 경우 품질 모드 조정
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    }
}
```

**예방 방법**:
- 정기적인 메모리 사용량 모니터링
- 적절한 품질 모드 선택
- 불필요한 리소스 해제

### LIBETUDE_ERROR_IO (-3)
**의미**: 입출력 오류

**발생 상황**:
- 파일을 찾을 수 없음
- 파일 읽기/쓰기 권한 없음
- 디스크 공간 부족
- 네트워크 연결 문제

**해결 방법**:
```c
LibEtudeEngine* engine = libetude_create_engine("model.lef");
if (!engine) {
    const char* error = libetude_get_last_error();
    if (strstr(error, "파일을 찾을 수 없습니다")) {
        fprintf(stderr, "모델 파일 경로를 확인하세요: model.lef\n");
    } else if (strstr(error, "권한이 없습니다")) {
        fprintf(stderr, "파일 읽기 권한을 확인하세요\n");
    }
}
```

**예방 방법**:
- 파일 존재 여부 사전 확인
- 적절한 파일 권한 설정
- 충분한 디스크 공간 확보

### LIBETUDE_ERROR_NOT_IMPLEMENTED (-4)
**의미**: 구현되지 않은 기능

**발생 상황**:
- 플랫폼에서 지원하지 않는 기능 호출
- 개발 중인 기능 사용

**해결 방법**:
```c
int result = libetude_enable_gpu_acceleration(engine);
if (result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
    printf("이 플랫폼에서는 GPU 가속을 지원하지 않습니다\n");
    // CPU 모드로 계속 진행
}
```

**예방 방법**:
- 플랫폼별 기능 지원 여부 확인
- 대체 방법 준비

### LIBETUDE_ERROR_RUNTIME (-5)
**의미**: 런타임 오류

**발생 상황**:
- 예상치 못한 내부 오류
- 데이터 처리 중 오류
- 알고리즘 실행 실패

**해결 방법**:
```c
int result = libetude_synthesize_text(engine, text, audio, &length);
if (result == LIBETUDE_ERROR_RUNTIME) {
    // 재시도 로직
    for (int i = 0; i < 3; i++) {
        usleep(100000); // 100ms 대기
        result = libetude_synthesize_text(engine, text, audio, &length);
        if (result == LIBETUDE_SUCCESS) break;
    }
}
```

**예방 방법**:
- 입력 데이터 검증
- 재시도 메커니즘 구현
- 로그 분석을 통한 패턴 파악

### LIBETUDE_ERROR_HARDWARE (-6)
**의미**: 하드웨어 관련 오류

**발생 상황**:
- GPU 드라이버 문제
- SIMD 명령어 지원 불가
- 오디오 디바이스 오류

**해결 방법**:
```c
uint32_t features = libetude_get_hardware_features();
if (features == LIBETUDE_SIMD_NONE) {
    printf("SIMD 최적화를 사용할 수 없습니다\n");
    // 기본 CPU 모드로 동작
}

int result = libetude_enable_gpu_acceleration(engine);
if (result == LIBETUDE_ERROR_HARDWARE) {
    printf("GPU를 사용할 수 없습니다. CPU 모드로 동작합니다\n");
}
```

**예방 방법**:
- 하드웨어 기능 사전 확인
- 폴백 메커니즘 구현
- 드라이버 업데이트 권장

### LIBETUDE_ERROR_MODEL (-7)
**의미**: 모델 관련 오류

**발생 상황**:
- 손상된 모델 파일
- 호환되지 않는 모델 버전
- 모델 검증 실패

**해결 방법**:
```c
LibEtudeEngine* engine = libetude_create_engine("model.lef");
if (!engine) {
    const char* error = libetude_get_last_error();
    if (strstr(error, "체크섬")) {
        fprintf(stderr, "모델 파일이 손상되었습니다. 다시 다운로드하세요\n");
    } else if (strstr(error, "버전")) {
        fprintf(stderr, "모델 버전이 호환되지 않습니다\n");
    }
}
```

**예방 방법**:
- 모델 파일 무결성 검증
- 호환 버전 확인
- 백업 모델 준비

### LIBETUDE_ERROR_TIMEOUT (-8)
**의미**: 시간 초과

**발생 상황**:
- 처리 시간이 너무 오래 걸림
- 네트워크 응답 지연
- 리소스 대기 시간 초과

**해결 방법**:
```c
// 타임아웃 발생 시 재시도
int result = libetude_synthesize_text(engine, text, audio, &length);
if (result == LIBETUDE_ERROR_TIMEOUT) {
    // 품질 모드를 낮춰서 처리 속도 향상
    libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    result = libetude_synthesize_text(engine, text, audio, &length);
}
```

**예방 방법**:
- 적절한 품질 모드 선택
- 텍스트 길이 제한
- 비동기 처리 사용

### LIBETUDE_ERROR_NOT_INITIALIZED (-9)
**의미**: 초기화되지 않은 상태에서 함수 호출

**발생 상황**:
- 엔진 생성 전 API 호출
- 초기화 실패 후 계속 사용

**해결 방법**:
```c
// 엔진 상태 확인
if (!engine) {
    fprintf(stderr, "엔진이 초기화되지 않았습니다\n");
    return LIBETUDE_ERROR_NOT_INITIALIZED;
}

int result = libetude_synthesize_text(engine, text, audio, &length);
```

**예방 방법**:
- 엔진 생성 성공 여부 확인
- 상태 변수를 통한 초기화 상태 추적

### LIBETUDE_ERROR_ALREADY_INITIALIZED (-10)
**의미**: 이미 초기화된 상태에서 재초기화 시도

**발생 상황**:
- 중복 초기화 호출
- 멀티스레드 환경에서 동시 초기화

**해결 방법**:
```c
static bool is_initialized = false;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

int safe_initialize() {
    pthread_mutex_lock(&init_mutex);

    if (is_initialized) {
        pthread_mutex_unlock(&init_mutex);
        return LIBETUDE_ERROR_ALREADY_INITIALIZED;
    }

    // 초기화 수행
    is_initialized = true;
    pthread_mutex_unlock(&init_mutex);

    return LIBETUDE_SUCCESS;
}
```

**예방 방법**:
- 초기화 상태 추적
- 스레드 안전한 초기화 구현

### LIBETUDE_ERROR_UNSUPPORTED (-11)
**의미**: 지원되지 않는 기능 또는 형식

**발생 상황**:
- 지원하지 않는 오디오 형식
- 플랫폼별 미지원 기능
- 구버전 호환성 문제

**해결 방법**:
```c
// 기능 지원 여부 확인
uint32_t features = libetude_get_hardware_features();
if (!(features & LIBETUDE_SIMD_AVX2)) {
    printf("AVX2를 지원하지 않습니다. 기본 모드로 동작합니다\n");
}
```

**예방 방법**:
- 지원 기능 사전 확인
- 대체 방법 구현
- 플랫폼별 조건부 컴파일

### LIBETUDE_ERROR_NOT_FOUND (-12)
**의미**: 요청한 리소스를 찾을 수 없음

**발생 상황**:
- 존재하지 않는 확장 모델 ID
- 찾을 수 없는 설정 값
- 누락된 의존성

**해결 방법**:
```c
int result = libetude_unload_extension(engine, extension_id);
if (result == LIBETUDE_ERROR_NOT_FOUND) {
    printf("확장 모델 ID %d를 찾을 수 없습니다\n", extension_id);
    // 현재 로드된 확장 목록 확인
}
```

**예방 방법**:
- 리소스 존재 여부 사전 확인
- 유효한 ID 관리
- 의존성 검증

### LIBETUDE_ERROR_INVALID_STATE (-13)
**의미**: 현재 상태에서 수행할 수 없는 작업

**발생 상황**:
- 스트리밍 중이 아닌데 텍스트 추가 시도
- 이미 중지된 엔진에서 작업 수행

**해결 방법**:
```c
// 스트리밍 상태 확인
static bool is_streaming = false;

int stream_text_safe(LibEtudeEngine* engine, const char* text) {
    if (!is_streaming) {
        fprintf(stderr, "스트리밍이 시작되지 않았습니다\n");
        return LIBETUDE_ERROR_INVALID_STATE;
    }

    return libetude_stream_text(engine, text);
}
```

**예방 방법**:
- 상태 변수를 통한 상태 추적
- 상태 전환 검증
- 명확한 상태 머신 구현

### LIBETUDE_ERROR_BUFFER_FULL (-14)
**의미**: 버퍼가 가득 참

**발생 상황**:
- 출력 버퍼 크기 부족
- 내부 큐 오버플로우
- 메모리 제한 도달

**해결 방법**:
```c
float audio_buffer[44100]; // 1초 분량
int buffer_size = sizeof(audio_buffer) / sizeof(float);

int result = libetude_synthesize_text(engine, text, audio_buffer, &buffer_size);
if (result == LIBETUDE_ERROR_BUFFER_FULL) {
    // 더 큰 버퍼로 재시도
    float* large_buffer = malloc(44100 * 10 * sizeof(float)); // 10초 분량
    int large_size = 44100 * 10;

    result = libetude_synthesize_text(engine, text, large_buffer, &large_size);

    free(large_buffer);
}
```

**예방 방법**:
- 충분한 버퍼 크기 할당
- 동적 버퍼 크기 조정
- 청크 단위 처리

## 확장 오류 코드

### ET_ERROR_THREAD (-15)
**의미**: 스레드 관련 오류

**발생 상황**:
- 스레드 생성 실패
- 동기화 오류
- 데드락 발생

### ET_ERROR_AUDIO (-16)
**의미**: 오디오 관련 오류

**발생 상황**:
- 오디오 디바이스 초기화 실패
- 샘플링 레이트 불일치
- 오디오 드라이버 문제

### ET_ERROR_COMPRESSION (-17)
**의미**: 압축 관련 오류

**발생 상황**:
- 압축 해제 실패
- 손상된 압축 데이터
- 지원하지 않는 압축 형식

### ET_ERROR_QUANTIZATION (-18)
**의미**: 양자화 관련 오류

**발생 상황**:
- 양자화 파라미터 오류
- 정밀도 손실 초과
- 양자화 형식 불일치

### ET_ERROR_GRAPH (-19)
**의미**: 그래프 관련 오류

**발생 상황**:
- 그래프 구조 오류
- 순환 참조 발견
- 노드 연결 실패

### ET_ERROR_KERNEL (-20)
**의미**: 커널 관련 오류

**발생 상황**:
- 커널 로드 실패
- SIMD 명령어 오류
- GPU 커널 실행 실패

## 오류 처리 모범 사례

### 1. 계층적 오류 처리

```c
typedef enum {
    ERROR_LEVEL_RECOVERABLE,    // 복구 가능한 오류
    ERROR_LEVEL_WARNING,        // 경고 (계속 진행 가능)
    ERROR_LEVEL_FATAL          // 치명적 오류 (중단 필요)
} ErrorLevel;

ErrorLevel classify_error(int error_code) {
    switch (error_code) {
        case LIBETUDE_ERROR_TIMEOUT:
        case LIBETUDE_ERROR_RUNTIME:
            return ERROR_LEVEL_RECOVERABLE;

        case LIBETUDE_ERROR_HARDWARE:
        case LIBETUDE_ERROR_UNSUPPORTED:
            return ERROR_LEVEL_WARNING;

        case LIBETUDE_ERROR_OUT_OF_MEMORY:
        case LIBETUDE_ERROR_MODEL:
            return ERROR_LEVEL_FATAL;

        default:
            return ERROR_LEVEL_WARNING;
    }
}
```

### 2. 오류 로깅 시스템

```c
void log_error_with_context(int error_code, const char* function,
                           const char* context) {
    const char* error_msg = libetude_get_last_error();
    ErrorLevel level = classify_error(error_code);

    switch (level) {
        case ERROR_LEVEL_RECOVERABLE:
            printf("[RECOVERABLE] %s in %s: %s (context: %s)\n",
                   error_msg, function, context);
            break;
        case ERROR_LEVEL_WARNING:
            printf("[WARNING] %s in %s: %s (context: %s)\n",
                   error_msg, function, context);
            break;
        case ERROR_LEVEL_FATAL:
            fprintf(stderr, "[FATAL] %s in %s: %s (context: %s)\n",
                    error_msg, function, context);
            break;
    }
}
```

### 3. 재시도 메커니즘

```c
int retry_with_backoff(int (*func)(void*), void* arg, int max_retries) {
    int result;
    int retry_count = 0;
    int delay_ms = 100; // 초기 지연 시간

    while (retry_count < max_retries) {
        result = func(arg);

        if (result == LIBETUDE_SUCCESS) {
            return result;
        }

        ErrorLevel level = classify_error(result);
        if (level == ERROR_LEVEL_FATAL) {
            // 치명적 오류는 재시도하지 않음
            return result;
        }

        if (level == ERROR_LEVEL_RECOVERABLE) {
            retry_count++;
            printf("재시도 %d/%d (지연: %dms)\n", retry_count, max_retries, delay_ms);

            usleep(delay_ms * 1000);
            delay_ms *= 2; // 지수적 백오프
            continue;
        }

        // 경고 수준 오류는 한 번만 재시도
        if (retry_count == 0) {
            retry_count++;
            usleep(delay_ms * 1000);
            continue;
        }

        return result;
    }

    return LIBETUDE_ERROR_TIMEOUT;
}
```

### 4. 오류 복구 전략

```c
int robust_synthesis_with_fallback(LibEtudeEngine* engine, const char* text,
                                  float* output, int* length) {
    // 1차 시도: 고품질 모드
    libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
    int result = libetude_synthesize_text(engine, text, output, length);

    if (result == LIBETUDE_SUCCESS) {
        return result;
    }

    // 2차 시도: 균형 모드
    if (result == LIBETUDE_ERROR_TIMEOUT || result == LIBETUDE_ERROR_OUT_OF_MEMORY) {
        printf("고품질 모드 실패, 균형 모드로 재시도\n");
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
        result = libetude_synthesize_text(engine, text, output, length);

        if (result == LIBETUDE_SUCCESS) {
            return result;
        }
    }

    // 3차 시도: 빠른 모드
    if (result == LIBETUDE_ERROR_TIMEOUT || result == LIBETUDE_ERROR_OUT_OF_MEMORY) {
        printf("균형 모드 실패, 빠른 모드로 재시도\n");
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        result = libetude_synthesize_text(engine, text, output, length);
    }

    return result;
}
```

### 5. 사용자 친화적 오류 메시지

```c
const char* get_user_friendly_error_message(int error_code) {
    switch (error_code) {
        case LIBETUDE_ERROR_INVALID_ARGUMENT:
            return "입력 데이터가 올바르지 않습니다. 텍스트와 설정을 확인해주세요.";
        case LIBETUDE_ERROR_OUT_OF_MEMORY:
            return "메모리가 부족합니다. 다른 프로그램을 종료하거나 텍스트 길이를 줄여보세요.";
        case LIBETUDE_ERROR_IO:
            return "파일을 읽을 수 없습니다. 파일 경로와 권한을 확인해주세요.";
        case LIBETUDE_ERROR_MODEL:
            return "모델 파일에 문제가 있습니다. 모델을 다시 다운로드해주세요.";
        case LIBETUDE_ERROR_HARDWARE:
            return "하드웨어 가속을 사용할 수 없습니다. CPU 모드로 동작합니다.";
        case LIBETUDE_ERROR_TIMEOUT:
            return "처리 시간이 너무 오래 걸립니다. 텍스트를 짧게 나누어 시도해보세요.";
        default:
            return "알 수 없는 오류가 발생했습니다. 개발자에게 문의해주세요.";
    }
}
```

## 디버깅 도구

### 1. 오류 추적 매크로

```c
#ifdef LIBETUDE_DEBUG
#define TRACE_ERROR(code) \
    do { \
        if (code != LIBETUDE_SUCCESS) { \
            fprintf(stderr, "[ERROR] %s:%d in %s(): %s\n", \
                    __FILE__, __LINE__, __func__, libetude_get_last_error()); \
        } \
    } while(0)
#else
#define TRACE_ERROR(code) ((void)0)
#endif

// 사용 예
int result = libetude_synthesize_text(engine, text, audio, &length);
TRACE_ERROR(result);
```

### 2. 오류 통계 수집

```c
typedef struct {
    int error_counts[32]; // 오류 코드별 발생 횟수
    int total_calls;
    int successful_calls;
} ErrorStatistics;

static ErrorStatistics g_error_stats = {0};

void record_api_call_result(int result) {
    g_error_stats.total_calls++;

    if (result == LIBETUDE_SUCCESS) {
        g_error_stats.successful_calls++;
    } else {
        int index = abs(result) % 32;
        g_error_stats.error_counts[index]++;
    }
}

void print_error_statistics() {
    printf("API 호출 통계:\n");
    printf("총 호출: %d\n", g_error_stats.total_calls);
    printf("성공: %d (%.1f%%)\n", g_error_stats.successful_calls,
           100.0 * g_error_stats.successful_calls / g_error_stats.total_calls);

    for (int i = 0; i < 32; i++) {
        if (g_error_stats.error_counts[i] > 0) {
            printf("오류 %d: %d회\n", -i, g_error_stats.error_counts[i]);
        }
    }
}
```

이 문서는 LibEtude의 모든 오류 코드와 효과적인 오류 처리 방법을 제공합니다. 개발 시 이 가이드를 참조하여 안정적이고 사용자 친화적인 애플리케이션을 만드시기 바랍니다.