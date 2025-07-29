#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include "libetude/plugin.h"
#include "libetude/error.h"

// 테스트용 플러그인 메타데이터
static PluginMetadata test_metadata = {
    .name = "TestPlugin",
    .description = "Test plugin for unit testing",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 1},
    .api_version = {1, 0, 0, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "12345678-1234-5678-9abc-123456789abc",
    .checksum = 0x12345678
};

// 테스트용 플러그인 파라미터
static PluginParameter test_parameters[] = {
    {
        .name = "gain",
        .display_name = "Gain",
        .description = "Audio gain control",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 2.0f,
            .default_value = 1.0f,
            .step = 0.01f
        }
    },
    {
        .name = "enabled",
        .display_name = "Enabled",
        .description = "Enable/disable effect",
        .type = PARAM_TYPE_BOOL,
        .value.bool_param = {
            .default_value = true
        }
    }
};

// 테스트용 플러그인 의존성
static PluginDependency test_dependencies[] = {
    {
        .name = "BaseAudioPlugin",
        .min_version = {1, 0, 0, 0},
        .max_version = {1, 9, 9, 9},
        .required = true
    }
};

// 테스트용 플러그인 함수들
static PluginError test_plugin_initialize(PluginContext* ctx, const void* config) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    // 간단한 상태 초기화
    float* gain = (float*)malloc(sizeof(float));
    if (!gain) return ET_ERROR_OUT_OF_MEMORY;

    *gain = 1.0f;
    ctx->user_data = gain;

    return ET_SUCCESS;
}

static PluginError test_plugin_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    float* gain = (float*)ctx->user_data;
    if (!gain) return ET_ERROR_RUNTIME;

    // 간단한 게인 적용
    for (int i = 0; i < num_samples; i++) {
        output[i] = input[i] * (*gain);
    }

    return ET_SUCCESS;
}

static PluginError test_plugin_finalize(PluginContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    if (ctx->user_data) {
        free(ctx->user_data);
        ctx->user_data = NULL;
    }

    return ET_SUCCESS;
}

static PluginError test_plugin_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx || param_id < 0 || param_id >= 2) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (param_id == 0) { // gain 파라미터
        float* gain = (float*)ctx->user_data;
        if (gain) {
            *gain = value.float_value;
        }
    }

    return ET_SUCCESS;
}

static PluginError test_plugin_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value || param_id < 0 || param_id >= 2) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (param_id == 0) { // gain 파라미터
        float* gain = (float*)ctx->user_data;
        if (gain) {
            value->float_value = *gain;
        }
    } else if (param_id == 1) { // enabled 파라미터
        value->bool_value = true;
    }

    return ET_SUCCESS;
}

// 테스트용 플러그인 인스턴스 생성
static PluginInstance* create_test_plugin_instance(void) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) return NULL;

    memset(plugin, 0, sizeof(PluginInstance));

    plugin->metadata = test_metadata;
    plugin->state = PLUGIN_STATE_LOADED;
    plugin->handle = NULL; // 테스트용이므로 실제 라이브러리 핸들 없음

    // 함수 포인터 설정
    plugin->functions.initialize = test_plugin_initialize;
    plugin->functions.process = test_plugin_process;
    plugin->functions.finalize = test_plugin_finalize;
    plugin->functions.set_parameter = test_plugin_set_parameter;
    plugin->functions.get_parameter = test_plugin_get_parameter;

    // 파라미터 설정
    plugin->parameters = test_parameters;
    plugin->num_parameters = 2;
    plugin->param_values = (PluginParamValue*)calloc(2, sizeof(PluginParamValue));
    plugin->param_values[0].float_value = 1.0f;
    plugin->param_values[1].bool_value = true;

    // 의존성 설정
    plugin->dependencies = test_dependencies;
    plugin->num_dependencies = 1;

    return plugin;
}

