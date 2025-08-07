/**
 * @file cross_platform_demo.c
 * @brief 크로스 플랫폼 데모 애플리케이션
 *
 * 이 예제는 모든 플랫폼 추상화 레이어 인터페이스를 통합하여 사용하는
 * 완전한 크로스 플랫폼 애플리케이션을 구현합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#include <libetude/platform/factory.h>
#include <libetude/platform/audio.h>
#include <libetude/platform/system.h>
#include <libetude/platform/threading.h>
#include <libetude/platform/memory.h>
#include <libetude/platform/filesystem.h>
#include <libetude/platform/network.h>
#include <libetude/error.h>

// 전역 인터페이스
static ETAudioInterface* g_audio = NULL;
static ETSystemInterface* g_system = NULL;
static ETThreadInterface* g_thread = NULL;
static ETMemoryInterface* g_memory = NULL;
static ETFilesystemInterface* g_filesystem = NULL;
static ETNetworkInterface* g_network = NULL;

// 전역 상태
static volatile int g_running = 1;
static ETAudioDevice* g_audio_device = NULL;
static ETThread* g_worker_threads[4] = {NULL};
static ETMutex* g_log_mutex = NULL;

// 로그 시스템
typedef struct {
    char message[256];
    uint64_t timestamp;
    int thread_id;
} LogEntry;

static LogEntry g_log_buffer[1000];
static int g_log_count = 0;

/**
 * @brief 신호 핸들러
 */
void signal_handler(int sig) {
    printf("\n프로그램을 종료합니다...\n");
    g_running = 0;
}

/**
 * @brief 스레드 안전 로깅
 */
void safe_log(const char* format, ...) {
    if (!g_log_mutex) return;

    g_thread->lock_mutex(g_log_mutex);

    if (g_log_count < 1000) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_log_buffer[g_log_count].message,
                 sizeof(g_log_buffer[g_log_count].message), format, args);
        va_end(args);

        g_system->get_high_resolution_time(&g_log_buffer[g_log_count].timestamp);
        g_log_buffer[g_log_count].thread_id = g_thread->get_current_thread_id();
        g_log_count++;
    }

    g_thread->unlock_mutex(g_log_mutex);

    // 콘솔에도 출력
    printf("[%d] %s\n", g_thread->get_current_thread_id(),
           g_log_buffer[g_log_count - 1].message);
}

/**
 * @brief 시스템 정보 수집 및 저장
 */
ETResult collect_system_info(void) {
    safe_log("시스템 정보 수집 시작");

    // 시스템 정보 수집
    ETSystemInfo sys_info;
    ETResult result = g_system->get_system_info(&sys_info);
    if (result != ET_SUCCESS) {
        safe_log("시스템 정보 수집 실패: %d", result);
        return result;
    }

    // 파일에 저장
    ETFile* file;
    result = g_filesystem->open_file("system_info.txt",
                                    ET_FILE_MODE_WRITE, &file);
    if (result != ET_SUCCESS) {
        safe_log("파일 열기 실패: %d", result);
        return result;
    }

    char buffer[1024];
    size_t written;

    snprintf(buffer, sizeof(buffer),
             "=== 시스템 정보 ===\n"
             "시스템: %s\n"
             "CPU: %s\n"
             "코어 수: %u\n"
             "메모리: %llu MB\n"
             "수집 시간: %llu\n",
             sys_info.system_name,
             sys_info.cpu_name,
             sys_info.cpu_count,
             sys_info.total_memory / (1024 * 1024),
             g_system->get_high_resolution_time(NULL));

    result = g_filesystem->write_file(file, buffer, strlen(buffer), &written);
    g_filesystem->close_file(file);

    if (result == ET_SUCCESS) {
        safe_log("시스템 정보를 system_info.txt에 저장 완료");
    } else {
        safe_log("파일 쓰기 실패: %d", result);
    }

    return result;
}

/**
 * @brief 메모리 사용량 모니터링 스레드
 */
