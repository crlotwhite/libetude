/**
 * @file engine_jni.cpp
 * @brief LibEtude 엔진 JNI 바인딩 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude_jni.h"
#include <vector>
#include <memory>

// Java 클래스 경로
static const char* ENGINE_CLASS = "com/libetude/Engine";
static const char* PERFORMANCE_STATS_CLASS = "com/libetude/PerformanceStats";

// 엔진 핸들을 Java long으로 변환하는 유틸리티
static inline jlong engine_to_jlong(LibEtudeEngine* engine) {
    return reinterpret_cast<jlong>(engine);
}

static inline LibEtudeEngine* jlong_to_engine(jlong handle) {
    return reinterpret_cast<LibEtudeEngine*>(handle);
}

extern "C" {

/**
 * @brief 엔진 생성
 * Java 시그니처: native long createEngine(String modelPath);
 */
JNIEXPORT jlong JNICALL
Java_com_libetude_Engine_createEngine(JNIEnv* env, jobject thiz, jstring model_path) {
    if (!model_path) {
        throw_illegal_argument_exception(env, "모델 경로가 null입니다");
        return 0;
    }

    char* path = get_cstring(env, model_path);
    if (!path) {
        throw_runtime_exception(env, "모델 경로 문자열 변환 실패");
        return 0;
    }

    LOGI("엔진 생성 시작: %s", path);

    LibEtudeEngine* engine = libetude_create_engine(path);
    release_cstring(env, model_path, path);

    if (!engine) {
        const char* error = libetude_get_last_error();
        LOGE("엔진 생성 실패: %s", error ? error : "알 수 없는 오류");
        throw_runtime_exception(env, error ? error : "엔진 생성 실패");
        return 0;
    }

    // 모바일 최적화 적용
    MobileOptimizationConfig config = {
        .low_memory_mode = true,
        .battery_optimized = true,
        .max_threads = 2,  // 모바일에서는 스레드 수 제한
        .memory_limit = 128 * 1024 * 1024  // 128MB
    };
    apply_mobile_optimizations(engine, &config);

    LOGI("엔진 생성 완료: %p", engine);
    return engine_to_jlong(engine);
}

/**
 * @brief 엔진 해제
 * Java 시그니처: native void destroyEngine(long engineHandle);
 */
JNIEXPORT void JNICALL
Java_com_libetude_Engine_destroyEngine(JNIEnv* env, jobject thiz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        LOGW("null 엔진 핸들로 destroyEngine 호출됨");
        return;
    }

    LOGI("엔진 해제: %p", engine);
    libetude_destroy_engine(engine);
}

/**
 * @brief 텍스트 음성 합성
 * Java 시그니처: native float[] synthesizeText(long engineHandle, String text);
 */
JNIEXPORT jfloatArray JNICALL
Java_com_libetude_Engine_synthesizeText(JNIEnv* env, jobject thiz, jlong engine_handle, jstring text) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return nullptr;
    }

    if (!text) {
        throw_illegal_argument_exception(env, "텍스트가 null입니다");
        return nullptr;
    }

    char* text_str = get_cstring(env, text);
    if (!text_str) {
        throw_runtime_exception(env, "텍스트 문자열 변환 실패");
        return nullptr;
    }

    LOGI("텍스트 합성 시작: %s", text_str);

    // 모바일 환경을 고려한 버퍼 크기 (최대 10초)
    const int max_samples = 48000 * 10;  // 48kHz * 10초
    std::vector<float> audio_buffer(max_samples);
    int output_length = max_samples;

    int result = libetude_synthesize_text(engine, text_str, audio_buffer.data(), &output_length);
    release_cstring(env, text, text_str);

    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("텍스트 합성 실패: %s", error ? error : "알 수 없는 오류");
        throw_runtime_exception(env, error ? error : "텍스트 합성 실패");
        return nullptr;
    }

    // Java float 배열 생성
    jfloatArray result_array = env->NewFloatArray(output_length);
    if (!result_array) {
        throw_runtime_exception(env, "결과 배열 생성 실패");
        return nullptr;
    }

    env->SetFloatArrayRegion(result_array, 0, output_length, audio_buffer.data());

    LOGI("텍스트 합성 완료: %d 샘플", output_length);

    // 리소스 모니터링
    monitor_mobile_resources(engine);

    return result_array;
}

/**
 * @brief 노래 합성
 * Java 시그니처: native float[] synthesizeSinging(long engineHandle, String lyrics, float[] notes);
 */
