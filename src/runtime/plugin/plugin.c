#include "libetude/plugin.h"
#include "libetude/memory.h"
#include "libetude/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uuid/uuid.h>
#include <dirent.h>
#include <sys/stat.h>

// plugin 컨텍스트 구조체는 plugin.h에서 정의됨

// plugin 체인 구조체
struct PluginChain {
    PluginInstance** plugins;        // plugin array
    bool* bypass_flags;              // bypass 플래그 array
    int num_plugins;                 // plugin 수
    int capacity;                    // 용량
    float* temp_buffer;              // 임시 버퍼
    int buffer_size;                 // 버퍼 크기
};

// 전역 콜백 함수들
static PluginLoadCallback g_load_callback = NULL;
static PluginUnloadCallback g_unload_callback = NULL;
static PluginEventCallback g_event_callback = NULL;

// plugin 레지스트리 생성
PluginRegistry* plugin_create_registry(void) {
    PluginRegistry* registry = (PluginRegistry*)malloc(sizeof(PluginRegistry));
    if (!registry) {
        return NULL;
    }

    registry->plugins = NULL;
    registry->num_plugins = 0;
    registry->capacity = 0;
    registry->search_paths = NULL;
    registry->num_search_paths = 0;

    return registry;
}

// plugin 레지스트리 해제
void plugin_destroy_registry(PluginRegistry* registry) {
    if (!registry) return;

    // 모든 plugin 언로드
    for (int i = 0; i < registry->num_plugins; i++) {
        plugin_unload(registry, registry->plugins[i]);
    }

    // 검색 경로 해제
    for (int i = 0; i < registry->num_search_paths; i++) {
        free(registry->search_paths[i]);
    }

    free(registry->plugins);
    free(registry->search_paths);
    free(registry);
}

