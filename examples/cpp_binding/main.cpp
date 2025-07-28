/**
 * @file main.cpp
 * @brief LibEtude C++ 바인딩 사용 예제
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <libetude/cpp/engine.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>

using namespace libetude;

/**
 * @brief WAV 파일로 오디오 데이터를 저장하는 간단한 함수
 * @param filename 파일명
 * @param audio_data 오디오 데이터
 * @param sample_rate 샘플링 레이트
 */
void saveWavFile(const std::string& filename, const std::vector<float>& audio_data, int sample_rate = 22050) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "파일을 열 수 없습니다: " << filename << std::endl;
        return;
    }

    // 간단한 WAV 헤더 (실제 구현에서는 더 정확한 WAV 라이브러리 사용 권장)
    const int num_samples = static_cast<int>(audio_data.size());
    const int byte_rate = sample_rate * 2; // 16-bit mono
    const int data_size = num_samples * 2;
    const int file_size = 36 + data_size;

    // WAV 헤더 작성
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);

    int fmt_size = 16;
    short audio_format = 1; // PCM
    short num_channels = 1; // Mono
    short bits_per_sample = 16;
    short block_align = num_channels * bits_per_sample / 8;

    file.write(reinterpret_cast<const char*>(&fmt_size), 4);
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    file.write(reinterpret_cast<const char*>(&num_channels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);

    // 오디오 데이터 작성 (float를 16-bit PCM으로 변환)
    for (float sample : audio_data) {
        short pcm_sample = static_cast<short>(sample * 32767.0f);
        file.write(reinterpret_cast<const char*>(&pcm_sample), 2);
    }

    std::cout << "오디오 파일 저장 완료: " << filename << std::endl;
}

/**
 * @brief 기본 텍스트 음성 합성 예제
 */
void basicTextToSpeechExample() {
    std::cout << "\n=== 기본 텍스트 음성 합성 예제 ===" << std::endl;

    try {
        // 엔진 생성
        std::cout << "엔진 생성 중..." << std::endl;
        Engine engine("models/korean_tts.lef");

        // 버전 정보 출력
        std::cout << "LibEtude 버전: " << Engine::getVersion() << std::endl;
        std::cout << "하드웨어 기능: 0x" << std::hex << Engine::getHardwareFeatures() << std::dec << std::endl;

        // 텍스트 합성
        const std::string text = "안녕하세요! LibEtude C++ 바인딩 테스트입니다.";
        std::cout << "텍스트 합성 중: \"" << text << "\"" << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<float> audio = engine.synthesizeText(text);
        auto end_time = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "합성 완료! 시간: " << duration.count() << "ms, 샘플 수: " << audio.size() << std::endl;

        // 성능 통계 출력
        PerformanceStats stats = engine.getPerformanceStats();
        std::cout << "성능 통계:" << std::endl;
        std::cout << "  추론 시간: " << stats.inference_time_ms << "ms" << std::endl;
        std::cout << "  메모리 사용량: " << stats.memory_usage_mb << "MB" << std::endl;
        std::cout << "  CPU 사용률: " << stats.cpu_usage_percent << "%" << std::endl;

        // WAV 파일로 저장
        saveWavFile("output_basic.wav", audio);

    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
        std::cerr << "오류 코드: " << e.getErrorCode() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "표준 오류: " << e.what() << std::endl;
    }
}

/**
 * @brief 비동기 음성 합성 예제
 */
void asyncSynthesisExample() {
    std::cout << "\n=== 비동기 음성 합성 예제 ===" << std::endl;

    try {
        Engine engine("models/korean_tts.lef");

        // 여러 텍스트를 비동기로 합성
        std::vector<std::string> texts = {
            "첫 번째 문장입니다.",
            "두 번째 문장입니다.",
            "세 번째 문장입니다."
        };

        std::vector<std::future<std::vector<float>>> futures;

        // 비동기 작업 시작
        std::cout << "비동기 합성 작업 시작..." << std::endl;
        for (const auto& text : texts) {
            futures.push_back(engine.synthesizeTextAsync(text));
        }

        // 결과 수집
        for (size_t i = 0; i < futures.size(); ++i) {
            std::vector<float> audio = futures[i].get();
            std::cout << "텍스트 " << (i + 1) << " 합성 완료: " << audio.size() << " 샘플" << std::endl;

            // 각각 다른 파일로 저장
            saveWavFile("output_async_" + std::to_string(i + 1) + ".wav", audio);
        }

    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    }
}

/**
 * @brief 실시간 스트리밍 예제
 */
void streamingExample() {
    std::cout << "\n=== 실시간 스트리밍 예제 ===" << std::endl;

    try {
        Engine engine("models/korean_tts.lef");

        // 스트리밍 콜백 설정
        std::vector<float> accumulated_audio;
        auto callback = [&accumulated_audio](const std::vector<float>& audio) {
            std::cout << "오디오 청크 수신: " << audio.size() << " 샘플" << std::endl;
            accumulated_audio.insert(accumulated_audio.end(), audio.begin(), audio.end());
        };

        // 스트리밍 시작
        std::cout << "스트리밍 시작..." << std::endl;
        engine.startStreaming(callback);

        // 여러 텍스트를 순차적으로 스트리밍
        std::vector<std::string> streaming_texts = {
            "실시간 스트리밍 테스트입니다.",
            "첫 번째 청크입니다.",
            "두 번째 청크입니다.",
            "스트리밍이 잘 작동하고 있습니다."
        };

        for (const auto& text : streaming_texts) {
            std::cout << "스트리밍: \"" << text << "\"" << std::endl;
            engine.streamText(text);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 잠시 대기
        }

        // 스트리밍 완료 대기
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 스트리밍 중지
        std::cout << "스트리밍 중지..." << std::endl;
        engine.stopStreaming();

        std::cout << "총 누적 오디오: " << accumulated_audio.size() << " 샘플" << std::endl;
        saveWavFile("output_streaming.wav", accumulated_audio);

    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    }
}

