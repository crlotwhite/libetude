/**
 * @file engine.hpp
 * @brief LibEtude C++ 바인딩 - 엔진 클래스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude C API를 위한 모던 C++ 래퍼 클래스입니다.
 * RAII 패턴을 사용하여 자동 리소스 관리를 제공하고,
 * 예외 처리와 타입 안전성을 보장합니다.
 */

#ifndef LIBETUDE_CPP_ENGINE_HPP
#define LIBETUDE_CPP_ENGINE_HPP

#include "../api.h"
#include "../types.h"
#include "../config.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <stdexcept>
#include <future>
#include <mutex>

namespace libetude {

// ============================================================================
// 예외 클래스 정의
// ============================================================================

/**
 * @brief LibEtude 기본 예외 클래스
 */
class Exception : public std::runtime_error {
public:
    explicit Exception(LibEtudeErrorCode code, const std::string& message = "")
        : std::runtime_error(formatMessage(code, message)), error_code_(code) {}

    /**
     * @brief 오류 코드를 반환합니다
     * @return LibEtude 오류 코드
     */
    LibEtudeErrorCode getErrorCode() const noexcept { return error_code_; }

private:
    LibEtudeErrorCode error_code_;

    static std::string formatMessage(LibEtudeErrorCode code, const std::string& message);
};

/**
 * @brief 잘못된 인수 예외
 */
class InvalidArgumentException : public Exception {
public:
    explicit InvalidArgumentException(const std::string& message = "")
        : Exception(LIBETUDE_ERROR_INVALID_ARGUMENT, message) {}
};

/**
 * @brief 메모리 부족 예외
 */
class OutOfMemoryException : public Exception {
public:
    explicit OutOfMemoryException(const std::string& message = "")
        : Exception(LIBETUDE_ERROR_OUT_OF_MEMORY, message) {}
};

/**
 * @brief 런타임 오류 예외
 */
class RuntimeException : public Exception {
public:
    explicit RuntimeException(const std::string& message = "")
        : Exception(LIBETUDE_ERROR_RUNTIME, message) {}
};

/**
 * @brief 모델 관련 예외
 */
class ModelException : public Exception {
public:
    explicit ModelException(const std::string& message = "")
        : Exception(LIBETUDE_ERROR_MODEL, message) {}
};

/**
 * @brief 하드웨어 관련 예외
 */
class HardwareException : public Exception {
public:
    explicit HardwareException(const std::string& message = "")
        : Exception(LIBETUDE_ERROR_HARDWARE, message) {}
};

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief 오류 코드를 예외로 변환합니다
 * @param code 오류 코드
 * @param message 추가 메시지
 * @throws Exception 해당하는 예외 타입
 */
void throwOnError(LibEtudeErrorCode code, const std::string& message = "");

// ============================================================================
// 품질 모드 열거형 (C++ 스타일)
// ============================================================================

/**
 * @brief 품질 모드 열거형 (C++ 스타일)
 */
enum class QualityMode {
    Fast = LIBETUDE_QUALITY_FAST,         /**< 빠른 처리 (낮은 품질) */
    Balanced = LIBETUDE_QUALITY_BALANCED, /**< 균형 모드 */
    High = LIBETUDE_QUALITY_HIGH          /**< 고품질 (느린 처리) */
};

// ============================================================================
// 성능 통계 구조체 (C++ 스타일)
// ============================================================================

/**
 * @brief 성능 통계 구조체 (C++ 스타일)
 */
struct PerformanceStats {
    double inference_time_ms;      /**< 추론 시간 (밀리초) */
    double memory_usage_mb;        /**< 메모리 사용량 (MB) */
    double cpu_usage_percent;      /**< CPU 사용률 (%) */
    double gpu_usage_percent;      /**< GPU 사용률 (%) */
    int active_threads;            /**< 활성 스레드 수 */

    /**
     * @brief C 구조체로부터 생성합니다
     * @param c_stats C 구조체
     */
    explicit PerformanceStats(const ::PerformanceStats& c_stats)
        : inference_time_ms(c_stats.inference_time_ms)
        , memory_usage_mb(c_stats.memory_usage_mb)
        , cpu_usage_percent(c_stats.cpu_usage_percent)
        , gpu_usage_percent(c_stats.gpu_usage_percent)
        , active_threads(c_stats.active_threads) {}