JNIEXPORT jfloatArray JNICALL
Java_com_libetude_Engine_synthesizeSinging(JNIEnv* env, jobject thiz, jlong engine_handle,
                                           jstring lyrics, jfloatArray notes) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return nullptr;
    }

    if (!lyrics || !notes) {
        throw_illegal_argument_exception(env, "가사 또는 음표가 null입니다");
        return nullptr;
    }

    char* lyrics_str = get_cstring(env, lyrics);
    if (!lyrics_str) {
        throw_runtime_exception(env, "가사 문자열 변환 실패");
        return nullptr;
    }

    jsize note_count = env->GetArrayLength(notes);
    jfloat* note_data = env->GetFloatArrayElements(notes, nullptr);
    if (!note_data) {
        release_cstring(env, lyrics, lyrics_str);
        throw_runtime_exception(env, "음표 배열 접근 실패");
        return nullptr;
    }

    LOGI("노래 합성 시작: %s (%d 음표)", lyrics_str, note_count);

    // 노래는 일반적으로 더 긴 오디오를 생성할 수 있음
    const int max_samples = 48000 * 30;  // 48kHz * 30초
    std::vector<float> audio_buffer(max_samples);
    int output_length = max_samples;

    int result = libetude_synthesize_singing(engine, lyrics_str, note_data, note_count,
                                           audio_buffer.data(), &output_length);

    env->ReleaseFloatArrayElements(notes, note_data, JNI_ABORT);
    release_cstring(env, lyrics, lyrics_str);

    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("노래 합성 실패: %s", error ? error : "알 수 없는 오류");
        throw_runtime_exception(env, error ? error : "노래 합성 실패");
        return nullptr;
    }

    // Java float 배열 생성
    jfloatArray result_array = env->NewFloatArray(output_length);
    if (!result_array) {
        throw_runtime_exception(env, "결과 배열 생성 실패");
        return nullptr;
    }

    env->SetFloatArrayRegion(result_array, 0, output_length, audio_buffer.data());

    LOGI("노래 합성 완료: %d 샘플", output_length);

    // 리소스 모니터링
    monitor_mobile_resources(engine);

    return result_array;
}

/**
 * @brief 품질 모드 설정
 * Java 시그니처: native boolean setQualityMode(long engineHandle, int qualityMode);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_setQualityMode(JNIEnv* env, jobject thiz, jlong engine_handle, jint quality_mode) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (quality_mode < 0 || quality_mode > 2) {
        throw_illegal_argument_exception(env, "유효하지 않은 품질 모드");
        return JNI_FALSE;
    }

    int result = libetude_set_quality_mode(engine, static_cast<QualityMode>(quality_mode));
    if (result != LIBETUDE_SUCCESS) {
        LOGE("품질 모드 설정 실패: %d", result);
        return JNI_FALSE;
    }

    LOGI("품질 모드 설정: %d", quality_mode);
    return JNI_TRUE;
}

/**
 * @brief GPU 가속 활성화
 * Java 시그니처: native boolean enableGPUAcceleration(long engineHandle);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_enableGPUAcceleration(JNIEnv* env, jobject thiz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    int result = libetude_enable_gpu_acceleration(engine);
    if (result != LIBETUDE_SUCCESS) {
        LOGW("GPU 가속 활성화 실패 (모바일에서는 정상적일 수 있음): %d", result);
        return JNI_FALSE;
    }

    LOGI("GPU 가속 활성화됨");
    return JNI_TRUE;
}

/**
 * @brief 성능 통계 가져오기
 * Java 시그니처: native PerformanceStats getPerformanceStats(long engineHandle);
 */
JNIEXPORT jobject JNICALL
Java_com_libetude_Engine_getPerformanceStats(JNIEnv* env, jobject thiz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return nullptr;
    }

    PerformanceStats stats;
    int result = libetude_get_performance_stats(engine, &stats);
    if (result != LIBETUDE_SUCCESS) {
        LOGE("성능 통계 가져오기 실패: %d", result);
        throw_runtime_exception(env, "성능 통계 가져오기 실패");
        return nullptr;
    }

    // PerformanceStats Java 객체 생성
    jclass stats_class = env->FindClass(PERFORMANCE_STATS_CLASS);
    if (!stats_class) {
        throw_runtime_exception(env, "PerformanceStats 클래스를 찾을 수 없음");
        return nullptr;
    }

    jmethodID constructor = env->GetMethodID(stats_class, "<init>", "(DDDDI)V");
    if (!constructor) {
        env->DeleteLocalRef(stats_class);
        throw_runtime_exception(env, "PerformanceStats 생성자를 찾을 수 없음");
        return nullptr;
    }

    jobject stats_obj = env->NewObject(stats_class, constructor,
                                      stats.inference_time_ms,
                                      stats.memory_usage_mb,
                                      stats.cpu_usage_percent,
                                      stats.gpu_usage_percent,
                                      stats.active_threads);

    env->DeleteLocalRef(stats_class);

    if (!stats_obj) {
        throw_runtime_exception(env, "PerformanceStats 객체 생성 실패");
        return nullptr;
    }

    return stats_obj;
}

/**
 * @brief 확장 모델 로드
 * Java 시그니처: native boolean loadExtension(long engineHandle, String extensionPath);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_loadExtension(JNIEnv* env, jobject thiz, jlong engine_handle, jstring extension_path) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (!extension_path) {
        throw_illegal_argument_exception(env, "확장 경로가 null입니다");
        return JNI_FALSE;
    }

    char* path = get_cstring(env, extension_path);
    if (!path) {
        throw_runtime_exception(env, "확장 경로 문자열 변환 실패");
        return JNI_FALSE;
    }

    LOGI("확장 모델 로드: %s", path);

    int result = libetude_load_extension(engine, path);
    release_cstring(env, extension_path, path);

    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("확장 모델 로드 실패: %s", error ? error : "알 수 없는 오류");
        return JNI_FALSE;
    }

    LOGI("확장 모델 로드 완료");
    return JNI_TRUE;
}

} // extern "C"