void* memory_monitor_thread(void* arg) {
    safe_log("메모리 모니터링 스레드 시작");

    while (g_running) {
        ETMemoryUsage usage;
        ETResult result = g_system->get_memory_usage(&usage);

        if (result == ET_SUCCESS) {
            safe_log("메모리 사용률: %.1f%% (물리: %s, 가상: %s)",
                    usage.usage_percent,
                    format_bytes(usage.physical_used),
                    format_bytes(usage.virtual_used));
        }

        g_system->sleep(5000); // 5초 간격
    }

    safe_log("메모리 모니터링 스레드 종료");
    return NULL;
}

/**
 * @brief CPU 사용률 모니터링 스레드
 */
void* cpu_monitor_thread(void* arg) {
    safe_log("CPU 모니터링 스레드 시작");

    while (g_running) {
        float cpu_usage;
        ETResult result = g_system->get_cpu_usage(&cpu_usage);

        if (result == ET_SUCCESS) {
            safe_log("CPU 사용률: %.1f%%", cpu_usage);
        }

        g_system->sleep(3000); // 3초 간격
    }

    safe_log("CPU 모니터링 스레드 종료");
    return NULL;
}

/**
 * @brief 파일 시스템 작업 스레드
 */
void* filesystem_worker_thread(void* arg) {
    int worker_id = *(int*)arg;
    safe_log("파일시스템 작업자 스레드 %d 시작", worker_id);

    char filename[64];
    snprintf(filename, sizeof(filename), "worker_%d_output.txt", worker_id);

    ETFile* file;
    ETResult result = g_filesystem->open_file(filename,
                                             ET_FILE_MODE_WRITE, &file);
    if (result != ET_SUCCESS) {
        safe_log("작업자 %d: 파일 열기 실패", worker_id);
        return NULL;
    }

    int iteration = 0;
    while (g_running && iteration < 10) {
        char buffer[256];
        size_t written;

        snprintf(buffer, sizeof(buffer),
                "작업자 %d - 반복 %d - 시간: %llu\n",
                worker_id, iteration,
                g_system->get_high_resolution_time(NULL));

        result = g_filesystem->write_file(file, buffer, strlen(buffer), &written);
        if (result != ET_SUCCESS) {
            safe_log("작업자 %d: 파일 쓰기 실패", worker_id);
            break;
        }

        iteration++;
        g_system->sleep(1000); // 1초 간격
    }

    g_filesystem->close_file(file);
    safe_log("파일시스템 작업자 스레드 %d 종료", worker_id);
    return NULL;
}

/**
 * @brief 네트워크 테스트 스레드
 */
void* network_test_thread(void* arg) {
    safe_log("네트워크 테스트 스레드 시작");

    // 간단한 TCP 서버 생성
    ETSocket* server_socket;
    ETResult result = g_network->create_socket(ET_SOCKET_TCP, &server_socket);
    if (result != ET_SUCCESS) {
        safe_log("서버 소켓 생성 실패: %d", result);
        return NULL;
    }

    // 로컬 주소에 바인딩
    ETSocketAddress addr;
    memset(&addr, 0, sizeof(addr));
    addr.family = ET_ADDRESS_FAMILY_IPV4;
    addr.port = 12345;
    strcpy(addr.address, "127.0.0.1");

    result = g_network->bind_socket(server_socket, &addr);
    if (result != ET_SUCCESS) {
        safe_log("소켓 바인딩 실패: %d", result);
        g_network->close_socket(server_socket);
        return NULL;
    }

    result = g_network->listen_socket(server_socket, 5);
    if (result != ET_SUCCESS) {
        safe_log("소켓 리스닝 실패: %d", result);
        g_network->close_socket(server_socket);
        return NULL;
    }

    safe_log("TCP 서버가 포트 12345에서 대기 중");

    // 클라이언트 연결 대기 (논블로킹으로 짧은 시간만)
    int wait_count = 0;
    while (g_running && wait_count < 10) {
        ETSocket* client_socket;
        ETSocketAddress client_addr;

        result = g_network->accept_socket(server_socket, &client_socket, &client_addr);
        if (result == ET_SUCCESS) {
            safe_log("클라이언트 연결됨: %s:%d", client_addr.address, client_addr.port);

            // 간단한 메시지 전송
            const char* message = "Hello from LibEtude cross-platform demo!\n";
            size_t sent;
            g_network->send_data(client_socket, message, strlen(message), &sent);

            g_network->close_socket(client_socket);
            safe_log("클라이언트 연결 종료");
        }

        g_system->sleep(1000); // 1초 대기
        wait_count++;
    }

    g_network->close_socket(server_socket);
    safe_log("네트워크 테스트 스레드 종료");
    return NULL;
}

