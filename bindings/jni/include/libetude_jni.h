/**
 * @file libetude_jni.h
 * @brief LibEtude Android JNI 바인딩 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Android JNI를 통한 LibEtude 엔진 접근을 위한 헤더 파일입니다.
 */

#ifndef LIBETUDE_JNI_H
#define LIBETUDE_JNI_H

#include <jni.h>
#include <android/log.h>
#include "libetude/api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Android 로그 매크로
#define LOG_TAG "LibEtude"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// JNI 유틸리티 함수들
jstring create_jstring(JNIEnv* env, const char* str);
char* get_cstring(JNIEnv* env, jstring jstr);
void release_cstring(JNIEnv* env, jstring jstr, const char* cstr);

// 메모리 관리 유틸리티
void* jni_malloc(size_t size);
void jni_free(void* ptr);
void jni_memory_stats(size_t* used, size_t* peak);

// 오류 처리 유틸리티
void throw_java_exception(JNIEnv* env, const char* exception_class, const char* message);
void throw_runtime_exception(JNIEnv* env, const char* message);
void throw_illegal_argument_exception(JNIEnv* env, const char* message);

// 모바일 최적화 설정
typedef struct {
    bool low_memory_mode;      // 저메모리 모드
    bool battery_optimized;    // 배터리 최적화
    int max_threads;           // 최대 스레드 수
    size_t memory_limit;       // 메모리 제한 (바이트)
} MobileOptimizationConfig;

// 모바일 최적화 함수들
void apply_mobile_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config);
void monitor_mobile_resources(LibEtudeEngine* engine);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_JNI_H