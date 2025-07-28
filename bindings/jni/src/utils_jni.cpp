/**
 * @file utils_jni.cpp
 * @brief LibEtude 유틸리티 JNI 바인딩 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude_jni.h"

extern "C" {

/**
 * @brief LibEtude 버전 가져오기
 * Java 시그니처: native String getVersion();
 */
JNIEXPORT jstring JNICALL
Java_com_libetude_Engine_getVersion(JNIEnv* env, jclass clazz) {
    const char* version = libetude_get_version();
    return create_jstring(env, version);
}

/**
 * @brief 하드웨어 기능 가져오기
 * Java 시그니처: native int getHardwareFeatures();
 */
JNIEXPORT jint JNICALL
Java_com_libetude_Engine_getHardwareFeatures(JNIEnv* env, jclass clazz) {
    uint32_t features = libetude_get_hardware_features();
    return static_cast<jint>(features);
}

/**
 * @brief 마지막 오류 메시지 가져오기
 * Java 시그니처: native String getLastError();
 */
JNIEXPORT jstring JNICALL
Java_com_libetude_Engine_getLastError(JNIEnv* env, jclass clazz) {
    const char* error = libetude_get_last_error();
    return create_jstring(env, error);
}

/**
 * @brief 로그 레벨 설정
 * Java 시그니처: native void setLogLevel(int level);
 */
JNIEXPORT void JNICALL
Java_com_libetude_Engine_setLogLevel(JNIEnv* env, jclass clazz, jint level) {
    if (level < 0 || level > 4) {
        throw_illegal_argument_exception(env, "유효하지 않은 로그 레벨");
        return;
    }

    libetude_set_log_level(static_cast<LibEtudeLogLevel>(level));
    LOGI("로그 레벨 설정: %d", level);
}

/**
 * @brief 현재 로그 레벨 가져오기
 * Java 시그니처: native int getLogLevel();
 */
JNIEXPORT jint JNICALL
Java_com_libetude_Engine_getLogLevel(JNIEnv* env, jclass clazz) {
    LibEtudeLogLevel level = libetude_get_log_level();
    return static_cast<jint>(level);
}

/**
 * @brief 메모리 사용량 통계 가져오기
 * Java 시그니처: native long[] getMemoryStats();
 */
JNIEXPORT jlongArray JNICALL
Java_com_libetude_Engine_getMemoryStats(JNIEnv* env, jclass clazz) {
    size_t used, peak;
    jni_memory_stats(&used, &peak);

    jlongArray result = env->NewLongArray(2);
    if (!result) {
        throw_runtime_exception(env, "메모리 통계 배열 생성 실패");
        return nullptr;
    }

    jlong stats[2] = {static_cast<jlong>(used), static_cast<jlong>(peak)};
    env->SetLongArrayRegion(result, 0, 2, stats);

    return result;
}

/**
 * @brief 시스템 정보 가져오기
 * Java 시그니처: native String getSystemInfo();
 */
JNIEXPORT jstring JNICALL
Java_com_libetude_Engine_getSystemInfo(JNIEnv* env, jclass clazz) {
    // 시스템 정보 수집
    char info_buffer[1024];
    snprintf(info_buffer, sizeof(info_buffer),
        "LibEtude %s\n"
        "Platform: Android\n"
        "Hardware Features: 0x%08X\n"
        "Build: %s %s\n",
        libetude_get_version(),
        libetude_get_hardware_features(),
        __DATE__, __TIME__
    );

    return create_jstring(env, info_buffer);
}

/**
 * @brief 모바일 최적화 설정 적용
 * Java 시그니처: native boolean applyMobileOptimizations(long engineHandle, boolean lowMemory, boolean batteryOptimized, int maxThreads);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_applyMobileOptimizations(JNIEnv* env, jclass clazz, jlong engine_handle,
                                                  jboolean low_memory, jboolean battery_optimized, jint max_threads) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (max_threads < 1 || max_threads > 8) {
        throw_illegal_argument_exception(env, "유효하지 않은 스레드 수 (1-8)");
        return JNI_FALSE;
    }

    MobileOptimizationConfig config = {
        .low_memory_mode = (low_memory == JNI_TRUE),
        .battery_optimized = (battery_optimized == JNI_TRUE),
        .max_threads = max_threads,
        .memory_limit = 128 * 1024 * 1024  // 128MB 기본값
    };

    apply_mobile_optimizations(engine, &config);

    LOGI("모바일 최적화 적용 완료: 저메모리=%d, 배터리최적화=%d, 최대스레드=%d",
         config.low_memory_mode, config.battery_optimized, config.max_threads);

    return JNI_TRUE;
}

/**
 * @brief 리소스 모니터링 시작
 * Java 시그니처: native void startResourceMonitoring(long engineHandle);
 */
JNIEXPORT void JNICALL
Java_com_libetude_Engine_startResourceMonitoring(JNIEnv* env, jclass clazz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return;
    }

    LOGI("리소스 모니터링 시작");
    monitor_mobile_resources(engine);
}

/**
 * @brief 네이티브 라이브러리 초기화 (선택적)
 * Java 시그니처: native boolean initializeNativeLibrary();
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_initializeNativeLibrary(JNIEnv* env, jclass clazz) {
    LOGI("네이티브 라이브러리 초기화");

    // 여기서 필요한 초기화 작업 수행
    // 예: 전역 리소스 할당, 하드웨어 감지 등

    uint32_t hw_features = libetude_get_hardware_features();
    LOGI("감지된 하드웨어 기능: 0x%08X", hw_features);

    return JNI_TRUE;
}

/**
 * @brief 네이티브 라이브러리 정리 (선택적)
 * Java 시그니처: native void cleanupNativeLibrary();
 */
JNIEXPORT void JNICALL
Java_com_libetude_Engine_cleanupNativeLibrary(JNIEnv* env, jclass clazz) {
    LOGI("네이티브 라이브러리 정리");

    // 여기서 필요한 정리 작업 수행
    // 예: 전역 리소스 해제, 스레드 정리 등

    size_t used, peak;
    jni_memory_stats(&used, &peak);
    LOGI("최종 메모리 통계 - 사용: %zu bytes, 피크: %zu bytes", used, peak);
}

} // extern "C"