/**
 * @file system_info_example.c
 * @brief 플랫폼 추상화 레이어 시스템 정보 조회 예제
 *
 * 이 예제는 ETSystemInterface를 사용하여 크로스 플랫폼 시스템 정보를 조회하는 방법을 보여줍니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libetude/platform/factory.h>
#include <libetude/platform/system.h>
#include <libetude/error.h>

static ETSystemInterface* g_system_interface = NULL;

/**
 * @brief 바이트를 사람이 읽기 쉬운 형태로 변환
 */
const char* format_bytes(uint64_t bytes) {
    static char buffer[64];
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0) {
        snprintf(buffer, sizeof(buffer), "%llu %s", bytes, units[unit_index]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
    }

    return buffer;
}

/**
 * @brief 기본 시스템 정보 출력
 */
void display_system_info(void) {
    printf("=== 기본 시스템 정보 ===\n");

    ETSystemInfo info;
    ETResult result = g_system_interface->get_system_info(&info);

    if (result != ET_SUCCESS) {
        printf("시스템 정보 조회 실패: %d\n", result);
        return;
    }

    printf("시스템 이름: %s\n", info.system_name);
    printf("CPU 이름: %s\n", info.cpu_name);
    printf("CPU 코어 수: %u개\n", info.cpu_count);
    printf("CPU 주파수: %u MHz\n", info.cpu_frequency);
    printf("총 메모리: %s\n", format_bytes(info.total_memory));
    printf("사용 가능한 메모리: %s\n", format_bytes(info.available_memory));

    // 메모리 사용률 계산
    double memory_usage = ((double)(info.total_memory - info.available_memory) / info.total_memory) * 100.0;
    printf("메모리 사용률: %.1f%%\n", memory_usage);

    printf("\n");
}

/**
 * @brief 상세 메모리 정보 출력
 */
void display_memory_info(void) {
    printf("=== 상세 메모리 정보 ===\n");

    ETMemoryInfo info;
    ETResult result = g_system_interface->get_memory_info(&info);

    if (result != ET_SUCCESS) {
        printf("메모리 정보 조회 실패: %d\n", result);
        return;
    }

    printf("물리 메모리:\n");
    printf("  총 크기: %s\n", format_bytes(info.physical_total));
    printf("  사용 가능: %s\n", format_bytes(info.physical_available));
    printf("  사용 중: %s\n", format_bytes(info.physical_total - info.physical_available));

    printf("가상 메모리:\n");
    printf("  총 크기: %s\n", format_bytes(info.virtual_total));
    printf("  사용 가능: %s\n", format_bytes(info.virtual_available));
    printf("  사용 중: %s\n", format_bytes(info.virtual_total - info.virtual_available));

    printf("페이지 파일:\n");
    printf("  총 크기: %s\n", format_bytes(info.page_file_total));
    printf("  사용 가능: %s\n", format_bytes(info.page_file_available));

    printf("\n");
}

/**
 * @brief CPU 정보 출력
 */
void display_cpu_info(void) {
    printf("=== CPU 정보 ===\n");

    ETCPUInfo info;
    ETResult result = g_system_interface->get_cpu_info(&info);

    if (result != ET_SUCCESS) {
        printf("CPU 정보 조회 실패: %d\n", result);
        return;
    }

    printf("CPU 모델: %s\n", info.model_name);
    printf("벤더: %s\n", info.vendor);
    printf("아키텍처: %s\n", info.architecture);
    printf("물리 코어: %u개\n", info.physical_cores);
    printf("논리 코어: %u개\n", info.logical_cores);
    printf("기본 주파수: %u MHz\n", info.base_frequency);
    printf("최대 주파수: %u MHz\n", info.max_frequency);
    printf("캐시 크기:\n");
    printf("  L1 데이터: %u KB\n", info.l1_cache_size / 1024);
    printf("  L1 명령어: %u KB\n", info.l1_instruction_cache_size / 1024);
    printf("  L2: %u KB\n", info.l2_cache_size / 1024);
    printf("  L3: %u KB\n", info.l3_cache_size / 1024);

    printf("\n");
}

/**
 * @brief SIMD 기능 지원 여부 출력
 */
