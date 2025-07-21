/**
 * @file engine.hpp
 * @brief LibEtude C++ 바인딩
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude의 C++ 래퍼 클래스를 제공합니다.
 * RAII 패턴을 사용하여 안전한 리소스 관리를 보장합니다.
 */

#ifndef LIBETUDE_CPP_ENGINE_HPP
#define LIBETUDE_CPP_ENGINE_HPP

#include "../api.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <stdexcept>

namespace libetude {

/**
 * @brief LibEtude 예외 클래스
 */
class Exception : public std::runtime_error {
public:
    explicit Exception(const std::string& message)
        : std::runtime_error(message), error_code_(LIBETUDE_ERROR_RUNTIME) {}

    Exception(LibEtudeErrorCode code, const std::string& message)
        : std::runtime_error(message), error_code_(code) {}

    LibEtudeErrorCode getErrorCode() const noexcept { return error_code_; }

private:
    LibEtudeErrorCode error_code_;
};

/**
 * @brief 오디오 스트림 콜백 함수 타입 (C++ 버전)
 */
using AudioStreamCallback = std::function<void(const std::vector<float>&)>;

/**
 * @brief LibEtude 엔진 C++ 래퍼 클래스
 *
 * RAII 패턴을 사용하여 자동 리소스 관리를 제공합니다.
 */
class Engine {
public:
    /**
     * @brief 엔진을 생성합니다
     *
     * @param model_path 모델 파일 경로
     * @throws Exception 엔진 생성에 실패한 경우
     */
    explicit Engine(const std::string& model_path);

    /**
     * @brief 소멸자 - 엔진 리소스를 자동으로 해제합니다
     */
    ~Engine();

    // 복사 생성자와 대입 연산자 삭제 (이동만 허용)
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /**
     * @brief 이동 생성자
     */
    Engine(Engine&& other) noexcept;

    /**
     * @brief 이동 대입 연산자
     */
    Engine& operator=(Engine&& other) noexcept;

    // ========================================================================
    // 음성 합성 (동기 처리)
    // ========================================================================

    /**
     * @brief 텍스트를 음성으로 합성합니다
     *
     * @param text 합성할 텍스트
     * @return 생성된 오디오 데이터
     * @throws Exception 합성에 실패한 경우
     */
    std::vector<float> synthesizeText(const std::string& text);

    /**
     * @brief 가사와 음표를 노래로 합성합니다
     *
     * @param lyrics 가사 텍스트
     * @param notes 음표 배열 (MIDI 노트 번호)
     * @return 생성된 오디오 데이터
     * @throws Exception 합성에 실패한 경우
     */
    std::vector<float> synthesizeSinging(const std::string& lyrics,
                                        const std::vector<float>& notes);

    // ========================================================================
    // 실시간 스트리밍 (비동기 처리)
    // ========================================================================

    /**
     * @brief 실시간 스트리밍을 시작합니다
     *
     * @param callback 오디오 데이터 콜백 함수
     * @throws Exception 스트리밍 시작에 실패한 경우
     */
    void startStreaming(AudioStreamCallback callback);

    /**
     * @brief 스트리밍 중에 텍스트를 추가합니다
     *
     * @param text 합성할 텍스트
     * @throws Exception 텍스트 추가에 실패한 경우
     */
    void streamText(const std::string& text);

    /**
     * @brief 실시간 스트리밍을 중지합니다
     *
     * @throws Exception 스트리밍 중지에 실패한 경우
     */
    void stopStreaming();

    /**
     * @brief 현재 스트리밍 상태를 확인합니다
     *
     * @return 스트리밍 중이면 true, 아니면 false
     */
    bool isStreaming() const noexcept { return streaming_active_; }

    // ========================================================================
    // 성능 제어 및 모니터링
    // ========================================================================

    /**
     * @brief 품질 모드를 설정합니다
     *
     * @param mode 품질 모드
     * @throws Exception 설정에 실패한 경우
     */
    void setQualityMode(QualityMode mode);

    /**
     * @brief GPU 가속을 활성화합니다
     *
     * @param enable 활성화 여부 (기본값: true)
     * @throws Exception GPU 가속 설정에 실패한 경우
     */
    void enableGPUAcceleration(bool enable = true);

    /**
     * @brief 성능 통계를 가져옵니다
     *
     * @return 성능 통계 구조체
     * @throws Exception 통계 조회에 실패한 경우
     */
    PerformanceStats getPerformanceStats();

    // ========================================================================
    // 확장 모델 관리
    // ========================================================================

    /**
     * @brief 확장 모델을 로드합니다
     *
     * @param extension_path 확장 모델 파일 경로
     * @return 확장 모델 ID
     * @throws Exception 확장 모델 로드에 실패한 경우
     */
    int loadExtension(const std::string& extension_path);

    /**
     * @brief 확장 모델을 언로드합니다
     *
     * @param extension_id 확장 모델 ID
     * @throws Exception 확장 모델 언로드에 실패한 경우
     */
    void unloadExtension(int extension_id);

    // ========================================================================
    // 유틸리티 함수
    // ========================================================================

    /**
     * @brief 엔진이 유효한지 확인합니다
     *
     * @return 유효하면 true, 아니면 false
     */
    bool isValid() const noexcept { return engine_ != nullptr; }

    /**
     * @brief 내부 C 엔진 핸들을 반환합니다 (고급 사용자용)
     *
     * @return C 엔진 핸들
     */
    LibEtudeEngine* getHandle() const noexcept { return engine_; }

    // ========================================================================
    // 정적 유틸리티 함수
    // ========================================================================

    /**
     * @brief LibEtude 버전을 반환합니다
     *
     * @return 버전 문자열
     */
    static std::string getVersion();

    /**
     * @brief 지원되는 하드웨어 기능을 반환합니다
     *
     * @return 하드웨어 기능 플래그
     */
    static uint32_t getHardwareFeatures();

private:
    LibEtudeEngine* engine_;           /**< C 엔진 핸들 */
    bool streaming_active_;            /**< 스트리밍 활성 상태 */
    AudioStreamCallback stream_callback_; /**< 스트림 콜백 함수 */

    /**
     * @brief C 콜백 래퍼 함수
     */
    static void streamCallbackWrapper(const float* audio, int length, void* user_data);

    /**
     * @brief 오류 코드를 예외로 변환합니다
     */
    void checkError(int error_code, const std::string& operation);
};

// ============================================================================
// 편의 함수들
// ============================================================================

/**
 * @brief 간단한 텍스트 합성 함수
 *
 * @param model_path 모델 파일 경로
 * @param text 합성할 텍스트
 * @return 생성된 오디오 데이터
 * @throws Exception 합성에 실패한 경우
 */
std::vector<float> synthesizeText(const std::string& model_path, const std::string& text);

/**
 * @brief 간단한 노래 합성 함수
 *
 * @param model_path 모델 파일 경로
 * @param lyrics 가사 텍스트
 * @param notes 음표 배열
 * @return 생성된 오디오 데이터
 * @throws Exception 합성에 실패한 경우
 */
std::vector<float> synthesizeSinging(const std::string& model_path,
                                    const std::string& lyrics,
                                    const std::vector<float>& notes);

} // namespace libetude

#endif // LIBETUDE_CPP_ENGINE_HPP