    PerformanceStats() = default;
};

// ============================================================================
// 오디오 스트림 콜백 타입 (C++ 스타일)
// ============================================================================

/**
 * @brief 오디오 스트림 콜백 함수 타입 (C++ 스타일)
 */
using AudioStreamCallback = std::function<void(const std::vector<float>& audio)>;

// ============================================================================
// 메인 엔진 클래스
// ============================================================================

/**
 * @brief LibEtude 엔진 C++ 래퍼 클래스
 *
 * RAII 패턴을 사용하여 자동 리소스 관리를 제공하고,
 * 예외 처리와 타입 안전성을 보장하는 모던 C++ 인터페이스입니다.
 */
class Engine {
public:
    // ========================================================================
    // 생성자 및 소멸자
    // ========================================================================

    /**
     * @brief 모델 파일로부터 엔진을 생성합니다
     * @param model_path 모델 파일 경로 (.lef 또는 .lefx)
     * @throws ModelException 모델 로딩 실패 시
     * @throws OutOfMemoryException 메모리 부족 시
     */
    explicit Engine(const std::string& model_path);

    /**
     * @brief 복사 생성자 (삭제됨)
     */
    Engine(const Engine&) = delete;

    /**
     * @brief 이동 생성자
     * @param other 이동할 엔진 객체
     */
    Engine(Engine&& other) noexcept;

    /**
     * @brief 소멸자 (자동 리소스 해제)
     */
    ~Engine();

    /**
     * @brief 복사 대입 연산자 (삭제됨)
     */
    Engine& operator=(const Engine&) = delete;

    /**
     * @brief 이동 대입 연산자
     * @param other 이동할 엔진 객체
     * @return 자기 자신의 참조
     */
    Engine& operator=(Engine&& other) noexcept;

    // ========================================================================
    // 음성 합성 (동기 처리)
    // ========================================================================

    /**
     * @brief 텍스트를 음성으로 합성합니다 (동기)
     * @param text 합성할 텍스트 (UTF-8)
     * @return 생성된 오디오 데이터 (float 벡터)
     * @throws InvalidArgumentException 잘못된 텍스트 입력 시
     * @throws RuntimeException 합성 실패 시
     */
    std::vector<float> synthesizeText(const std::string& text);

    /**
     * @brief 가사와 음표를 노래로 합성합니다 (동기)
     * @param lyrics 가사 텍스트 (UTF-8)
     * @param notes 음표 배열 (MIDI 노트 번호)
     * @return 생성된 오디오 데이터 (float 벡터)
     * @throws InvalidArgumentException 잘못된 입력 시
     * @throws RuntimeException 합성 실패 시
     */
    std::vector<float> synthesizeSinging(const std::string& lyrics,
                                        const std::vector<float>& notes);

    // ========================================================================
    // 비동기 음성 합성
    // ========================================================================

    /**
     * @brief 텍스트를 비동기로 음성 합성합니다
     * @param text 합성할 텍스트
     * @return 오디오 데이터를 담은 future 객체
     */
    std::future<std::vector<float>> synthesizeTextAsync(const std::string& text);

    /**
     * @brief 노래를 비동기로 합성합니다
     * @param lyrics 가사 텍스트
     * @param notes 음표 배열
     * @return 오디오 데이터를 담은 future 객체
     */
    std::future<std::vector<float>> synthesizeSingingAsync(const std::string& lyrics,
                                                          const std::vector<float>& notes);

    // ========================================================================
    // 실시간 스트리밍 (비동기 처리)
    // ========================================================================

    /**
     * @brief 실시간 스트리밍을 시작합니다
     * @param callback 오디오 데이터 콜백 함수
     * @throws RuntimeException 스트리밍 시작 실패 시
     */
    void startStreaming(AudioStreamCallback callback);

    /**
     * @brief 스트리밍 중에 텍스트를 추가합니다
     * @param text 합성할 텍스트
     * @throws InvalidArgumentException 잘못된 텍스트 입력 시
     * @throws RuntimeException 스트리밍이 활성화되지 않은 경우
     */
    void streamText(const std::string& text);

    /**
     * @brief 실시간 스트리밍을 중지합니다
     * @throws RuntimeException 스트리밍 중지 실패 시
     */
    void stopStreaming();

    /**
     * @brief 스트리밍이 활성화되어 있는지 확인합니다
     * @return 스트리밍 활성화 여부
     */
    bool isStreaming() const noexcept { return streaming_active_; }

    // ========================================================================
    // 성능 제어 및 모니터링
    // ========================================================================

    /**
     * @brief 품질 모드를 설정합니다
     * @param mode 품질 모드
     * @throws RuntimeException 설정 실패 시
     */
    void setQualityMode(QualityMode mode);

    /**
     * @brief 현재 품질 모드를 반환합니다
     * @return 현재 품질 모드
     */
    QualityMode getQualityMode() const noexcept { return current_quality_mode_; }