void display_simd_features(void) {
    printf("=== SIMD 기능 지원 ===\n");

    uint32_t features = g_system_interface->get_simd_features();

    printf("지원되는 SIMD 기능:\n");

    if (features & ET_SIMD_SSE) printf("  ✓ SSE\n");
    else printf("  ✗ SSE\n");

    if (features & ET_SIMD_SSE2) printf("  ✓ SSE2\n");
    else printf("  ✗ SSE2\n");

    if (features & ET_SIMD_SSE3) printf("  ✓ SSE3\n");
    else printf("  ✗ SSE3\n");

    if (features & ET_SIMD_SSSE3) printf("  ✓ SSSE3\n");
    else printf("  ✗ SSSE3\n");

    if (features & ET_SIMD_SSE4_1) printf("  ✓ SSE4.1\n");
    else printf("  ✗ SSE4.1\n");

    if (features & ET_SIMD_SSE4_2) printf("  ✓ SSE4.2\n");
    else printf("  ✗ SSE4.2\n");

    if (features & ET_SIMD_AVX) printf("  ✓ AVX\n");
    else printf("  ✗ AVX\n");

    if (features & ET_SIMD_AVX2) printf("  ✓ AVX2\n");
    else printf("  ✗ AVX2\n");

    if (features & ET_SIMD_AVX512) printf("  ✓ AVX-512\n");
    else printf("  ✗ AVX-512\n");

    if (features & ET_SIMD_NEON) printf("  ✓ NEON (ARM)\n");
    else printf("  ✗ NEON (ARM)\n");

    printf("\n");
}

/**
 * @brief 하드웨어 기능 지원 여부 확인
 */
void check_hardware_features(void) {
    printf("=== 하드웨어 기능 지원 ===\n");

    const struct {
        ETHardwareFeature feature;
        const char* name;
    } features[] = {
        {ET_FEATURE_HIGH_RES_TIMER, "고해상도 타이머"},
        {ET_FEATURE_HARDWARE_AES, "하드웨어 AES"},
        {ET_FEATURE_RDRAND, "RDRAND 명령어"},
        {ET_FEATURE_RDSEED, "RDSEED 명령어"},
        {ET_FEATURE_TSC, "타임스탬프 카운터"},
        {ET_FEATURE_INVARIANT_TSC, "불변 TSC"},
        {ET_FEATURE_HYPERTHREADING, "하이퍼스레딩"},
        {ET_FEATURE_VIRTUALIZATION, "가상화 지원"},
    };

    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        bool supported = g_system_interface->has_feature(features[i].feature);
        printf("  %s %s\n", supported ? "✓" : "✗", features[i].name);
    }

    printf("\n");
}

/**
 * @brief 고해상도 타이머 테스트
 */
void test_high_resolution_timer(void) {
    printf("=== 고해상도 타이머 테스트 ===\n");

    uint64_t start_time, end_time;

    // 타이머 해상도 측정
    ETResult result = g_system_interface->get_high_resolution_time(&start_time);
    if (result != ET_SUCCESS) {
        printf("고해상도 타이머 조회 실패: %d\n", result);
        return;
    }

    // 짧은 지연
    g_system_interface->sleep(1); // 1ms

    g_system_interface->get_high_resolution_time(&end_time);

    uint64_t elapsed_ns = end_time - start_time;
    double elapsed_ms = elapsed_ns / 1000000.0;

    printf("1ms 지연 측정 결과:\n");
    printf("  실제 경과 시간: %.3f ms\n", elapsed_ms);
    printf("  나노초 단위: %llu ns\n", elapsed_ns);
    printf("  타이머 해상도: ~%.1f ns\n", elapsed_ns > 0 ? (double)elapsed_ns / 1000.0 : 0.0);

    // 여러 번 측정하여 정확도 확인
    printf("\n타이머 정확도 테스트 (10회 측정):\n");
    for (int i = 0; i < 10; i++) {
        g_system_interface->get_high_resolution_time(&start_time);
        g_system_interface->sleep(10); // 10ms
        g_system_interface->get_high_resolution_time(&end_time);

        elapsed_ns = end_time - start_time;
        elapsed_ms = elapsed_ns / 1000000.0;

        printf("  측정 %d: %.3f ms\n", i + 1, elapsed_ms);
    }

    printf("\n");
}