// 테스트 함수들
void test_plugin_registry_creation(void) {
    printf("Testing plugin registry creation...\n");

    PluginRegistry* registry = plugin_create_registry();
    assert(registry != NULL);
    assert(registry->plugins == NULL);
    assert(registry->num_plugins == 0);
    assert(registry->capacity == 0);
    assert(registry->search_paths == NULL);
    assert(registry->num_search_paths == 0);

    plugin_destroy_registry(registry);
    printf("✓ Plugin registry creation test passed\n");
}

void test_search_path_management(void) {
    printf("Testing search path management...\n");

    PluginRegistry* registry = plugin_create_registry();
    assert(registry != NULL);

    // 검색 경로 추가
    PluginError error = plugin_add_search_path(registry, "/usr/lib/libetude/plugins");
    assert(error == ET_SUCCESS);
    assert(registry->num_search_paths == 1);

    error = plugin_add_search_path(registry, "/usr/local/lib/libetude/plugins");
    assert(error == ET_SUCCESS);
    assert(registry->num_search_paths == 2);

    // 중복 경로 추가 (무시되어야 함)
    error = plugin_add_search_path(registry, "/usr/lib/libetude/plugins");
    assert(error == ET_SUCCESS);
    assert(registry->num_search_paths == 2);

    // 검색 경로 제거
    error = plugin_remove_search_path(registry, "/usr/lib/libetude/plugins");
    assert(error == ET_SUCCESS);
    assert(registry->num_search_paths == 1);

    // 존재하지 않는 경로 제거
    error = plugin_remove_search_path(registry, "/nonexistent/path");
    assert(error == ET_ERROR_INVALID_ARGUMENT);

    // 모든 경로 초기화
    plugin_clear_search_paths(registry);
    assert(registry->num_search_paths == 0);

    plugin_destroy_registry(registry);
    printf("✓ Search path management test passed\n");
}

void test_plugin_metadata_validation(void) {
    printf("Testing plugin metadata validation...\n");

    // 유효한 메타데이터
    PluginError error = plugin_validate_metadata(&test_metadata);
    assert(error == ET_SUCCESS);

    // 잘못된 메타데이터 테스트
    PluginMetadata invalid_metadata = test_metadata;

    // 빈 이름
    strcpy(invalid_metadata.name, "");
    error = plugin_validate_metadata(&invalid_metadata);
    assert(error == ET_ERROR_INVALID_ARGUMENT);

    // 너무 긴 이름
    memset(invalid_metadata.name, 'A', 64);
    invalid_metadata.name[63] = '\0';
    error = plugin_validate_metadata(&invalid_metadata);
    assert(error == ET_ERROR_INVALID_ARGUMENT);

    // 잘못된 UUID
    invalid_metadata = test_metadata;
    strcpy(invalid_metadata.uuid, "invalid-uuid");
    error = plugin_validate_metadata(&invalid_metadata);
    assert(error == ET_ERROR_INVALID_ARGUMENT);

    // 잘못된 버전
    invalid_metadata = test_metadata;
    invalid_metadata.version.major = 0;
    invalid_metadata.version.minor = 0;
    invalid_metadata.version.patch = 0;
    error = plugin_validate_metadata(&invalid_metadata);
    assert(error == ET_ERROR_INVALID_ARGUMENT);

    printf("✓ Plugin metadata validation test passed\n");
}

