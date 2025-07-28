/**
 * @file engine.cpp
 * @brief LibEtude C++ 바인딩 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "../../include/libetude/cpp/engine.hpp"
#include <thread>
#include <sstream>
#include <algorithm>

namespace libetude {

// ============================================================================
// 예외 클래스 구현
// ============================================================================

std::string Exception::formatMessage(LibEtudeErrorCode code, const std::string& message) {
    std::ostringstream oss;
    oss << "LibEtude Error [" << code << "]";

    // 오류 코드에 따른 기본 메시지
    switch (code) {
        case LIBETUDE_ERROR_INVALID_ARGUMENT:
            oss << " Invalid argument";
            break;
        case LIBETUDE_ERROR_OUT_OF_MEMORY:
            oss << " Out of memory";
            break;
        case LIBETUDE_ERROR_IO:
            oss << " I/O error";
            break;
        case LIBETUDE_ERROR_NOT_IMPLEMENTED:
            oss << " Not implemented";
            break;
        case LIBETUDE_ERROR_RUNTIME:
            oss << " Runtime error";
            break;
        case LIBETUDE_ERROR_HARDWARE:
            oss << " Hardware error";
            break;
        case LIBETUDE_ERROR_MODEL:
            oss << " Model error";
            break;
        case LIBETUDE_ERROR_TIMEOUT:
            oss << " Timeout";
            break;
        case LIBETUDE_ERROR_NOT_INITIALIZED:
            oss << " Not initialized";
            break;
        case LIBETUDE_ERROR_ALREADY_INITIALIZED:
            oss << " Already initialized";
            break;
        case LIBETUDE_ERROR_UNSUPPORTED:
            oss << " Unsupported";
            break;
        case LIBETUDE_ERROR_NOT_FOUND:
            oss << " Not found";
            break;
        case LIBETUDE_ERROR_INVALID_STATE:
            oss << " Invalid state";
            break;
        case LIBETUDE_ERROR_BUFFER_FULL:
            oss << " Buffer full";
            break;
        default:
            oss << " Unknown error";
            break;
    }

    if (!message.empty()) {
        oss << ": " << message;
    }

    return oss.str();
}

void throwOnError(LibEtudeErrorCode code, const std::string& message) {
    if (code == LIBETUDE_SUCCESS) {
        return;
    }

    switch (code) {
        case LIBETUDE_ERROR_INVALID_ARGUMENT:
            throw InvalidArgumentException(message);
        case LIBETUDE_ERROR_OUT_OF_MEMORY:
            throw OutOfMemoryException(message);
        case LIBETUDE_ERROR_RUNTIME:
            throw RuntimeException(message);
        case LIBETUDE_ERROR_MODEL:
            throw ModelException(message);
        case LIBETUDE_ERROR_HARDWARE:
            throw HardwareException(message);
        default:
            throw Exception(code, message);
    }
}

// ============================================================================
// Engine 클래스 구현
// ============================================================================

Engine::Engine(const std::string& model_path)
    : engine_(nullptr)
    , streaming_active_(false)
    , current_quality_mode_(QualityMode::Balanced)
    , gpu_acceleration_enabled_(false)
    , stream_callback_(nullptr) {

    if (model_path.empty()) {
        throw InvalidArgumentException("Model path cannot be empty");
    }

    engine_ = libetude_create_engine(model_path.c_str());
    if (!engine_) {
        std::string error_msg = libetude_get_last_error();
        throw ModelException("Failed to create engine: " + error_msg);
    }
}

Engine::Engine(Engine&& other) noexcept
    : engine_(other.engine_)
    , streaming_active_(other.streaming_active_)
    , current_quality_mode_(other.current_quality_mode_)
    , gpu_acceleration_enabled_(other.gpu_acceleration_enabled_)
    , loaded_extensions_(std::move(other.loaded_extensions_))
    , stream_callback_(std::move(other.stream_callback_)) {

    other.engine_ = nullptr;
    other.streaming_active_ = false;
}

Engine::~Engine() {
    cleanup();
}

Engine& Engine::operator=(Engine&& other) noexcept {
    if (this != &other) {
        cleanup();

        engine_ = other.engine_;
        streaming_active_ = other.streaming_active_;
        current_quality_mode_ = other.current_quality_mode_;
        gpu_acceleration_enabled_ = other.gpu_acceleration_enabled_;
        loaded_extensions_ = std::move(other.loaded_extensions_);
        stream_callback_ = std::move(other.stream_callback_);

        other.engine_ = nullptr;
        other.streaming_active_ = false;
    }
    return *this;
}

void Engine::cleanup() noexcept {
    if (engine_) {
        try {
            if (streaming_active_) {
                libetude_stop_streaming(engine_);
            }
        } catch (...) {
            // 소멸자에서는 예외를 던지지 않음
        }

        libetude_destroy_engine(engine_);
        engine_ = nullptr;
    }
    streaming_active_ = false;
}

// ========================================================================
// 음성 합성 (동기 처리)
// ========================================================================

std::vector<float> Engine::synthesizeText(const std::string& text) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (text.empty()) {
        throw InvalidArgumentException("Text cannot be empty");
    }

    if (text.length() > LIBETUDE_MAX_TEXT_LENGTH) {
        throw InvalidArgumentException("Text is too long");
    }

    // 초기 버퍼 크기 추정 (텍스트 길이 * 샘플레이트 * 평균 지속시간)
    const int estimated_duration_sec = static_cast<int>(text.length() * 0.1); // 대략적인 추정
    const int buffer_size = std::max(1024, estimated_duration_sec * LIBETUDE_DEFAULT_SAMPLE_RATE);

    std::vector<float> audio_buffer(buffer_size);
    int actual_length = buffer_size;

    int result = libetude_synthesize_text(engine_, text.c_str(),
                                         audio_buffer.data(), &actual_length);

    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Text synthesis failed: " + error_msg);
    }

    // 실제 길이로 벡터 크기 조정
    audio_buffer.resize(actual_length);
    return audio_buffer;
}

std::vector<float> Engine::synthesizeSinging(const std::string& lyrics,
                                            const std::vector<float>& notes) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (lyrics.empty()) {
        throw InvalidArgumentException("Lyrics cannot be empty");
    }

    if (notes.empty()) {
        throw InvalidArgumentException("Notes cannot be empty");
    }

    // 초기 버퍼 크기 추정
    const int estimated_duration_sec = static_cast<int>(notes.size() * 0.5); // 음표당 0.5초 추정
    const int buffer_size = std::max(1024, estimated_duration_sec * LIBETUDE_DEFAULT_SAMPLE_RATE);

    std::vector<float> audio_buffer(buffer_size);
    int actual_length = buffer_size;

    int result = libetude_synthesize_singing(engine_, lyrics.c_str(),
                                           notes.data(), static_cast<int>(notes.size()),
                                           audio_buffer.data(), &actual_length);

    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Singing synthesis failed: " + error_msg);
    }

    audio_buffer.resize(actual_length);
    return audio_buffer;
}

// ========================================================================
// 비동기 음성 합성
// ========================================================================

std::future<std::vector<float>> Engine::synthesizeTextAsync(const std::string& text) {
    return std::async(std::launch::async, [this, text]() {
        return synthesizeText(text);
    });
}

std::future<std::vector<float>> Engine::synthesizeSingingAsync(const std::string& lyrics,
                                                              const std::vector<float>& notes) {
    return std::async(std::launch::async, [this, lyrics, notes]() {
        return synthesizeSinging(lyrics, notes);
    });
}

// ========================================================================
// 실시간 스트리밍 (비동기 처리)
// ========================================================================

void Engine::startStreaming(AudioStreamCallback callback) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (!callback) {
        throw InvalidArgumentException("Callback cannot be null");
    }

    std::lock_guard<std::mutex> lock(stream_mutex_);

    if (streaming_active_) {
        throw RuntimeException("Streaming is already active");
    }

    stream_callback_ = std::make_unique<AudioStreamCallback>(std::move(callback));

    int result = libetude_start_streaming(engine_, streamCallbackWrapper, this);
    if (result != LIBETUDE_SUCCESS) {
        stream_callback_.reset();
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to start streaming: " + error_msg);
    }

    streaming_active_ = true;
}

void Engine::streamText(const std::string& text) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (text.empty()) {
        throw InvalidArgumentException("Text cannot be empty");
    }

    std::lock_guard<std::mutex> lock(stream_mutex_);

    if (!streaming_active_) {
        throw RuntimeException("Streaming is not active");
    }

    int result = libetude_stream_text(engine_, text.c_str());
    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to stream text: " + error_msg);
    }
}

void Engine::stopStreaming() {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    std::lock_guard<std::mutex> lock(stream_mutex_);

    if (!streaming_active_) {
        return; // 이미 중지됨
    }

    int result = libetude_stop_streaming(engine_);
    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to stop streaming: " + error_msg);
    }

    streaming_active_ = false;
    stream_callback_.reset();
}

void Engine::streamCallbackWrapper(const float* audio, int length, void* user_data) {
    Engine* engine = static_cast<Engine*>(user_data);
    if (!engine || !engine->stream_callback_) {
        return;
    }

    try {
        std::vector<float> audio_vector(audio, audio + length);
        (*engine->stream_callback_)(audio_vector);
    } catch (...) {
        // 콜백에서 예외가 발생해도 C 코드로 전파되지 않도록 함
    }
}

// ========================================================================
// 성능 제어 및 모니터링
// ========================================================================

void Engine::setQualityMode(QualityMode mode) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    int result = libetude_set_quality_mode(engine_, static_cast<::QualityMode>(mode));
    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to set quality mode: " + error_msg);
    }

    current_quality_mode_ = mode;
}

void Engine::enableGPUAcceleration(bool enable) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (enable) {
        int result = libetude_enable_gpu_acceleration(engine_);
        if (result != LIBETUDE_SUCCESS) {
            std::string error_msg = libetude_get_last_error();
            throwOnError(static_cast<LibEtudeErrorCode>(result),
                        "Failed to enable GPU acceleration: " + error_msg);
        }
    }

    gpu_acceleration_enabled_ = enable;
}

PerformanceStats Engine::getPerformanceStats() {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    ::PerformanceStats c_stats;
    int result = libetude_get_performance_stats(engine_, &c_stats);
    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to get performance stats: " + error_msg);
    }

    return PerformanceStats(c_stats);
}

// ========================================================================
// 확장 모델 관리
// ========================================================================

int Engine::loadExtension(const std::string& extension_path) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    if (extension_path.empty()) {
        throw InvalidArgumentException("Extension path cannot be empty");
    }

    int result = libetude_load_extension(engine_, extension_path.c_str());
    if (result < 0) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to load extension: " + error_msg);
    }

    int extension_id = result;
    loaded_extensions_.push_back(extension_id);
    return extension_id;
}

void Engine::unloadExtension(int extension_id) {
    if (!engine_) {
        throw RuntimeException("Engine is not initialized");
    }

    auto it = std::find(loaded_extensions_.begin(), loaded_extensions_.end(), extension_id);
    if (it == loaded_extensions_.end()) {
        throw InvalidArgumentException("Extension ID not found");
    }

    int result = libetude_unload_extension(engine_, extension_id);
    if (result != LIBETUDE_SUCCESS) {
        std::string error_msg = libetude_get_last_error();
        throwOnError(static_cast<LibEtudeErrorCode>(result),
                    "Failed to unload extension: " + error_msg);
    }

    loaded_extensions_.erase(it);
}

// ========================================================================
// 정적 유틸리티 함수
// ========================================================================

std::string Engine::getVersion() {
    const char* version = libetude_get_version();
    return version ? std::string(version) : std::string();
}

uint32_t Engine::getHardwareFeatures() {
    return libetude_get_hardware_features();
}

std::string Engine::getLastError() {
    const char* error = libetude_get_last_error();
    return error ? std::string(error) : std::string();
}

// ============================================================================
// 편의 함수들
// ============================================================================

std::unique_ptr<Engine> createEngine(const std::string& model_path) {
    return std::make_unique<Engine>(model_path);
}

std::vector<float> textToSpeech(const std::string& model_path, const std::string& text) {
    Engine engine(model_path);
    return engine.synthesizeText(text);
}

} // namespace libetude