/**
 * @brief 실시간 시스템 모니터링
 */
void real_time_monitoring(void) {
    printf("=== 실시간 시스템 모니터링 ===\n");
    printf("5초간 시스템 상태를 모니터링합니다...\n");
    printf("(Ctrl+C로 중단 가능)\n\n");

    for (int i = 0; i < 50; i++) { // 5초간 0.1초 간격
        // CPU 사용률 조회
        float cpu_usage = 0.0f;
        ETResult result = g_system_interface->get_cpu_usage(&cpu_usage);

        // 메모리 사용량 조회
        ETMemoryUsage memory_usage;
        g_system_interface->get_memory_usage(&memory_usage);

        // 현재 시간 조회
        uint64_t current_time;
        g_system_interface->get_high_resolution_time(&current_time);

        // 진행률 표시
        printf("\r[");
        for (int j = 0; j < 20; j++) {
            if (j < (i * 20 / 50)) printf("=");
            else printf(" ");
        }
        printf("] ");

        if (result == ET_SUCCESS) {
            printf("CPU: %5.1f%% ", cpu_usage);
        } else {
            printf("CPU: N/A   ");
        }

        printf("메모리: %5.1f%% ", memory_usage.usage_percent);
        printf("시간: %llu", current_time / 1000000); // ms 단위

        fflush(stdout);
        g_system_interface->sleep(100); // 0.1초 대기
    }

    printf("\n\n모니터링 완료!\n\n");
}

/**
 * @brief 성능 벤치마크
 */
void performance_benchmark(void) {
    printf("=== 성능 벤치마크 ===\n");

    const int iterations = 1000000;
    uint64_t start_time, end_time;

    // 시스템 정보 조회 성능 측정
    printf("시스템 정보 조회 성능 측정 (%d회)...\n", iterations);

    g_system_interface->get_high_resolution_time(&start_time);

    for (int i = 0; i < iterations; i++) {
        ETSystemInfo info;
        g_system_interface->get_system_info(&info);
    }

    g_system_interface->get_high_resolution_time(&end_time);

    uint64_t elapsed_ns = end_time - start_time;
    double avg_ns = (double)elapsed_ns / iterations;

    printf("결과:\n");
    printf("  총 시간: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("  평균 시간: %.1f ns/호출\n", avg_ns);
    printf("  초당 호출 수: %.0f 호출/초\n", 1000000000.0 / avg_ns);

    // 고해상도 타이머 성능 측정
    printf("\n고해상도 타이머 성능 측정 (%d회)...\n", iterations);

    g_system_interface->get_high_resolution_time(&start_time);

    for (int i = 0; i < iterations; i++) {
        uint64_t dummy_time;
        g_system_interface->get_high_resolution_time(&dummy_time);
    }

    g_system_interface->get_high_resolution_time(&end_time);

    elapsed_ns = end_time - start_time;
    avg_ns = (double)elapsed_ns / iterations;

    printf("결과:\n");
    printf("  총 시간: %.3f ms\n", elapsed_ns / 1000000.0);
    printf("  평균 시간: %.1f ns/호출\n", avg_ns);
    printf("  초당 호출 수: %.0f 호출/초\n", 1000000000.0 / avg_ns);

    printf("\n");
}

int main(void) {
    printf("=== LibEtude 플랫폼 추상화 레이어 시스템 정보 예제 ===\n\n");

    // 플랫폼 초기화
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        printf("플랫폼 초기화 실패: %d\n", result);
        return 1;
    }

    // 시스템 인터페이스 획득
    g_system_interface = et_platform_get_system_interface();
    if (!g_system_interface) {
        printf("시스템 인터페이스 획득 실패\n");
        et_platform_cleanup();
        return 1;
    }

    // 각종 시스템 정보 출력
    display_system_info();
    display_memory_info();
    display_cpu_info();
    display_simd_features();
    check_hardware_features();

    // 고해상도 타이머 테스트
    test_high_resolution_timer();

    // 실시간 모니터링
    real_time_monitoring();

    // 성능 벤치마크
    performance_benchmark();

    // 정리
    printf("플랫폼 정리 중...\n");
    et_platform_cleanup();

    printf("예제 완료!\n");
    return 0;
}