/**
 * @brief 품질 모드 및 GPU 가속 예제
 */
void qualityAndGpuExample() {
    std::cout << "\n=== 품질 모드 및 GPU 가속 예제 ===" << std::endl;

    try {
        Engine engine("models/korean_tts.lef");
        const std::string test_text = "품질 모드 테스트 문장입니다.";

        // 다양한 품질 모드 테스트
        std::vector<QualityMode> quality_modes = {
            QualityMode::Fast,
            QualityMode::Balanced,
            QualityMode::High
        };

        std::vector<std::string> mode_names = {"Fast", "Balanced", "High"};

        for (size_t i = 0; i < quality_modes.size(); ++i) {
            std::cout << "\n품질 모드: " << mode_names[i] << std::endl;
            engine.setQualityMode(quality_modes[i]);

            auto start_time = std::chrono::high_resolution_clock::now();
            std::vector<float> audio = engine.synthesizeText(test_text);
            auto end_time = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "합성 시간: " << duration.count() << "ms" << std::endl;

            PerformanceStats stats = engine.getPerformanceStats();
            std::cout << "추론 시간: " << stats.inference_time_ms << "ms" << std::endl;

            saveWavFile("output_quality_" + mode_names[i] + ".wav", audio);
        }

        // GPU 가속 테스트 (사용 가능한 경우)
        try {
            std::cout << "\nGPU 가속 활성화 시도..." << std::endl;
            engine.enableGPUAcceleration(true);
            std::cout << "GPU 가속 활성화됨" << std::endl;

            auto start_time = std::chrono::high_resolution_clock::now();
            std::vector<float> gpu_audio = engine.synthesizeText(test_text);
            auto end_time = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "GPU 가속 합성 시간: " << duration.count() << "ms" << std::endl;

            saveWavFile("output_gpu.wav", gpu_audio);

        } catch (const HardwareException& e) {
            std::cout << "GPU 가속을 사용할 수 없습니다: " << e.what() << std::endl;
        }

    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    }
}

/**
 * @brief 확장 모델 예제
 */
void extensionExample() {
    std::cout << "\n=== 확장 모델 예제 ===" << std::endl;

    try {
        Engine engine("models/korean_tts.lef");

        // 확장 모델 로드
        std::cout << "확장 모델 로드 중..." << std::endl;
        int extension_id = engine.loadExtension("models/speaker_extension.lefx");
        std::cout << "확장 모델 로드됨, ID: " << extension_id << std::endl;

        // 확장 모델을 사용한 합성
        const std::string text = "확장 모델을 사용한 음성 합성입니다.";
        std::vector<float> audio = engine.synthesizeText(text);

        std::cout << "확장 모델 합성 완료: " << audio.size() << " 샘플" << std::endl;
        saveWavFile("output_extension.wav", audio);

        // 로드된 확장 모델 목록 출력
        auto loaded_extensions = engine.getLoadedExtensions();
        std::cout << "로드된 확장 모델 수: " << loaded_extensions.size() << std::endl;

        // 확장 모델 언로드
        std::cout << "확장 모델 언로드 중..." << std::endl;
        engine.unloadExtension(extension_id);
        std::cout << "확장 모델 언로드 완료" << std::endl;

    } catch (const ModelException& e) {
        std::cout << "확장 모델을 찾을 수 없습니다: " << e.what() << std::endl;
    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    }
}

/**
 * @brief 편의 함수 사용 예제
 */
void convenienceFunctionExample() {
    std::cout << "\n=== 편의 함수 사용 예제 ===" << std::endl;

    try {
        // 간단한 텍스트-음성 변환
        std::cout << "편의 함수를 사용한 간단한 TTS..." << std::endl;
        std::vector<float> audio = textToSpeech("models/korean_tts.lef", "편의 함수 테스트입니다.");

        std::cout << "합성 완료: " << audio.size() << " 샘플" << std::endl;
        saveWavFile("output_convenience.wav", audio);

        // 엔진 팩토리 함수 사용
        auto engine = createEngine("models/korean_tts.lef");
        std::vector<float> audio2 = engine->synthesizeText("팩토리 함수 테스트입니다.");

        std::cout << "팩토리 함수 합성 완료: " << audio2.size() << " 샘플" << std::endl;
        saveWavFile("output_factory.wav", audio2);

    } catch (const Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "LibEtude C++ 바인딩 예제 프로그램" << std::endl;
    std::cout << "====================================" << std::endl;

    // 모델 파일 존재 확인 (실제 구현에서는 적절한 모델 파일 경로 사용)
    std::cout << "주의: 이 예제는 'models/korean_tts.lef' 파일이 필요합니다." << std::endl;
    std::cout << "실제 모델 파일이 없으면 일부 예제가 실패할 수 있습니다." << std::endl;

    try {
        // 각 예제 실행
        basicTextToSpeechExample();
        asyncSynthesisExample();
        streamingExample();
        qualityAndGpuExample();
        extensionExample();
        convenienceFunctionExample();

        std::cout << "\n모든 예제 실행 완료!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "예상치 못한 오류: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}