/**
 * @brief 오디오 콜백 함수
 */
void audio_callback(void* output_buffer, int frame_count, void* user_data) {
    static double phase = 0.0;
    float* buffer = (float*)output_buffer;

    // 간단한 사인파 생성 (440Hz)
    for (int i = 0; i < frame_count * 2; i += 2) {
        float sample = 0.1f * sinf(phase); // 낮은 볼륨
        buffer[i] = sample;     // 좌채널
        buffer[i + 1] = sample; // 우채널

        phase += 2.0 * M_PI * 440.0 / 44100.0;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
}

/**
 * @brief 오디오 시스템 초기화
 */
ETResult initialize_audio(void) {
    safe_log("오디오 시스템 초기화");

    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 32,
        .format = ET_AUDIO_FORMAT_FLOAT32
    };

    ETResult result = g_audio->open_output_device(NULL, &format, &g_audio_device);
    if (result != ET_SUCCESS) {
        safe_log("오디오 디바이스 열기 실패: %d", result);
        return result;
    }

    result = g_audio->set_callback(g_audio_device, audio_callback, NULL);
    if (result != ET_SUCCESS) {
        safe_log("오디오 콜백 설정 실패: %d", result);
        return result;
    }

    result = g_audio->start_stream(g_audio_device);
    if (result != ET_SUCCESS) {
        safe_log("오디오 스트림 시작 실패: %d", result);
        return result;
    }

    safe_log("오디오 시스템 초기화 완료");
    return ET_SUCCESS;
}

/**
 * @brief 모든 작업 스레드 시작
 */
ETResult start_worker_threads(void) {
    safe_log("작업 스레드들 시작");

    // 메모리 모니터링 스레드
    ETResult result = g_thread->create_thread(&g_worker_threads[0],
                                             memory_monitor_thread, NULL);
    if (result != ET_SUCCESS) {
        safe_log("메모리 모니터링 스레드 생성 실패: %d", result);
        return result;
    }

    // CPU 모니터링 스레드
    result = g_thread->create_thread(&g_worker_threads[1],
                                    cpu_monitor_thread, NULL);
    if (result != ET_SUCCESS) {
        safe_log("CPU 모니터링 스레드 생성 실패: %d", result);
        return result;
    }

    // 파일시스템 작업 스레드
    static int worker_id = 2;
    result = g_thread->create_thread(&g_worker_threads[2],
                                    filesystem_worker_thread, &worker_id);
    if (result != ET_SUCCESS) {
        safe_log("파일시스템 작업 스레드 생성 실패: %d", result);
        return result;
    }

    // 네트워크 테스트 스레드
    result = g_thread->create_thread(&g_worker_threads[3],
                                    network_test_thread, NULL);
    if (result != ET_SUCCESS) {
        safe_log("네트워크 테스트 스레드 생성 실패: %d", result);
        return result;
    }

    safe_log("모든 작업 스레드 시작 완료");
    return ET_SUCCESS;
}

/**
 * @brief 로그 저장
 */