    /**
     * @brief GPU 가속을 활성화합니다
     * @param enable GPU 가속 활성화 여부 (기본값: true)
     * @throws HardwareException GPU 초기화 실패 시
     */
    void enableGPUAcceleration(bool enable = true);

    /**
     * @brief GPU 가속이 활성화되어 있는지 확인합니다
     * @return GPU 가속 활성화 여부
     */
    bool isGPUAccelerationEnabled() const noexcept { return gpu_acceleration_enabled_; }

    /**
     * @brief 성능 통계를 가져옵니다
     * @return 성능 통계 구조체
     * @throws RuntimeException 통계 수집 실패 시
     */
    PerformanceStats getPerformanceStats();

    // ========================================================================
    // 확장 모델 관리
    // ========================================================================

    /**
     * @brief 확장 모델을 로드합니다
     * @param extension_path 확장 모델 파일 경로 (.lefx)
     * @return 로드된 확장 모델 ID
     * @throws ModelException 확장 모델 로딩 실패 시
     */
    int loadExtension(const std::string& extension_path);

    /**
     * @brief 확장 모델을 언로드합니다
     * @param extension_id 확장 모델 ID
     * @throws InvalidArgumentException 잘못된 확장 ID 시
     * @throws RuntimeException 언로드 실패 시
     */
    void unloadExtension(int extension_id);

    /**
     * @brief 로드된 확장 모델 ID 목록을 반환합니다
     * @return 확장 모델 ID 벡터
     */
    std::vector<int> getLoadedExtensions() const { return loaded_extensions_; }

    // ========================================================================
    // 유틸리티 함수
    // ========================================================================

    /**
     * @brief 엔진이 유효한지 확인합니다
     * @return 엔진 유효성 여부
     */
    bool isValid() const noexcept { return engine_ != nullptr; }

    /**
     * @brief 내부 C 엔진 핸들을 반환합니다 (고급 사용자용)
     * @return C 엔진 핸들
     * @warning 이 함수는 고급 사용자를 위한 것입니다. 직접 사용 시 주의하세요.
     */
    LibEtudeEngine* getCHandle() const noexcept { return engine_; }

    // ========================================================================
    // 정적 유틸리티 함수
    // ========================================================================

    /**
     * @brief LibEtude 버전 문자열을 반환합니다
     * @return 버전 문자열 (예: "1.0.0")
     */
    static std::string getVersion();

    /**
     * @brief 지원되는 하드웨어 기능을 반환합니다
     * @return 하드웨어 기능 플래그
     */
    static uint32_t getHardwareFeatures();

    /**
     * @brief 마지막 오류 메시지를 반환합니다
     * @return 오류 메시지 문자열
     */
    static std::string getLastError();

private:
    // ========================================================================
    // 내부 멤버 변수
    // ========================================================================

    LibEtudeEngine* engine_;                    /**< C 엔진 핸들 */
    bool streaming_active_;                     /**< 스트리밍 활성화 상태 */
    QualityMode current_quality_mode_;          /**< 현재 품질 모드 */
    bool gpu_acceleration_enabled_;             /**< GPU 가속 활성화 상태 */
    std::vector<int> loaded_extensions_;        /**< 로드된 확장 모델 ID 목록 */

    // 스트리밍 관련
    std::unique_ptr<AudioStreamCallback> stream_callback_; /**< 스트림 콜백 */
    mutable std::mutex stream_mutex_;           /**< 스트림 동기화 뮤텍스 */

    // ========================================================================
    // 내부 헬퍼 함수
    // ========================================================================

    /**
     * @brief C 콜백을 C++ 콜백으로 변환하는 래퍼 함수
     * @param audio 오디오 데이터
     * @param length 오디오 길이
     * @param user_data 사용자 데이터 (Engine 포인터)
     */
    static void streamCallbackWrapper(const float* audio, int length, void* user_data);

    /**
     * @brief 리소스를 해제합니다
     */
    void cleanup() noexcept;
};

// ============================================================================
// 편의 함수들
// ============================================================================

/**
 * @brief 엔진을 생성하는 편의 함수
 * @param model_path 모델 파일 경로
 * @return 엔진 unique_ptr
 */
std::unique_ptr<Engine> createEngine(const std::string& model_path);

/**
 * @brief 텍스트를 음성으로 변환하는 편의 함수
 * @param model_path 모델 파일 경로
 * @param text 합성할 텍스트
 * @return 생성된 오디오 데이터
 */
std::vector<float> textToSpeech(const std::string& model_path, const std::string& text);

} // namespace libetude

#endif // LIBETUDE_CPP_ENGINE_HPP