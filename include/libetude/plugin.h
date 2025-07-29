#ifndef LIBETUDE_PLUGIN_H
#define LIBETUDE_PLUGIN_H

#include "types.h"
#include "error.h"
#include "tensor.h"
#include "audio_io.h"

// 플러그인에서 사용할 오류 코드 타입
typedef ETErrorCode PluginError;

#ifdef __cplusplus
extern "C" {
#endif

// 플러그인 API 버전
#define LIBETUDE_PLUGIN_API_VERSION_MAJOR 1
#define LIBETUDE_PLUGIN_API_VERSION_MINOR 0
#define LIBETUDE_PLUGIN_API_VERSION_PATCH 0

// 플러그인 타입 정의
typedef enum {
    PLUGIN_TYPE_AUDIO_EFFECT = 0,    // 오디오 효과 플러그인
    PLUGIN_TYPE_VOCODER = 1,         // 보코더 플러그인
    PLUGIN_TYPE_PREPROCESSOR = 2,    // 전처리 플러그인
    PLUGIN_TYPE_POSTPROCESSOR = 3,   // 후처리 플러그인
    PLUGIN_TYPE_EXTENSION = 4,       // 확장 모델 플러그인
    PLUGIN_TYPE_CUSTOM = 255         // 사용자 정의 플러그인
} PluginType;

// 플러그인 상태
typedef enum {
    PLUGIN_STATE_UNLOADED = 0,       // 로드되지 않음
    PLUGIN_STATE_LOADED = 1,         // 로드됨
    PLUGIN_STATE_INITIALIZED = 2,    // 초기화됨
    PLUGIN_STATE_ACTIVE = 3,         // 활성화됨
    PLUGIN_STATE_ERROR = 4           // 오류 상태
} PluginState;

// 플러그인 버전 정보
typedef struct {
    uint16_t major;                  // 주 버전
    uint16_t minor;                  // 부 버전
    uint16_t patch;                  // 패치 버전
    uint16_t build;                  // 빌드 번호
} PluginVersion;

// 플러그인 메타데이터
typedef struct {
    char name[64];                   // 플러그인 이름
    char description[256];           // 플러그인 설명
    char author[64];                 // 제작자
    char vendor[64];                 // 벤더
    PluginVersion version;           // 플러그인 버전
    PluginVersion api_version;       // 요구되는 API 버전
    PluginType type;                 // 플러그인 타입
    uint32_t flags;                  // 플러그인 플래그
    char uuid[37];                   // 고유 식별자 (UUID)
    uint32_t checksum;               // 체크섬
} PluginMetadata;

// 플러그인 의존성 정보
typedef struct {
    char name[64];                   // 의존성 이름
    PluginVersion min_version;       // 최소 버전
    PluginVersion max_version;       // 최대 버전
    bool required;                   // 필수 여부
} PluginDependency;

// 플러그인 파라미터 타입
typedef enum {
    PARAM_TYPE_FLOAT = 0,            // 실수형
    PARAM_TYPE_INT = 1,              // 정수형
    PARAM_TYPE_BOOL = 2,             // 불린형
    PARAM_TYPE_STRING = 3,           // 문자열
    PARAM_TYPE_ENUM = 4              // 열거형
} PluginParamType;

// 플러그인 파라미터 정의
typedef struct {
    char name[32];                   // 파라미터 이름
    char display_name[64];           // 표시 이름
    char description[128];           // 설명
    PluginParamType type;            // 파라미터 타입

    union {
        struct {
            float min_value;         // 최소값
            float max_value;         // 최대값
            float default_value;     // 기본값
            float step;              // 스텝 크기
        } float_param;

        struct {
            int min_value;           // 최소값
            int max_value;           // 최대값
            int default_value;       // 기본값
            int step;                // 스텝 크기
        } int_param;

        struct {
            bool default_value;      // 기본값
        } bool_param;

        struct {
            char default_value[128]; // 기본값
            int max_length;          // 최대 길이
        } string_param;

        struct {
            char** options;          // 옵션 배열
            int num_options;         // 옵션 수
            int default_index;       // 기본 인덱스
        } enum_param;
    } value;
} PluginParameter;

// 플러그인 파라미터 값
typedef union {
    float float_value;
    int int_value;
    bool bool_value;
    char* string_value;
    int enum_index;
} PluginParamValue;

// 플러그인 컨텍스트
typedef struct PluginContext {
    void* user_data;                 // 사용자 데이터
    struct PluginInstance* plugin;  // 플러그인 인스턴스 참조
    void* internal_state;            // 내부 상태
    size_t state_size;               // 상태 크기
} PluginContext;

// 플러그인 인스턴스
typedef struct {
    PluginMetadata metadata;         // 메타데이터
    PluginState state;               // 현재 상태
    void* handle;                    // 동적 라이브러리 핸들
    PluginContext* context;          // 플러그인 컨텍스트

    // 의존성 정보
    PluginDependency* dependencies;  // 의존성 배열
    int num_dependencies;            // 의존성 수

    // 파라미터 정보
    PluginParameter* parameters;     // 파라미터 배열
    int num_parameters;              // 파라미터 수
    PluginParamValue* param_values;  // 현재 파라미터 값

    // 플러그인 함수 포인터
    struct {
        // 필수 함수들
        PluginError (*initialize)(PluginContext* ctx, const void* config);
        PluginError (*process)(PluginContext* ctx, const float* input, float* output, int num_samples);
        PluginError (*finalize)(PluginContext* ctx);

        // 선택적 함수들
        PluginError (*set_parameter)(PluginContext* ctx, int param_id, PluginParamValue value);
        PluginError (*get_parameter)(PluginContext* ctx, int param_id, PluginParamValue* value);
        PluginError (*reset)(PluginContext* ctx);
        PluginError (*suspend)(PluginContext* ctx);
        PluginError (*resume)(PluginContext* ctx);

        // 정보 조회 함수들
        const char* (*get_info)(PluginContext* ctx, const char* key);
        PluginError (*get_latency)(PluginContext* ctx, int* latency_samples);
        PluginError (*get_tail_time)(PluginContext* ctx, float* tail_time_seconds);
    } functions;
} PluginInstance;

// 플러그인 레지스트리
typedef struct {
    PluginInstance** plugins;        // 플러그인 배열
    int num_plugins;                 // 플러그인 수
    int capacity;                    // 용량
    char** search_paths;             // 검색 경로 배열
    int num_search_paths;            // 검색 경로 수
} PluginRegistry;

// 플러그인 로더 콜백
typedef PluginError (*PluginLoadCallback)(const char* path, PluginInstance** plugin);
typedef void (*PluginUnloadCallback)(PluginInstance* plugin);

// 플러그인 이벤트 콜백
typedef void (*PluginEventCallback)(PluginInstance* plugin, const char* event, void* data);

// 플러그인 레지스트리 함수
PluginRegistry* plugin_create_registry(void);
void plugin_destroy_registry(PluginRegistry* registry);

// 검색 경로 관리
PluginError plugin_add_search_path(PluginRegistry* registry, const char* path);
PluginError plugin_remove_search_path(PluginRegistry* registry, const char* path);
void plugin_clear_search_paths(PluginRegistry* registry);

// 플러그인 검색 및 로딩
PluginError plugin_scan_directory(PluginRegistry* registry, const char* directory);
PluginError plugin_load_from_file(PluginRegistry* registry, const char* path, PluginInstance** plugin);
PluginError plugin_load_by_name(PluginRegistry* registry, const char* name, PluginInstance** plugin);
PluginError plugin_unload(PluginRegistry* registry, PluginInstance* plugin);

// 플러그인 관리
PluginError plugin_register(PluginRegistry* registry, PluginInstance* plugin);
PluginError plugin_unregister(PluginRegistry* registry, const char* name);
PluginInstance* plugin_find_by_name(PluginRegistry* registry, const char* name);
PluginInstance* plugin_find_by_uuid(PluginRegistry* registry, const char* uuid);

// 플러그인 인스턴스 관리
PluginError plugin_initialize(PluginInstance* plugin, const void* config);
PluginError plugin_finalize(PluginInstance* plugin);
PluginError plugin_process(PluginInstance* plugin, const float* input, float* output, int num_samples);

// 파라미터 관리
PluginError plugin_set_parameter(PluginInstance* plugin, const char* name, PluginParamValue value);
PluginError plugin_get_parameter(PluginInstance* plugin, const char* name, PluginParamValue* value);
PluginError plugin_set_parameter_by_id(PluginInstance* plugin, int param_id, PluginParamValue value);
PluginError plugin_get_parameter_by_id(PluginInstance* plugin, int param_id, PluginParamValue* value);
PluginError plugin_reset_parameters(PluginInstance* plugin);

// 상태 관리
PluginError plugin_activate(PluginInstance* plugin);
PluginError plugin_deactivate(PluginInstance* plugin);
PluginError plugin_suspend(PluginInstance* plugin);
PluginError plugin_resume(PluginInstance* plugin);
PluginState plugin_get_state(PluginInstance* plugin);

// 의존성 검증
PluginError plugin_check_dependencies(PluginInstance* plugin, PluginRegistry* registry);
PluginError plugin_resolve_dependencies(PluginInstance* plugin, PluginRegistry* registry);

// 버전 호환성 검증
bool plugin_is_version_compatible(const PluginVersion* required, const PluginVersion* available);
bool plugin_is_api_compatible(const PluginVersion* plugin_api, const PluginVersion* engine_api);

// 플러그인 정보 조회
const PluginMetadata* plugin_get_metadata(PluginInstance* plugin);
const PluginParameter* plugin_get_parameters(PluginInstance* plugin, int* num_params);
const PluginDependency* plugin_get_dependencies(PluginInstance* plugin, int* num_deps);

// 콜백 설정
void plugin_set_load_callback(PluginLoadCallback callback);
void plugin_set_unload_callback(PluginUnloadCallback callback);
void plugin_set_event_callback(PluginEventCallback callback);

// 유틸리티 함수
PluginError plugin_validate_metadata(const PluginMetadata* metadata);
uint32_t plugin_calculate_checksum(const void* data, size_t size);
PluginError plugin_generate_uuid(char* uuid_str, size_t buffer_size);

// 플러그인 체인 관리 (여러 플러그인을 연결하여 처리)
typedef struct PluginChain PluginChain;

PluginChain* plugin_create_chain(void);
void plugin_destroy_chain(PluginChain* chain);
PluginError plugin_chain_add(PluginChain* chain, PluginInstance* plugin);
PluginError plugin_chain_remove(PluginChain* chain, PluginInstance* plugin);
PluginError plugin_chain_process(PluginChain* chain, const float* input, float* output, int num_samples);
PluginError plugin_chain_set_bypass(PluginChain* chain, PluginInstance* plugin, bool bypass);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLUGIN_H