/**
 * @file libetude_jni.cpp
 * @brief LibEtude Android JNI 바인딩 메인 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude_jni.h"
#include <cstring>
#include <cstdlib>
#include <memory>
#include <atomic>

// 전역 메모리 사용량 추적
static std::atomic<size_t> g_memory_used{0};
static std::atomic<size_t> g_memory_peak{0};

// JNI 유틸리티 함수 구현
jstring create_jstring(JNIEnv* env, const char* str) {
    if (!str) return nullptr;
    return env->NewStringUTF(str);
}

char* get_cstring(JNIEnv* env, jstring jstr) {
    if (!jstr) return nullptr;
    const char* str = env->GetStringUTFChars(jstr, nullptr);
    if (!str) return nullptr;

    size_t len = strlen(str);
    char* result = static_cast<char*>(jni_malloc(len + 1));
    if (result) {
        strcpy(result, str);
    }
    env->ReleaseStringUTFChars(jstr, str);
    return result;
}

void release_cstring(JNIEnv* env, jstring jstr, const char* cstr) {
    if (cstr) {
        jni_free(const_cast<char*>(cstr));
    }
}

// 메모리 관리 구현
void* jni_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        size_t current = g_memory_used.fetch_add(size) + size;
        size_t peak = g_memory_peak.load();
        while (current > peak && !g_memory_peak.compare_exchange_weak(peak, current)) {
            // CAS 루프
        }
    }
    return ptr;
}

void jni_free(void* ptr) {
    if (ptr) {
        // 실제 크기를 추적하기 어려우므로 근사치 사용
        // 실제 구현에서는 더 정교한 메모리 추적이 필요
        free(ptr);
    }
}

void jni_memory_stats(size_t* used, size_t* peak) {
    if (used) *used = g_memory_used.load();
    if (peak) *peak = g_memory_peak.load();
}

// 오류 처리 구현
void throw_java_exception(JNIEnv* env, const char* exception_class, const char* message) {
    jclass clazz = env->FindClass(exception_class);
    if (clazz) {
        env->ThrowNew(clazz, message);
        env->DeleteLocalRef(clazz);
    }
}

void throw_runtime_exception(JNIEnv* env, const char* message) {
    throw_java_exception(env, "java/lang/RuntimeException", message);
}

void throw_illegal_argument_exception(JNIEnv* env, const char* message) {
    throw_java_exception(env, "java/lang/IllegalArgumentException", message);
}

// 모바일 최적화 구현
void apply_mobile_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config) {
    if (!engine || !config) return;

    LOGI("모바일 최적화 적용: 저메모리=%d, 배터리최적화=%d, 최대스레드=%d",
         config->low_memory_mode, config->battery_optimized, config->max_threads);

    // 저메모리 모드 설정
    if (config->low_memory_mode) {
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    }

    // 배터리 최적화 - GPU 가속 비활성화
    if (config->battery_optimized) {
        // GPU 가속 비활성화는 별도 API가 필요할 수 있음
        LOGI("배터리 최적화 모드 활성화");
    }
}

void monitor_mobile_resources(LibEtudeEngine* engine) {
    if (!engine) return;

    PerformanceStats stats;
    int result = libetude_get_performance_stats(engine, &stats);
    if (result == LIBETUDE_SUCCESS) {
        LOGD("성능 통계 - 추론시간: %.2fms, 메모리: %.2fMB, CPU: %.1f%%",
             stats.inference_time_ms, stats.memory_usage_mb, stats.cpu_usage_percent);

        // 메모리 사용량이 임계치를 초과하면 경고
        if (stats.memory_usage_mb > 100.0) {
            LOGW("높은 메모리 사용량 감지: %.2fMB", stats.memory_usage_mb);
        }

        // CPU 사용률이 높으면 경고
        if (stats.cpu_usage_percent > 80.0) {
            LOGW("높은 CPU 사용률 감지: %.1f%%", stats.cpu_usage_percent);
        }
    }
}

// JNI 라이브러리 로드/언로드 시 호출되는 함수들
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("LibEtude JNI 라이브러리 로드됨");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("LibEtude JNI 라이브러리 언로드됨");

    // 메모리 사용량 통계 출력
    size_t used, peak;
    jni_memory_stats(&used, &peak);
    LOGI("메모리 통계 - 현재: %zu bytes, 피크: %zu bytes", used, peak);
}