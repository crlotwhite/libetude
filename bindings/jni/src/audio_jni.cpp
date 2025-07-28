/**
 * @file audio_jni.cpp
 * @brief LibEtude 오디오 스트리밍 JNI 바인딩 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude_jni.h"
#include <memory>
#include <mutex>
#include <atomic>

// 스트리밍 콜백 데이터 구조체
struct StreamingCallbackData {
    JavaVM* jvm;
    jobject callback_obj;  // Java 콜백 객체의 글로벌 참조
    jmethodID callback_method;
    std::atomic<bool> active{false};
    std::mutex mutex;
};

// 전역 스트리밍 데이터 (엔진당 하나씩 관리해야 하지만 단순화)
static std::unique_ptr<StreamingCallbackData> g_streaming_data;

// C 콜백 함수 (LibEtude에서 호출됨)
static void native_audio_callback(const float* audio, int length, void* user_data) {
    StreamingCallbackData* data = static_cast<StreamingCallbackData*>(user_data);
    if (!data || !data->active.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(data->mutex);

    JNIEnv* env = nullptr;
    int attach_result = data->jvm->AttachCurrentThread(&env, nullptr);
    if (attach_result != JNI_OK || !env) {
        LOGE("JNI 환경 연결 실패: %d", attach_result);
        return;
    }

    // float 배열을 Java로 전달
    jfloatArray audio_array = env->NewFloatArray(length);
    if (audio_array) {
        env->SetFloatArrayRegion(audio_array, 0, length, audio);

        // Java 콜백 메서드 호출
        env->CallVoidMethod(data->callback_obj, data->callback_method, audio_array);

        // 예외 확인
        if (env->ExceptionCheck()) {
            LOGE("Java 콜백에서 예외 발생");
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        env->DeleteLocalRef(audio_array);
    } else {
        LOGE("오디오 배열 생성 실패");
    }

    data->jvm->DetachCurrentThread();
}

extern "C" {

/**
 * @brief 스트리밍 시작
 * Java 시그니처: native boolean startStreaming(long engineHandle, AudioStreamCallback callback);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_startStreaming(JNIEnv* env, jobject thiz, jlong engine_handle, jobject callback) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (!callback) {
        throw_illegal_argument_exception(env, "콜백이 null입니다");
        return JNI_FALSE;
    }

    // 이미 스트리밍 중이면 중지
    if (g_streaming_data && g_streaming_data->active.load()) {
        LOGW("이미 스트리밍 중입니다. 기존 스트리밍을 중지합니다.");
        libetude_stop_streaming(engine);
    }

    // 새로운 스트리밍 데이터 생성
    g_streaming_data = std::make_unique<StreamingCallbackData>();

    // JavaVM 가져오기
    if (env->GetJavaVM(&g_streaming_data->jvm) != JNI_OK) {
        throw_runtime_exception(env, "JavaVM 가져오기 실패");
        g_streaming_data.reset();
        return JNI_FALSE;
    }

    // 콜백 객체의 글로벌 참조 생성
    g_streaming_data->callback_obj = env->NewGlobalRef(callback);
    if (!g_streaming_data->callback_obj) {
        throw_runtime_exception(env, "콜백 객체 글로벌 참조 생성 실패");
        g_streaming_data.reset();
        return JNI_FALSE;
    }

    // 콜백 메서드 찾기
    jclass callback_class = env->GetObjectClass(callback);
    g_streaming_data->callback_method = env->GetMethodID(callback_class, "onAudioData", "([F)V");
    env->DeleteLocalRef(callback_class);

    if (!g_streaming_data->callback_method) {
        throw_runtime_exception(env, "콜백 메서드를 찾을 수 없습니다");
        env->DeleteGlobalRef(g_streaming_data->callback_obj);
        g_streaming_data.reset();
        return JNI_FALSE;
    }

    g_streaming_data->active.store(true);

    LOGI("스트리밍 시작");

    // LibEtude 스트리밍 시작
    int result = libetude_start_streaming(engine, native_audio_callback, g_streaming_data.get());
    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("스트리밍 시작 실패: %s", error ? error : "알 수 없는 오류");

        g_streaming_data->active.store(false);
        env->DeleteGlobalRef(g_streaming_data->callback_obj);
        g_streaming_data.reset();

        throw_runtime_exception(env, error ? error : "스트리밍 시작 실패");
        return JNI_FALSE;
    }

    LOGI("스트리밍 시작 완료");
    return JNI_TRUE;
}

/**
 * @brief 스트리밍 텍스트 추가
 * Java 시그니처: native boolean streamText(long engineHandle, String text);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_streamText(JNIEnv* env, jobject thiz, jlong engine_handle, jstring text) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (!text) {
        throw_illegal_argument_exception(env, "텍스트가 null입니다");
        return JNI_FALSE;
    }

    if (!g_streaming_data || !g_streaming_data->active.load()) {
        throw_illegal_argument_exception(env, "스트리밍이 활성화되지 않았습니다");
        return JNI_FALSE;
    }

    char* text_str = get_cstring(env, text);
    if (!text_str) {
        throw_runtime_exception(env, "텍스트 문자열 변환 실패");
        return JNI_FALSE;
    }

    LOGI("스트리밍 텍스트 추가: %s", text_str);

    int result = libetude_stream_text(engine, text_str);
    release_cstring(env, text, text_str);

    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("스트리밍 텍스트 추가 실패: %s", error ? error : "알 수 없는 오류");
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/**
 * @brief 스트리밍 중지
 * Java 시그니처: native boolean stopStreaming(long engineHandle);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_stopStreaming(JNIEnv* env, jobject thiz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        throw_illegal_argument_exception(env, "유효하지 않은 엔진 핸들");
        return JNI_FALSE;
    }

    if (!g_streaming_data || !g_streaming_data->active.load()) {
        LOGW("스트리밍이 활성화되지 않았습니다");
        return JNI_TRUE;  // 이미 중지된 상태이므로 성공으로 처리
    }

    LOGI("스트리밍 중지");

    // 스트리밍 비활성화
    g_streaming_data->active.store(false);

    // LibEtude 스트리밍 중지
    int result = libetude_stop_streaming(engine);

    // 리소스 정리
    {
        std::lock_guard<std::mutex> lock(g_streaming_data->mutex);
        if (g_streaming_data->callback_obj) {
            env->DeleteGlobalRef(g_streaming_data->callback_obj);
        }
    }

    g_streaming_data.reset();

    if (result != LIBETUDE_SUCCESS) {
        const char* error = libetude_get_last_error();
        LOGE("스트리밍 중지 실패: %s", error ? error : "알 수 없는 오류");
        return JNI_FALSE;
    }

    LOGI("스트리밍 중지 완료");
    return JNI_TRUE;
}

/**
 * @brief 스트리밍 상태 확인
 * Java 시그니처: native boolean isStreaming(long engineHandle);
 */
JNIEXPORT jboolean JNICALL
Java_com_libetude_Engine_isStreaming(JNIEnv* env, jobject thiz, jlong engine_handle) {
    LibEtudeEngine* engine = jlong_to_engine(engine_handle);
    if (!engine) {
        return JNI_FALSE;
    }

    return (g_streaming_data && g_streaming_data->active.load()) ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"