void test_version_compatibility(void) {
    printf("Testing version compatibility...\n");

    PluginVersion v1_0_0 = {1, 0, 0, 0};
    PluginVersion v1_1_0 = {1, 1, 0, 0};
    PluginVersion v1_0_1 = {1, 0, 1, 0};
    PluginVersion v2_0_0 = {2, 0, 0, 0};

    // 동일한 버전
    assert(plugin_is_version_compatible(&v1_0_0, &v1_0_0) == true);

    // 하위 호환성 (부 버전)
    assert(plugin_is_version_compatible(&v1_0_0, &v1_1_0) == true);
    assert(plugin_is_version_compatible(&v1_1_0, &v1_0_0) == false);

    // 하위 호환성 (패치 버전)
    assert(plugin_is_version_compatible(&v1_0_0, &v1_0_1) == true);
    assert(plugin_is_version_compatible(&v1_0_1, &v1_0_0) == false);

    // 주 버전 비호환성
    assert(plugin_is_version_compatible(&v1_0_0, &v2_0_0) == false);
    assert(plugin_is_version_compatible(&v2_0_0, &v1_0_0) == false);

    printf("✓ Version compatibility test passed\n");
}

void test_plugin_lifecycle(void) {
    printf("Testing plugin lifecycle...\n");

    PluginInstance* plugin = create_test_plugin_instance();
    assert(plugin != NULL);
    assert(plugin->state == PLUGIN_STATE_LOADED);

    // 초기화
    PluginError error = plugin_initialize(plugin, NULL);
    assert(error == ET_SUCCESS);
    assert(plugin->state == PLUGIN_STATE_INITIALIZED);
    assert(plugin->context != NULL);

    // 활성화
    error = plugin_activate(plugin);
    assert(error == ET_SUCCESS);
    assert(plugin->state == PLUGIN_STATE_ACTIVE);

    // 처리 테스트
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {0.0f};

    error = plugin_process(plugin, input, output, 4);
    assert(error == ET_SUCCESS);

    // 게인이 1.0이므로 입력과 출력이 같아야 함
    for (int i = 0; i < 4; i++) {
        assert(output[i] == input[i]);
    }

    // 파라미터 변경 테스트
    PluginParamValue gain_value;
    gain_value.float_value = 2.0f;
    error = plugin->functions.set_parameter(plugin->context, 0, gain_value);
    assert(error == ET_SUCCESS);

    // 변경된 게인으로 처리
    error = plugin_process(plugin, input, output, 4);
    assert(error == ET_SUCCESS);

    // 게인이 2.0이므로 출력이 입력의 2배가 되어야 함
    for (int i = 0; i < 4; i++) {
        assert(output[i] == input[i] * 2.0f);
    }

    // 비활성화
    error = plugin_deactivate(plugin);
    assert(error == ET_SUCCESS);
    assert(plugin->state == PLUGIN_STATE_INITIALIZED);

    // 종료
    error = plugin_finalize(plugin);
    assert(error == ET_SUCCESS);
    assert(plugin->state == PLUGIN_STATE_LOADED);

    // 메모리 해제
    free(plugin->param_values);
    free(plugin);

    printf("✓ Plugin lifecycle test passed\n");
}