void save_logs(void) {
    safe_log("로그 저장 시작");

    ETFile* file;
    ETResult result = g_filesystem->open_file("demo_log.txt",
                                             ET_FILE_MODE_WRITE, &file);
    if (result != ET_SUCCESS) {
        printf("로그 파일 열기 실패: %d\n", result);
        return;
    }

    char buffer[512];
    size_t written;

    for (int i = 0; i < g_log_count; i++) {
        snprintf(buffer, sizeof(buffer), "[%llu] [스레드 %d] %s\n",
                g_log_buffer[i].timestamp,
                g_log_buffer[i].thread_id,
                g_log_buffer[i].message);

        g_filesystem->write_file(file, buffer, strlen(buffer), &written);
    }

    g_filesystem->close_file(file);
    printf("로그를 demo_log.txt에 저장 완료 (%d개 항목)\n", g_log_count);
}

/**
 * @brief 리소스 정리
 */
void cleanup_resources(void) {
    printf("리소스 정리 중...\n");

    // 오디오 정리
    if (g_audio_device) {
        g_audio->stop_stream(g_audio_device);
        g_audio->close_device(g_audio_device);
        g_audio_device = NULL;
    }

    // 스레드 정리
    for (int i = 0; i < 4; i++) {
        if (g_worker_threads[i]) {
            void* result;
            g_thread->join_thread(g_worker_threads[i], &result);
            g_thread->destroy_thread(g_worker_threads[i]);
            g_worker_threads[i] = NULL;
        }
    }

    // 뮤텍스 정리
    if (g_log_mutex) {
        g_thread->destroy_mutex(g_log_mutex);
        g_log_mutex = NULL;
    }

    // 로그 저장
    save_logs();

    printf("리소스 정리 완료\n");
}

int main(void) {
    printf("=== LibEtude 크로스 플랫폼 데모 애플리케이션 ===\n\n");

    // 신호 핸들러 설정
    signal(SIGINT, signal_handler);

    // 플랫폼 초기화
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        printf("플랫폼 초기화 실패: %d\n", result);
        return 1;
    }

    // 모든 인터페이스 획득
    g_audio = et_platform_get_audio_interface();
    g_system = et_platform_get_system_interface();
    g_thread = et_platform_get_thread_interface();
    g_memory = et_platform_get_memory_interface();
    g_filesystem = et_platform_get_filesystem_interface();
    g_network = et_platform_get_network_interface();

    if (!g_audio || !g_system || !g_thread || !g_memory || !g_filesystem || !g_network) {
        printf("인터페이스 획득 실패\n");
        et_platform_cleanup();
        return 1;
    }

    // 로그 뮤텍스 생성
    result = g_thread->create_mutex(&g_log_mutex);
    if (result != ET_SUCCESS) {
        printf("로그 뮤텍스 생성 실패: %d\n", result);
        et_platform_cleanup();
        return 1;
    }

    safe_log("=== 크로스 플랫폼 데모 시작 ===");

    // 시스템 정보 수집
    collect_system_info();

    // 오디오 시스템 초기화
    initialize_audio();

    // 작업 스레드들 시작
    start_worker_threads();

    safe_log("모든 시스템 초기화 완료");
    safe_log("데모가 실행 중입니다... (Ctrl+C로 종료)");

    // 메인 루프
    int main_loop_count = 0;
    while (g_running && main_loop_count < 30) { // 최대 30초
        // 메인 스레드에서 주기적 작업
        safe_log("메인 루프 반복 %d", main_loop_count);

        // 시스템 상태 체크
        ETSystemInfo info;
        if (g_system->get_system_info(&info) == ET_SUCCESS) {
            safe_log("시스템 상태: CPU %u코어, 메모리 %llu MB",
                    info.cpu_count, info.available_memory / (1024 * 1024));
        }

        g_system->sleep(1000); // 1초 대기
        main_loop_count++;
    }

    safe_log("=== 크로스 플랫폼 데모 종료 ===");

    // 리소스 정리
    cleanup_resources();

    // 플랫폼 정리
    et_platform_cleanup();

    printf("\n데모 애플리케이션 완료!\n");
    printf("생성된 파일들:\n");
    printf("  - system_info.txt: 시스템 정보\n");
    printf("  - worker_2_output.txt: 파일시스템 작업 결과\n");
    printf("  - demo_log.txt: 전체 로그\n");

    return 0;
}