// 검색 경로 추가
PluginError plugin_add_search_path(PluginRegistry* registry, const char* path) {
    if (!registry || !path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 중복 검사
    for (int i = 0; i < registry->num_search_paths; i++) {
        if (strcmp(registry->search_paths[i], path) == 0) {
            return ET_SUCCESS; // already exists
        }
    }

    // array 확장
    char** new_paths = (char**)realloc(registry->search_paths,
                                      (registry->num_search_paths + 1) * sizeof(char*));
    if (!new_paths) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    registry->search_paths = new_paths;
    registry->search_paths[registry->num_search_paths] = strdup(path);
    if (!registry->search_paths[registry->num_search_paths]) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    registry->num_search_paths++;
    return ET_SUCCESS;
}

// 검색 경로 제거
PluginError plugin_remove_search_path(PluginRegistry* registry, const char* path) {
    if (!registry || !path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    for (int i = 0; i < registry->num_search_paths; i++) {
        if (strcmp(registry->search_paths[i], path) == 0) {
            free(registry->search_paths[i]);

            // array 압축
            for (int j = i; j < registry->num_search_paths - 1; j++) {
                registry->search_paths[j] = registry->search_paths[j + 1];
            }

            registry->num_search_paths--;
            return ET_SUCCESS;
        }
    }

    return ET_ERROR_INVALID_ARGUMENT; // path not found
}

// 검색 경로 초기화
void plugin_clear_search_paths(PluginRegistry* registry) {
    if (!registry) return;

    for (int i = 0; i < registry->num_search_paths; i++) {
        free(registry->search_paths[i]);
    }

    free(registry->search_paths);
    registry->search_paths = NULL;
    registry->num_search_paths = 0;
}

// plugin 메타데이터 검증
PluginError plugin_validate_metadata(const PluginMetadata* metadata) {
    if (!metadata) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 이름 검증
    if (strlen(metadata->name) == 0 || strlen(metadata->name) >= 64) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // UUID 검증 (간단한 형식 검사)
    if (strlen(metadata->uuid) != 36) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 버전 검증
    if (metadata->version.major == 0 && metadata->version.minor == 0 &&
        metadata->version.patch == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 체크섬 계산
uint32_t plugin_calculate_checksum(const void* data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    // 간단한 CRC32 구현
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;

    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

// UUID 생성
PluginError plugin_generate_uuid(char* uuid_str, size_t buffer_size) {
    if (!uuid_str || buffer_size < 37) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);

    return ET_SUCCESS;
}

// 파일에서 plugin 로드
PluginError plugin_load_from_file(PluginRegistry* registry, const char* path, PluginInstance** plugin) {
    if (!registry || !path || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 동적 라이브러리 로드
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        return ET_ERROR_IO;
    }

    // plugin 메타데이터 함수 찾기
    typedef const PluginMetadata* (*GetMetadataFunc)(void);
    GetMetadataFunc get_metadata = (GetMetadataFunc)dlsym(handle, "plugin_get_metadata");
    if (!get_metadata) {
        dlclose(handle);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메타데이터 가져오기
    const PluginMetadata* metadata = get_metadata();
    if (!metadata) {
        dlclose(handle);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메타데이터 검증
    PluginError error = plugin_validate_metadata(metadata);
    if (error != ET_SUCCESS) {
        dlclose(handle);
        return error;
    }

    // API 버전 호환성 검증
    PluginVersion engine_api = {
        LIBETUDE_PLUGIN_API_VERSION_MAJOR,
        LIBETUDE_PLUGIN_API_VERSION_MINOR,
        LIBETUDE_PLUGIN_API_VERSION_PATCH,
        0
    };

    if (!plugin_is_api_compatible(&metadata->api_version, &engine_api)) {
        dlclose(handle);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // plugin 인스턴스 생성
    PluginInstance* instance = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!instance) {
        dlclose(handle);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(instance, 0, sizeof(PluginInstance));
    instance->metadata = *metadata;
    instance->state = PLUGIN_STATE_LOADED;
    instance->handle = handle;

    // 필수 함수 포인터 로드
    instance->functions.initialize = (PluginError (*)(PluginContext*, const void*))
        dlsym(handle, "plugin_initialize");
    instance->functions.process = (PluginError (*)(PluginContext*, const float*, float*, int))
        dlsym(handle, "plugin_process");
    instance->functions.finalize = (PluginError (*)(PluginContext*))
        dlsym(handle, "plugin_finalize");

    if (!instance->functions.initialize || !instance->functions.process ||
        !instance->functions.finalize) {
        free(instance);
        dlclose(handle);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 선택적 함수 포인터 로드
    instance->functions.set_parameter = (PluginError (*)(PluginContext*, int, PluginParamValue))
        dlsym(handle, "plugin_set_parameter");
    instance->functions.get_parameter = (PluginError (*)(PluginContext*, int, PluginParamValue*))
        dlsym(handle, "plugin_get_parameter");
    instance->functions.reset = (PluginError (*)(PluginContext*))
        dlsym(handle, "plugin_reset");
    instance->functions.suspend = (PluginError (*)(PluginContext*))
        dlsym(handle, "plugin_suspend");
    instance->functions.resume = (PluginError (*)(PluginContext*))
        dlsym(handle, "plugin_resume");
    instance->functions.get_info = (const char* (*)(PluginContext*, const char*))
        dlsym(handle, "plugin_get_info");
    instance->functions.get_latency = (PluginError (*)(PluginContext*, int*))
        dlsym(handle, "plugin_get_latency");
    instance->functions.get_tail_time = (PluginError (*)(PluginContext*, float*))
        dlsym(handle, "plugin_get_tail_time");

    // 파라미터 정보 로드
    typedef const PluginParameter* (*GetParametersFunc)(int*);
    GetParametersFunc get_parameters = (GetParametersFunc)dlsym(handle, "plugin_get_parameters");
    if (get_parameters) {
        instance->parameters = (PluginParameter*)get_parameters(&instance->num_parameters);
        if (instance->num_parameters > 0) {
            instance->param_values = (PluginParamValue*)calloc(instance->num_parameters,
                                                              sizeof(PluginParamValue));
        }
    }

    // 의존성 정보 로드
    typedef const PluginDependency* (*GetDependenciesFunc)(int*);
    GetDependenciesFunc get_dependencies = (GetDependenciesFunc)dlsym(handle, "plugin_get_dependencies");
    if (get_dependencies) {
        instance->dependencies = (PluginDependency*)get_dependencies(&instance->num_dependencies);
    }

    // 콜백 호출
    if (g_load_callback) {
        error = g_load_callback(path, &instance);
        if (error != ET_SUCCESS) {
            free(instance->param_values);
            free(instance);
            dlclose(handle);
            return error;
        }
    }

    *plugin = instance;
    return ET_SUCCESS;
}

// plugin 언로드
PluginError plugin_unload(PluginRegistry* registry, PluginInstance* plugin) {
    if (!registry || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // plugin이 활성화되어 있으면 비활성화
    if (plugin->state == PLUGIN_STATE_ACTIVE) {
        plugin_deactivate(plugin);
    }

    // 초기화되어 있으면 종료
    if (plugin->state == PLUGIN_STATE_INITIALIZED) {
        plugin_finalize(plugin);
    }

    // 콜백 호출
    if (g_unload_callback) {
        g_unload_callback(plugin);
    }

    // 메모리 해제
    if (plugin->param_values) {
        free(plugin->param_values);
    }

    if (plugin->context) {
        free(plugin->context);
    }

    // 동적 라이브러리 언로드
    if (plugin->handle) {
        dlclose(plugin->handle);
    }

    free(plugin);
    return ET_SUCCESS;
}

// 버전 호환성 검증
bool plugin_is_version_compatible(const PluginVersion* required, const PluginVersion* available) {
    if (!required || !available) {
        return false;
    }

    // 주 버전이 다르면 호환되지 않음
    if (required->major != available->major) {
        return false;
    }

    // 부 버전은 하위 호환성 허용
    if (required->minor > available->minor) {
        return false;
    }

    // 패치 버전은 하위 호환성 허용
    if (required->minor == available->minor && required->patch > available->patch) {
        return false;
    }

    return true;
}

// API 호환성 검증
bool plugin_is_api_compatible(const PluginVersion* plugin_api, const PluginVersion* engine_api) {
    return plugin_is_version_compatible(plugin_api, engine_api);
}

// plugin 초기화
PluginError plugin_initialize(PluginInstance* plugin, const void* config) {
    if (!plugin || plugin->state != PLUGIN_STATE_LOADED) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 컨텍스트 생성
    plugin->context = (PluginContext*)malloc(sizeof(PluginContext));
    if (!plugin->context) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(plugin->context, 0, sizeof(PluginContext));
    plugin->context->plugin = plugin;

    // plugin 초기화 함수 호출
    PluginError error = plugin->functions.initialize(plugin->context, config);
    if (error != ET_SUCCESS) {
        free(plugin->context);
        plugin->context = NULL;
        plugin->state = PLUGIN_STATE_ERROR;
        return error;
    }

    plugin->state = PLUGIN_STATE_INITIALIZED;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback(plugin, "initialized", NULL);
    }

    return ET_SUCCESS;
}

// plugin 종료
PluginError plugin_finalize(PluginInstance* plugin) {
    if (!plugin || plugin->state != PLUGIN_STATE_INITIALIZED) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    PluginError error = plugin->functions.finalize(plugin->context);

    if (plugin->context) {
        free(plugin->context);
        plugin->context = NULL;
    }

    plugin->state = PLUGIN_STATE_LOADED;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback(plugin, "finalized", NULL);
    }

    return error;
}

// plugin 처리
PluginError plugin_process(PluginInstance* plugin, const float* input, float* output, int num_samples) {
    if (!plugin || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (plugin->state != PLUGIN_STATE_ACTIVE) {
        return ET_ERROR_RUNTIME;
    }

    return plugin->functions.process(plugin->context, input, output, num_samples);
}

// plugin 활성화
PluginError plugin_activate(PluginInstance* plugin) {
    if (!plugin || plugin->state != PLUGIN_STATE_INITIALIZED) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    plugin->state = PLUGIN_STATE_ACTIVE;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback(plugin, "activated", NULL);
    }

    return ET_SUCCESS;
}

// plugin 비활성화
PluginError plugin_deactivate(PluginInstance* plugin) {
    if (!plugin || plugin->state != PLUGIN_STATE_ACTIVE) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    plugin->state = PLUGIN_STATE_INITIALIZED;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback(plugin, "deactivated", NULL);
    }

    return ET_SUCCESS;
}

// plugin 상태 조회
PluginState plugin_get_state(PluginInstance* plugin) {
    if (!plugin) {
        return PLUGIN_STATE_ERROR;
    }

    return plugin->state;
}

// 콜백 설정 함수들
void plugin_set_load_callback(PluginLoadCallback callback) {
    g_load_callback = callback;
}

void plugin_set_unload_callback(PluginUnloadCallback callback) {
    g_unload_callback = callback;
}

void plugin_set_event_callback(PluginEventCallback callback) {
    g_event_callback = callback;
}

// plugin 체인 생성
PluginChain* plugin_create_chain(void) {
    PluginChain* chain = (PluginChain*)malloc(sizeof(PluginChain));
    if (!chain) {
        return NULL;
    }

    chain->plugins = NULL;
    chain->bypass_flags = NULL;
    chain->num_plugins = 0;
    chain->capacity = 0;
    chain->temp_buffer = NULL;
    chain->buffer_size = 0;

    return chain;
}

// plugin 체인 해제
void plugin_destroy_chain(PluginChain* chain) {
    if (!chain) return;

    free(chain->plugins);
    free(chain->bypass_flags);
    free(chain->temp_buffer);
    free(chain);
}

// plugin 체인에 plugin 추가
PluginError plugin_chain_add(PluginChain* chain, PluginInstance* plugin) {
    if (!chain || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // array 확장이 필요한 경우
    if (chain->num_plugins >= chain->capacity) {
        int new_capacity = chain->capacity == 0 ? 4 : chain->capacity * 2;

        PluginInstance** new_plugins = (PluginInstance**)realloc(chain->plugins,
                                                                new_capacity * sizeof(PluginInstance*));
        if (!new_plugins) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        bool* new_bypass_flags = (bool*)realloc(chain->bypass_flags,
                                               new_capacity * sizeof(bool));
        if (!new_bypass_flags) {
            // new_plugins는 이미 할당되었으므로 해제해야 함
            // 하지만 chain->plugins는 여전히 유효하므로 새로 할당된 것만 해제
            if (new_plugins != chain->plugins) {
                free(new_plugins);
            }
            return ET_ERROR_OUT_OF_MEMORY;
        }

        chain->plugins = new_plugins;
        chain->bypass_flags = new_bypass_flags;
        chain->capacity = new_capacity;
    }

    chain->plugins[chain->num_plugins] = plugin;
    chain->bypass_flags[chain->num_plugins] = false;
    chain->num_plugins++;

    return ET_SUCCESS;
}

// plugin 체인 처리
PluginError plugin_chain_process(PluginChain* chain, const float* input, float* output, int num_samples) {
    if (!chain || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (chain->num_plugins == 0) {
        // plugin이 없으면 입력을 그대로 출력에 복사
        memcpy(output, input, num_samples * sizeof(float));
        return ET_SUCCESS;
    }

    // 임시 버퍼 크기 확인 및 할당
    if (chain->buffer_size < num_samples) {
        float* new_buffer = (float*)realloc(chain->temp_buffer, num_samples * sizeof(float));
        if (!new_buffer) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
        chain->temp_buffer = new_buffer;
        chain->buffer_size = num_samples;
    }

    const float* current_input = input;
    float* current_output = (chain->num_plugins == 1) ? output : chain->temp_buffer;

    // 각 plugin을 순차적으로 처리
    for (int i = 0; i < chain->num_plugins; i++) {
        if (chain->bypass_flags[i]) {
            // bypass된 plugin은 건너뛰고 입력을 출력에 복사
            if (current_input != current_output) {
                memcpy(current_output, current_input, num_samples * sizeof(float));
            }
        } else {
            // plugin 처리
            PluginError error = plugin_process(chain->plugins[i], current_input, current_output, num_samples);
            if (error != ET_SUCCESS) {
                return error;
            }
        }

        // 다음 반복을 위한 입출력 버퍼 설정
        if (i < chain->num_plugins - 1) {
            current_input = current_output;
            current_output = (i == chain->num_plugins - 2) ? output : chain->temp_buffer;
        }
    }

    return ET_SUCCESS;
}

// plugin 체인에서 bypass 설정
PluginError plugin_chain_set_bypass(PluginChain* chain, PluginInstance* plugin, bool bypass) {
    if (!chain || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // plugin 찾기
    for (int i = 0; i < chain->num_plugins; i++) {
        if (chain->plugins[i] == plugin) {
            chain->bypass_flags[i] = bypass;
            return ET_SUCCESS;
        }
    }

    return ET_ERROR_INVALID_ARGUMENT; // Plugin not found
}

// 파라미터 설정/조회 함수들
PluginError plugin_set_parameter_by_id(PluginInstance* plugin, int param_id, PluginParamValue value) {
    if (!plugin || param_id < 0 || param_id >= plugin->num_parameters) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (plugin->functions.set_parameter) {
        PluginError result = plugin->functions.set_parameter(plugin->context, param_id, value);
        if (result == ET_SUCCESS && plugin->param_values) {
            plugin->param_values[param_id] = value;
        }
        return result;
    }

    return ET_ERROR_NOT_IMPLEMENTED;
}

PluginError plugin_get_parameter_by_id(PluginInstance* plugin, int param_id, PluginParamValue* value) {
    if (!plugin || !value || param_id < 0 || param_id >= plugin->num_parameters) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (plugin->functions.get_parameter) {
        return plugin->functions.get_parameter(plugin->context, param_id, value);
    } else if (plugin->param_values) {
        *value = plugin->param_values[param_id];
        return ET_SUCCESS;
    }

    return ET_ERROR_NOT_IMPLEMENTED;
}

// Plugin registration
PluginError plugin_register(PluginRegistry* registry, PluginInstance* plugin) {
    if (!registry || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // array 확장이 필요한 경우
    if (registry->num_plugins >= registry->capacity) {
        int new_capacity = registry->capacity == 0 ? 4 : registry->capacity * 2;

        PluginInstance** new_plugins = (PluginInstance**)realloc(registry->plugins,
                                                                new_capacity * sizeof(PluginInstance*));
        if (!new_plugins) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        registry->plugins = new_plugins;
        registry->capacity = new_capacity;
    }

    registry->plugins[registry->num_plugins] = plugin;
    registry->num_plugins++;

    return ET_SUCCESS;
}

// plugin 등록 해제
PluginError plugin_unregister(PluginRegistry* registry, const char* name) {
    if (!registry || !name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    for (int i = 0; i < registry->num_plugins; i++) {
        if (strcmp(registry->plugins[i]->metadata.name, name) == 0) {
            // array 압축
            for (int j = i; j < registry->num_plugins - 1; j++) {
                registry->plugins[j] = registry->plugins[j + 1];
            }

            registry->num_plugins--;
            return ET_SUCCESS;
        }
    }

    return ET_ERROR_NOT_FOUND;
}

// 이름으로 plugin 찾기
PluginInstance* plugin_find_by_name(PluginRegistry* registry, const char* name) {
    if (!registry || !name) {
        return NULL;
    }

    for (int i = 0; i < registry->num_plugins; i++) {
        if (strcmp(registry->plugins[i]->metadata.name, name) == 0) {
            return registry->plugins[i];
        }
    }

    return NULL;
}

// UUID로 plugin 찾기
PluginInstance* plugin_find_by_uuid(PluginRegistry* registry, const char* uuid) {
    if (!registry || !uuid) {
        return NULL;
    }

    for (int i = 0; i < registry->num_plugins; i++) {
        if (strcmp(registry->plugins[i]->metadata.uuid, uuid) == 0) {
            return registry->plugins[i];
        }
    }

    return NULL;
}

// plugin 체인에서 plugin 제거
PluginError plugin_chain_remove(PluginChain* chain, PluginInstance* plugin) {
    if (!chain || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // plugin 찾기
    for (int i = 0; i < chain->num_plugins; i++) {
        if (chain->plugins[i] == plugin) {
            // array 압축
            for (int j = i; j < chain->num_plugins - 1; j++) {
                chain->plugins[j] = chain->plugins[j + 1];
                chain->bypass_flags[j] = chain->bypass_flags[j + 1];
            }

            chain->num_plugins--;
            return ET_SUCCESS;
        }
    }

    return ET_ERROR_NOT_FOUND;
}