void test_plugin_chain(void) {
    printf("Testing plugin chain...\n");

    PluginChain* chain = plugin_create_chain();
    assert(chain != NULL);

    // 테스트용 플러그인 2개 생성
    PluginInstance* plugin1 = create_test_plugin_instance();
    PluginInstance* plugin2 = create_test_plugin_instance();

    assert(plugin1 != NULL);
    assert(plugin2 != NULL);

    // 플러그인 초기화 및 활성화
    PluginError error = plugin_initialize(plugin1, NULL);
    assert(error == ET_SUCCESS);
    error = plugin_activate(plugin1);
    assert(error == ET_SUCCESS);

    error = plugin_initialize(plugin2, NULL);
    assert(error == ET_SUCCESS);
    error = plugin_activate(plugin2);
    assert(error == ET_SUCCESS);

    // 체인에 플러그인 추가
    error = plugin_chain_add(chain, plugin1);
    assert(error == ET_SUCCESS);
    error = plugin_chain_add(chain, plugin2);
    assert(error == ET_SUCCESS);

    // 첫 번째 플러그인의 게인을 2.0으로 설정
    PluginParamValue gain_value;
    gain_value.float_value = 2.0f;
    error = plugin1->functions.set_parameter(plugin1->context, 0, gain_value);
    assert(error == ET_SUCCESS);

    // 두 번째 플러그인의 게인을 1.5로 설정
    gain_value.float_value = 1.5f;
    error = plugin2->functions.set_parameter(plugin2->context, 0, gain_value);
    assert(error == ET_SUCCESS);

    // 체인 처리 테스트
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {0.0f};

    error = plugin_chain_process(chain, input, output, 4);
    assert(error == ET_SUCCESS);

    // 첫 번째 플러그인: 2.0배, 두 번째 플러그인: 1.5배
    // 총 3.0배가 되어야 함
    for (int i = 0; i < 4; i++) {
        assert(output[i] == input[i] * 3.0f);
    }

    // 첫 번째 플러그인 바이패스 테스트
    error = plugin_chain_set_bypass(chain, plugin1, true);
    assert(error == ET_SUCCESS);

    error = plugin_chain_process(chain, input, output, 4);
    assert(error == ET_SUCCESS);

    // 첫 번째 플러그인이 바이패스되므로 1.5배만 적용
    for (int i = 0; i < 4; i++) {
        assert(output[i] == input[i] * 1.5f);
    }

    // 정리
    plugin_deactivate(plugin1);
    plugin_finalize(plugin1);
    plugin_deactivate(plugin2);
    plugin_finalize(plugin2);

    free(plugin1->param_values);
    free(plugin1);
    free(plugin2->param_values);
    free(plugin2);

    plugin_destroy_chain(chain);

    printf("✓ Plugin chain test passed\n");
}

void test_checksum_calculation(void) {
    printf("Testing checksum calculation...\n");

    const char* test_data = "Hello, LibEtude!";
    uint32_t checksum1 = plugin_calculate_checksum(test_data, strlen(test_data));
    uint32_t checksum2 = plugin_calculate_checksum(test_data, strlen(test_data));

    // 동일한 데이터는 동일한 체크섬을 가져야 함
    assert(checksum1 == checksum2);
    assert(checksum1 != 0);

    // 다른 데이터는 다른 체크섬을 가져야 함
    const char* test_data2 = "Hello, World!";
    uint32_t checksum3 = plugin_calculate_checksum(test_data2, strlen(test_data2));
    assert(checksum1 != checksum3);

    // NULL 데이터는 0을 반환해야 함
    uint32_t checksum4 = plugin_calculate_checksum(NULL, 10);
    assert(checksum4 == 0);

    printf("✓ Checksum calculation test passed\n");
}

void test_uuid_generation(void) {
    printf("Testing UUID generation...\n");

    char uuid1[37];
    char uuid2[37];

    PluginError error1 = plugin_generate_uuid(uuid1, sizeof(uuid1));
    PluginError error2 = plugin_generate_uuid(uuid2, sizeof(uuid2));

    assert(error1 == ET_SUCCESS);
    assert(error2 == ET_SUCCESS);

    // UUID 길이 확인
    assert(strlen(uuid1) == 36);
    assert(strlen(uuid2) == 36);

    // 서로 다른 UUID가 생성되어야 함
    assert(strcmp(uuid1, uuid2) != 0);

    // 잘못된 버퍼 크기 테스트
    char small_buffer[10];
    PluginError error3 = plugin_generate_uuid(small_buffer, sizeof(small_buffer));
    assert(error3 == ET_ERROR_INVALID_ARGUMENT);

    printf("✓ UUID generation test passed\n");
}

int main(void) {
    printf("Running LibEtude Plugin System Tests...\n\n");

    test_plugin_registry_creation();
    test_search_path_management();
    test_plugin_metadata_validation();
    test_version_compatibility();
    test_plugin_lifecycle();
    test_plugin_chain();
    test_checksum_calculation();
    test_uuid_generation();

    printf("\n✅ All plugin system tests passed!\n");
    return 0;
}