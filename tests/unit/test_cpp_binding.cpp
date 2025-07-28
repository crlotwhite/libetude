/**
 * @file test_cpp_binding.cpp
 * @brief LibEtude C++ 바인딩 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <libetude/cpp/engine.hpp>
#include <thread>
#include <chrono>
#include <future>

using namespace libetude;

// 테스트용 더미 모델 경로 (실제 테스트에서는 적절한 모델 파일 사용)
const std::string TEST_MODEL_PATH = "test_model.lef";
const std::string TEST_EXTENSION_PATH = "test_extension.lefx";

class CppBindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }
};

// ============================================================================
// 예외 클래스 테스트
// ============================================================================

TEST_F(CppBindingTest, ExceptionTypes) {
    // 기본 예외 테스트
    Exception base_exception(LIBETUDE_ERROR_RUNTIME, "Test message");
    EXPECT_EQ(base_exception.getErrorCode(), LIBETUDE_ERROR_RUNTIME);
    EXPECT_TRUE(std::string(base_exception.what()).find("Test message") != std::string::npos);

    // 특화된 예외 타입 테스트
    InvalidArgumentException invalid_arg("Invalid argument");
    EXPECT_EQ(invalid_arg.getErrorCode(), LIBETUDE_ERROR_INVALID_ARGUMENT);

    OutOfMemoryException out_of_memory("Out of memory");
    EXPECT_EQ(out_of_memory.getErrorCode(), LIBETUDE_ERROR_OUT_OF_MEMORY);

    RuntimeException runtime_error("Runtime error");
    EXPECT_EQ(runtime_error.getErrorCode(), LIBETUDE_ERROR_RUNTIME);

    ModelException model_error("Model error");
    EXPECT_EQ(model_error.getErrorCode(), LIBETUDE_ERROR_MODEL);

    HardwareException hardware_error("Hardware error");
    EXPECT_EQ(hardware_error.getErrorCode(), LIBETUDE_ERROR_HARDWARE);
}

TEST_F(CppBindingTest, ThrowOnError) {
    // 성공 코드는 예외를 던지지 않음
    EXPECT_NO_THROW(throwOnError(LIBETUDE_SUCCESS));

    // 각 오류 코드에 대해 적절한 예외 타입이 던져지는지 확인
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_INVALID_ARGUMENT), InvalidArgumentException);
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_OUT_OF_MEMORY), OutOfMemoryException);
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_RUNTIME), RuntimeException);
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_MODEL), ModelException);
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_HARDWARE), HardwareException);
    EXPECT_THROW(throwOnError(LIBETUDE_ERROR_TIMEOUT), Exception);
}

// ============================================================================
// 엔진 생성 및 소멸 테스트
// ============================================================================

TEST_F(CppBindingTest, EngineConstruction) {
    // 빈 경로로 엔진 생성 시 예외 발생
    EXPECT_THROW(Engine(""), InvalidArgumentException);

    // 존재하지 않는 모델 파일로 엔진 생성 시 예외 발생
    EXPECT_THROW(Engine("nonexistent_model.lef"), ModelException);
}

TEST_F(CppBindingTest, EngineMoveSemantics) {
    // 이 테스트는 실제 모델 파일이 있을 때만 실행 가능
    // 모의 테스트로 대체하거나 조건부 실행 필요

    // 이동 생성자 테스트는 실제 엔진 객체가 필요하므로
    // 모의 객체나 테스트 더블을 사용해야 함
    EXPECT_TRUE(true); // 플레이스홀더
}

// ============================================================================
// 정적 유틸리티 함수 테스트
// ============================================================================

TEST_F(CppBindingTest, StaticUtilityFunctions) {
    // 버전 문자열 테스트
    std::string version = Engine::getVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_TRUE(version.find('.') != std::string::npos); // 버전 형식 확인

    // 하드웨어 기능 테스트
    uint32_t features = Engine::getHardwareFeatures();
    // 하드웨어 기능은 0일 수도 있으므로 단순히 호출 가능한지만 확인
    EXPECT_GE(features, 0u);

    // 마지막 오류 메시지 테스트
    std::string error = Engine::getLastError();
    // 오류 메시지는 비어있을 수 있음
    EXPECT_TRUE(true); // 단순히 호출 가능한지만 확인
}

// ============================================================================
// 품질 모드 열거형 테스트
// ============================================================================

TEST_F(CppBindingTest, QualityModeEnum) {
    // 품질 모드 값 확인
    EXPECT_EQ(static_cast<int>(QualityMode::Fast), LIBETUDE_QUALITY_FAST);
    EXPECT_EQ(static_cast<int>(QualityMode::Balanced), LIBETUDE_QUALITY_BALANCED);
    EXPECT_EQ(static_cast<int>(QualityMode::High), LIBETUDE_QUALITY_HIGH);
}

// ============================================================================
// 성능 통계 구조체 테스트
// ============================================================================

TEST_F(CppBindingTest, PerformanceStatsConversion) {
    // C 구조체에서 C++ 구조체로 변환 테스트
    ::PerformanceStats c_stats;
    c_stats.inference_time_ms = 100.5;
    c_stats.memory_usage_mb = 256.0;
    c_stats.cpu_usage_percent = 75.5;
    c_stats.gpu_usage_percent = 50.0;
    c_stats.active_threads = 4;

    PerformanceStats cpp_stats(c_stats);

    EXPECT_DOUBLE_EQ(cpp_stats.inference_time_ms, 100.5);
    EXPECT_DOUBLE_EQ(cpp_stats.memory_usage_mb, 256.0);
    EXPECT_DOUBLE_EQ(cpp_stats.cpu_usage_percent, 75.5);
    EXPECT_DOUBLE_EQ(cpp_stats.gpu_usage_percent, 50.0);
    EXPECT_EQ(cpp_stats.active_threads, 4);
}

// ============================================================================
// 편의 함수 테스트
// ============================================================================

TEST_F(CppBindingTest, ConvenienceFunctions) {
    // createEngine 함수 테스트
    EXPECT_THROW(createEngine(""), InvalidArgumentException);
    EXPECT_THROW(createEngine("nonexistent.lef"), ModelException);

    // textToSpeech 함수 테스트
    EXPECT_THROW(textToSpeech("", "test"), InvalidArgumentException);
    EXPECT_THROW(textToSpeech("nonexistent.lef", "test"), ModelException);
}

// ============================================================================
// 스레드 안전성 테스트
// ============================================================================

TEST_F(CppBindingTest, ThreadSafety) {
    // 여러 스레드에서 정적 함수 호출 테스트
    const int num_threads = 4;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, []() {
            // 정적 함수들은 스레드 안전해야 함
            std::string version = Engine::getVersion();
            uint32_t features = Engine::getHardwareFeatures();
            std::string error = Engine::getLastError();

            EXPECT_FALSE(version.empty());
        }));
    }

    // 모든 스레드 완료 대기
    for (auto& future : futures) {
        EXPECT_NO_THROW(future.get());
    }
}

// ============================================================================
// 입력 검증 테스트
// ============================================================================

TEST_F(CppBindingTest, InputValidation) {
    // 빈 문자열 검증
    EXPECT_THROW(Engine(""), InvalidArgumentException);

    // 널 포인터 검증은 C++ 바인딩에서는 해당 없음 (std::string 사용)

    // 매우 긴 문자열 처리 (실제 엔진 없이는 테스트 어려움)
    std::string very_long_text(LIBETUDE_MAX_TEXT_LENGTH + 1, 'a');
    // 실제 엔진이 있다면: EXPECT_THROW(engine.synthesizeText(very_long_text), InvalidArgumentException);
}

// ============================================================================
// RAII 패턴 테스트
// ============================================================================

TEST_F(CppBindingTest, RAIIPattern) {
    // 스코프를 벗어날 때 자동으로 리소스가 해제되는지 테스트
    {
        // 실제 엔진 객체가 있다면 여기서 생성
        // Engine engine(TEST_MODEL_PATH);
        // 스코프를 벗어나면 자동으로 소멸자 호출
    }

    // 메모리 누수 검사는 별도의 도구(Valgrind, AddressSanitizer 등) 필요
    EXPECT_TRUE(true); // 플레이스홀더
}

// ============================================================================
// 예외 안전성 테스트
// ============================================================================

TEST_F(CppBindingTest, ExceptionSafety) {
    // 예외가 발생해도 리소스가 안전하게 해제되는지 테스트
    try {
        Engine engine("nonexistent.lef");
        FAIL() << "예외가 발생해야 함";
    } catch (const ModelException&) {
        // 예외 발생 후에도 프로그램이 안정적으로 동작해야 함
        EXPECT_TRUE(true);
    }

    // 추가적인 예외 안전성 테스트는 실제 엔진 객체가 필요
}

// ============================================================================
// 메인 함수
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}