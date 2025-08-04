/**
 * LibEtude Windows Application Template
 *
 * 이 템플릿은 LibEtude를 사용하는 Windows 애플리케이션의 기본 구조를 제공합니다.
 * Windows 특화 기능들을 활용하여 최적의 성능을 얻을 수 있습니다.
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>

// LibEtude 헤더 파일
#include <libetude/api.h>
#include <libetude/platform/windows.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>  // _kbhit(), _getch()
#endif

class $safeprojectname$App {
private:
    ETContext* context_;
    bool initialized_;

public:
    $safeprojectname$App() : context_(nullptr), initialized_(false) {}

    ~$safeprojectname$App() {
        cleanup();
    }

    /**
     * 애플리케이션 초기화
     * Windows 특화 설정을 적용하여 LibEtude를 초기화합니다.
     */
    bool initialize() {
        std::cout << "LibEtude Windows 애플리케이션 초기화 중..." << std::endl;

        // Windows 특화 설정
        ETWindowsConfig windows_config = {
            .use_wasapi = true,           // WASAPI 오디오 백엔드 사용
            .enable_large_pages = true,   // Large Page 메모리 사용 (성능 향상)
            .enable_etw_logging = false,  // ETW 로깅 (개발 시에만 활성화)
            .thread_pool_min = 2,         // 최소 스레드 수
            .thread_pool_max = 8          // 최대 스레드 수
        };

        // LibEtude Windows 플랫폼 초기화
        ETResult result = et_windows_init(&windows_config);
        if (result != ET_SUCCESS) {
            std::cerr << "Windows 플랫폼 초기화 실패: " << result << std::endl;
            return false;
        }

        // LibEtude 컨텍스트 생성
        context_ = et_create_context();
        if (!context_) {
            std::cerr << "LibEtude 컨텍스트 생성 실패" << std::endl;
            et_windows_finalize();
            return false;
        }

        initialized_ = true;
        std::cout << "초기화 완료!" << std::endl;

        // 시스템 정보 출력
        printSystemInfo();

        return true;
    }

    /**
     * 시스템 정보 출력
     * CPU 기능, 메모리 상태 등을 확인합니다.
     */
    void printSystemInfo() {
        std::cout << "\n=== 시스템 정보 ===" << std::endl;

        // CPU 기능 감지
        ETWindowsCPUFeatures cpu_features = et_windows_detect_cpu_features();
        std::cout << "CPU 기능:" << std::endl;
        std::cout << "  - SSE4.1: " << (cpu_features.has_sse41 ? "지원됨" : "지원 안됨") << std::endl;
        std::cout << "  - AVX: " << (cpu_features.has_avx ? "지원됨" : "지원 안됨") << std::endl;
        std::cout << "  - AVX2: " << (cpu_features.has_avx2 ? "지원됨" : "지원 안됨") << std::endl;
        std::cout << "  - AVX-512: " << (cpu_features.has_avx512 ? "지원됨" : "지원 안됨") << std::endl;

        // 메모리 정보
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        if (GlobalMemoryStatusEx(&mem_status)) {
            std::cout << "메모리 정보:" << std::endl;
            std::cout << "  - 총 물리 메모리: " << (mem_status.ullTotalPhys / (1024 * 1024)) << " MB" << std::endl;
            std::cout << "  - 사용 가능 메모리: " << (mem_status.ullAvailPhys / (1024 * 1024)) << " MB" << std::endl;
        }

        // Large Page 지원 확인
        bool large_page_enabled = et_windows_enable_large_page_privilege();
        std::cout << "Large Page 지원: " << (large_page_enabled ? "활성화됨" : "비활성화됨") << std::endl;

        std::cout << std::endl;
    }

    /**
     * 음성 합성 데모
     * 간단한 텍스트를 음성으로 변환합니다.
     */
    bool runSynthesisDemo() {
        if (!initialized_) {
            std::cerr << "애플리케이션이 초기화되지 않았습니다." << std::endl;
            return false;
        }

        std::cout << "=== 음성 합성 데모 ===" << std::endl;

        // 합성할 텍스트
        std::string demo_text = "안녕하세요, LibEtude Windows 애플리케이션입니다.";
        std::cout << "합성할 텍스트: \"" << demo_text << "\"" << std::endl;

        // 오디오 출력 버퍼 준비 (5초 분량)
        const size_t sample_rate = 22050;
        const size_t max_samples = sample_rate * 5;
        std::vector<float> audio_output(max_samples);

        // 합성 설정
        ETSynthesisConfig config = {
            .sample_rate = static_cast<uint32_t>(sample_rate),
            .channels = 1,
            .format = ET_AUDIO_FORMAT_FLOAT32
        };

        // 성능 측정 시작
        LARGE_INTEGER start_time, end_time, frequency;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_time);

        // 음성 합성 실행
        ETResult result = et_synthesize_text(context_, demo_text.c_str(), &config,
                                           audio_output.data(), audio_output.size());

        // 성능 측정 종료
        QueryPerformanceCounter(&end_time);
        double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / frequency.QuadPart;

        if (result != ET_SUCCESS) {
            std::cerr << "음성 합성 실패: " << result << std::endl;
            return false;
        }

        std::cout << "음성 합성 성공!" << std::endl;
        std::cout << "처리 시간: " << elapsed_ms << " ms" << std::endl;
        std::cout << "생성된 오디오 샘플 수: " << audio_output.size() << std::endl;

        // RTF (Real-Time Factor) 계산
        double audio_duration_ms = (double)audio_output.size() / sample_rate * 1000.0;
        double rtf = elapsed_ms / audio_duration_ms;
        std::cout << "RTF (Real-Time Factor): " << rtf << std::endl;

        if (rtf < 1.0) {
            std::cout << "실시간보다 빠른 처리 성능!" << std::endl;
        }

        return true;
    }

    /**
     * 대화형 메뉴 실행
     */
    void runInteractiveMenu() {
        std::cout << "\n=== LibEtude 대화형 메뉴 ===" << std::endl;
        std::cout << "1. 음성 합성 데모 실행" << std::endl;
        std::cout << "2. 시스템 정보 다시 보기" << std::endl;
        std::cout << "3. 사용자 정의 텍스트 합성" << std::endl;
        std::cout << "4. 종료" << std::endl;
        std::cout << "\n선택하세요 (1-4): ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(); // 개행 문자 제거

        switch (choice) {
            case 1:
                runSynthesisDemo();
                break;

            case 2:
                printSystemInfo();
                break;

            case 3:
                runCustomTextSynthesis();
                break;

            case 4:
                std::cout << "애플리케이션을 종료합니다." << std::endl;
                return;

            default:
                std::cout << "잘못된 선택입니다." << std::endl;
                break;
        }

        // 메뉴 반복
        std::cout << "\n계속하려면 아무 키나 누르세요...";
        _getch();
        runInteractiveMenu();
    }

    /**
     * 사용자 정의 텍스트 합성
     */
    void runCustomTextSynthesis() {
        std::cout << "\n=== 사용자 정의 텍스트 합성 ===" << std::endl;
        std::cout << "합성할 텍스트를 입력하세요: ";

        std::string user_text;
        std::getline(std::cin, user_text);

        if (user_text.empty()) {
            std::cout << "텍스트가 입력되지 않았습니다." << std::endl;
            return;
        }

        // 오디오 출력 버퍼 준비
        const size_t sample_rate = 22050;
        const size_t max_samples = sample_rate * 10; // 10초 분량
        std::vector<float> audio_output(max_samples);

        // 합성 설정
        ETSynthesisConfig config = {
            .sample_rate = static_cast<uint32_t>(sample_rate),
            .channels = 1,
            .format = ET_AUDIO_FORMAT_FLOAT32
        };

        std::cout << "음성 합성 중..." << std::endl;

        // 음성 합성 실행
        ETResult result = et_synthesize_text(context_, user_text.c_str(), &config,
                                           audio_output.data(), audio_output.size());

        if (result == ET_SUCCESS) {
            std::cout << "음성 합성 완료!" << std::endl;
            // 여기에 오디오 재생 코드를 추가할 수 있습니다
        } else {
            std::cerr << "음성 합성 실패: " << result << std::endl;
        }
    }

    /**
     * 리소스 정리
     */
    void cleanup() {
        if (context_) {
            et_destroy_context(context_);
            context_ = nullptr;
        }

        if (initialized_) {
            et_windows_finalize();
            initialized_ = false;
        }
    }
};

/**
 * 메인 함수
 */
int main() {
    // 콘솔 출력 인코딩 설정 (한글 지원)
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "LibEtude Windows 애플리케이션 시작" << std::endl;
    std::cout << "프로젝트: $projectname$" << std::endl;
    std::cout << "========================================" << std::endl;

    // 애플리케이션 인스턴스 생성
    $safeprojectname$App app;

    // 초기화
    if (!app.initialize()) {
        std::cerr << "애플리케이션 초기화 실패" << std::endl;
        std::cout << "아무 키나 누르면 종료합니다...";
        _getch();
        return -1;
    }

    // 기본 데모 실행
    app.runSynthesisDemo();

    // 대화형 메뉴 실행
    app.runInteractiveMenu();

    std::cout << "\n애플리케이션이 정상적으로 종료되었습니다." << std::endl;
    return 0;
}