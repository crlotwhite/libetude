#include "libetude/plugin_dependency.h"
#include "libetude/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
// #include <curl/curl.h>  // 임시로 비활성화
// #include <json-c/json.h>  // 임시로 비활성화
// #include <openssl/sha.h>  // 임시로 비활성화
// #include <openssl/rsa.h>  // 임시로 비활성화
// #include <openssl/pem.h>  // 임시로 비활성화

// 의존성 캐시 구조체
struct DependencyCache {
    char cache_dir[256];                 // 캐시 디렉토리
    time_t last_update;                  // 마지막 업데이트 시간
};

// 전역 설정
static DependencyConfig g_dependency_config = {
    .version_policy = DEPENDENCY_POLICY_COMPATIBLE,
    .allow_prerelease = false,
    .auto_update = false,
    .require_signature = false,
    .max_dependency_depth = 10,
    .num_trusted_sources = 0
};

// 전역 이벤트 콜백
static DependencyEventCallback g_event_callback = NULL;
static void* g_event_user_data = NULL;

// HTTP 응답 구조체
typedef struct {
    char* data;
    size_t size;
} HTTPResponse;

// HTTP 응답 콜백
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t total_size = size * nmemb;
    char* new_data = realloc(response->data, response->size + total_size + 1);

    if (!new_data) {
        return 0;
    }

    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = '\0';

    return total_size;
}

// 의존성 그래프 생성
DependencyGraph* dependency_create_graph(PluginRegistry* registry) {
    if (!registry) {
        return NULL;
    }

    DependencyGraph* graph = (DependencyGraph*)malloc(sizeof(DependencyGraph));
    if (!graph) {
        return NULL;
    }

    graph->nodes = NULL;
    graph->num_nodes = 0;
    graph->capacity = 0;
    graph->registry = registry;

    return graph;
}

// 의존성 그래프 해제
void dependency_destroy_graph(DependencyGraph* graph) {
    if (!graph) return;

    for (int i = 0; i < graph->num_nodes; i++) {
        DependencyNode* node = graph->nodes[i];
        if (node) {
            free(node->dependencies);
            free(node->dependents);
            free(node);
        }
    }

    free(graph->nodes);
    free(graph);
}

// 의존성 노드 찾기
static DependencyNode* find_dependency_node(DependencyGraph* graph, PluginInstance* plugin) {
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i]->plugin == plugin) {
            return graph->nodes[i];
        }
    }
    return NULL;
}

// 의존성 그래프에 플러그인 추가
PluginError dependency_add_plugin(DependencyGraph* graph, PluginInstance* plugin) {
    if (!graph || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 이미 존재하는지 확인
    if (find_dependency_node(graph, plugin)) {
        return ET_SUCCESS; // 이미 존재함
    }

    // 그래프 확장이 필요한 경우
    if (graph->num_nodes >= graph->capacity) {
        int new_capacity = graph->capacity == 0 ? 4 : graph->capacity * 2;
        DependencyNode** new_nodes = (DependencyNode**)realloc(graph->nodes,
                                                              new_capacity * sizeof(DependencyNode*));
        if (!new_nodes) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        graph->nodes = new_nodes;
        graph->capacity = new_capacity;
    }

    // 새 노드 생성
    DependencyNode* node = (DependencyNode*)malloc(sizeof(DependencyNode));
    if (!node) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(node, 0, sizeof(DependencyNode));
    node->plugin = plugin;

    // 의존성 배열 초기화
    if (plugin->num_dependencies > 0) {
        node->dependencies = (DependencyNode**)calloc(plugin->num_dependencies, sizeof(DependencyNode*));
        if (!node->dependencies) {
            free(node);
            return ET_ERROR_OUT_OF_MEMORY;
        }
        node->num_dependencies = plugin->num_dependencies;
    }

    graph->nodes[graph->num_nodes] = node;
    graph->num_nodes++;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("plugin_added", plugin->metadata.name, plugin, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 의존성 그래프에서 플러그인 제거
PluginError dependency_remove_plugin(DependencyGraph* graph, PluginInstance* plugin) {
    if (!graph || !plugin) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 노드 찾기
    int node_index = -1;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i]->plugin == plugin) {
            node_index = i;
            break;
        }
    }

    if (node_index == -1) {
        return ET_ERROR_NOT_FOUND;
    }

    DependencyNode* node = graph->nodes[node_index];

    // 다른 노드들에서 이 노드에 대한 참조 제거
    for (int i = 0; i < graph->num_nodes; i++) {
        if (i == node_index) continue;

        DependencyNode* other_node = graph->nodes[i];

        // 의존성에서 제거
        for (int j = 0; j < other_node->num_dependencies; j++) {
            if (other_node->dependencies[j] == node) {
                other_node->dependencies[j] = NULL;
            }
        }

        // 종속에서 제거
        for (int j = 0; j < other_node->num_dependents; j++) {
            if (other_node->dependents[j] == node) {
                other_node->dependents[j] = NULL;
            }
        }
    }

    // 노드 메모리 해제
    free(node->dependencies);
    free(node->dependents);
    free(node);

    // 배열 압축
    for (int i = node_index; i < graph->num_nodes - 1; i++) {
        graph->nodes[i] = graph->nodes[i + 1];
    }
    graph->num_nodes--;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("plugin_removed", plugin->metadata.name, plugin, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 버전 비교 함수
int dependency_compare_versions(const PluginVersion* v1, const PluginVersion* v2) {
    if (!v1 || !v2) {
        return 0;
    }

    if (v1->major != v2->major) {
        return (v1->major > v2->major) ? 1 : -1;
    }

    if (v1->minor != v2->minor) {
        return (v1->minor > v2->minor) ? 1 : -1;
    }

    if (v1->patch != v2->patch) {
        return (v1->patch > v2->patch) ? 1 : -1;
    }

    if (v1->build != v2->build) {
        return (v1->build > v2->build) ? 1 : -1;
    }

    return 0;
}

// 버전 만족 여부 확인
bool dependency_is_version_satisfied(const PluginVersion* required_min, const PluginVersion* required_max, const PluginVersion* available) {
    if (!required_min || !available) {
        return false;
    }

    // 최소 버전 확인
    if (dependency_compare_versions(available, required_min) < 0) {
        return false;
    }

    // 최대 버전 확인 (지정된 경우)
    if (required_max && dependency_compare_versions(available, required_max) > 0) {
        return false;
    }

    return true;
}

// 순환 의존성 검사 (DFS 사용)
static bool check_circular_dependency_recursive(DependencyNode* node, DependencyNode* target) {
    if (!node) return false;

    if (node == target) {
        return true; // 순환 발견
    }

    if (node->visited) {
        return false; // 이미 방문함
    }

    node->visited = true;

    // 모든 의존성에 대해 재귀 검사
    for (int i = 0; i < node->num_dependencies; i++) {
        if (node->dependencies[i] && check_circular_dependency_recursive(node->dependencies[i], target)) {
            return true;
        }
    }

    return false;
}

// 순환 의존성 검사
PluginError dependency_check_circular(DependencyGraph* graph, bool* has_circular) {
    if (!graph || !has_circular) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *has_circular = false;

    // 모든 노드의 방문 플래그 초기화
    for (int i = 0; i < graph->num_nodes; i++) {
        graph->nodes[i]->visited = false;
    }

    // 각 노드에서 순환 검사
    for (int i = 0; i < graph->num_nodes; i++) {
        DependencyNode* node = graph->nodes[i];

        // 방문 플래그 초기화
        for (int j = 0; j < graph->num_nodes; j++) {
            graph->nodes[j]->visited = false;
        }

        // 순환 검사
        for (int j = 0; j < node->num_dependencies; j++) {
            if (node->dependencies[j] && check_circular_dependency_recursive(node->dependencies[j], node)) {
                *has_circular = true;
                return ET_SUCCESS;
            }
        }
    }

    return ET_SUCCESS;
}

// 의존성 해결
PluginError dependency_resolve_plugin(DependencyGraph* graph, PluginInstance* plugin, DependencyResult** results, int* num_results) {
    if (!graph || !plugin || !results || !num_results) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *results = NULL;
    *num_results = 0;

    if (plugin->num_dependencies == 0) {
        return ET_SUCCESS; // 의존성 없음
    }

    // 결과 배열 할당
    DependencyResult* dep_results = (DependencyResult*)calloc(plugin->num_dependencies, sizeof(DependencyResult));
    if (!dep_results) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int resolved_count = 0;

    // 각 의존성 해결
    for (int i = 0; i < plugin->num_dependencies; i++) {
        PluginDependency* dep = &plugin->dependencies[i];
        DependencyResult* result = &dep_results[resolved_count];

        strncpy(result->plugin_name, plugin->metadata.name, sizeof(result->plugin_name) - 1);
        strncpy(result->dependency_name, dep->name, sizeof(result->dependency_name) - 1);
        result->required_version = dep->min_version;

        // 의존성 플러그인 찾기
        PluginInstance* dep_plugin = plugin_find_by_name(graph->registry, dep->name);
        if (!dep_plugin) {
            result->status = DEPENDENCY_STATUS_MISSING;
            snprintf(result->error_message, sizeof(result->error_message),
                    "Required plugin '%s' not found", dep->name);
        } else {
            result->available_version = dep_plugin->metadata.version;

            // 버전 호환성 검사
            if (dependency_is_version_satisfied(&dep->min_version, &dep->max_version, &dep_plugin->metadata.version)) {
                result->status = DEPENDENCY_STATUS_RESOLVED;
            } else {
                result->status = DEPENDENCY_STATUS_INCOMPATIBLE;
                snprintf(result->error_message, sizeof(result->error_message),
                        "Version mismatch: required %d.%d.%d, available %d.%d.%d",
                        dep->min_version.major, dep->min_version.minor, dep->min_version.patch,
                        dep_plugin->metadata.version.major, dep_plugin->metadata.version.minor, dep_plugin->metadata.version.patch);
            }
        }

        resolved_count++;
    }

    *results = dep_results;
    *num_results = resolved_count;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("dependencies_resolved", plugin->metadata.name, dep_results, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 모든 플러그인의 의존성 해결
PluginError dependency_resolve_all(DependencyGraph* graph, DependencyResult** results, int* num_results) {
    if (!graph || !results || !num_results) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *results = NULL;
    *num_results = 0;

    // 총 의존성 수 계산
    int total_dependencies = 0;
    for (int i = 0; i < graph->num_nodes; i++) {
        total_dependencies += graph->nodes[i]->plugin->num_dependencies;
    }

    if (total_dependencies == 0) {
        return ET_SUCCESS; // 의존성 없음
    }

    // 결과 배열 할당
    DependencyResult* all_results = (DependencyResult*)calloc(total_dependencies, sizeof(DependencyResult));
    if (!all_results) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int result_index = 0;

    // 각 플러그인의 의존성 해결
    for (int i = 0; i < graph->num_nodes; i++) {
        PluginInstance* plugin = graph->nodes[i]->plugin;
        DependencyResult* plugin_results = NULL;
        int plugin_num_results = 0;

        PluginError error = dependency_resolve_plugin(graph, plugin, &plugin_results, &plugin_num_results);
        if (error != ET_SUCCESS) {
            free(all_results);
            return error;
        }

        // 결과 복사
        if (plugin_results && plugin_num_results > 0) {
            memcpy(&all_results[result_index], plugin_results, plugin_num_results * sizeof(DependencyResult));
            result_index += plugin_num_results;
            free(plugin_results);
        }
    }

    *results = all_results;
    *num_results = result_index;

    return ET_SUCCESS;
}

// 로드 순서 계산 (위상 정렬)
PluginError dependency_get_load_order(DependencyGraph* graph, PluginInstance*** load_order, int* num_plugins) {
    if (!graph || !load_order || !num_plugins) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *load_order = NULL;
    *num_plugins = 0;

    if (graph->num_nodes == 0) {
        return ET_SUCCESS;
    }

    // 순환 의존성 검사
    bool has_circular = false;
    PluginError error = dependency_check_circular(graph, &has_circular);
    if (error != ET_SUCCESS) {
        return error;
    }

    if (has_circular) {
        return ET_ERROR_RUNTIME; // 순환 의존성 존재
    }

    // 결과 배열 할당
    PluginInstance** order = (PluginInstance**)malloc(graph->num_nodes * sizeof(PluginInstance*));
    if (!order) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 진입 차수 계산
    int* in_degree = (int*)calloc(graph->num_nodes, sizeof(int));
    if (!in_degree) {
        free(order);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 의존성 그래프 구축 및 진입 차수 계산
    for (int i = 0; i < graph->num_nodes; i++) {
        DependencyNode* node = graph->nodes[i];
        PluginInstance* plugin = node->plugin;

        for (int j = 0; j < plugin->num_dependencies; j++) {
            PluginDependency* dep = &plugin->dependencies[j];

            // 의존성 플러그인 찾기
            for (int k = 0; k < graph->num_nodes; k++) {
                if (strcmp(graph->nodes[k]->plugin->metadata.name, dep->name) == 0) {
                    in_degree[i]++;
                    break;
                }
            }
        }
    }

    // 위상 정렬 (Kahn's algorithm)
    int* queue = (int*)malloc(graph->num_nodes * sizeof(int));
    if (!queue) {
        free(order);
        free(in_degree);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int queue_front = 0, queue_rear = 0;
    int order_index = 0;

    // 진입 차수가 0인 노드들을 큐에 추가
    for (int i = 0; i < graph->num_nodes; i++) {
        if (in_degree[i] == 0) {
            queue[queue_rear++] = i;
        }
    }

    // 위상 정렬 수행
    while (queue_front < queue_rear) {
        int current_index = queue[queue_front++];
        DependencyNode* current_node = graph->nodes[current_index];
        order[order_index++] = current_node->plugin;

        // 현재 노드에 의존하는 노드들의 진입 차수 감소
        PluginInstance* current_plugin = current_node->plugin;
        for (int i = 0; i < graph->num_nodes; i++) {
            if (i == current_index) continue;

            PluginInstance* other_plugin = graph->nodes[i]->plugin;

            // other_plugin이 current_plugin에 의존하는지 확인
            for (int j = 0; j < other_plugin->num_dependencies; j++) {
                if (strcmp(other_plugin->dependencies[j].name, current_plugin->metadata.name) == 0) {
                    in_degree[i]--;
                    if (in_degree[i] == 0) {
                        queue[queue_rear++] = i;
                    }
                    break;
                }
            }
        }
    }

    free(queue);
    free(in_degree);

    // 모든 노드가 처리되었는지 확인
    if (order_index != graph->num_nodes) {
        free(order);
        return ET_ERROR_RUNTIME; // 순환 의존성 존재
    }

    *load_order = order;
    *num_plugins = order_index;

    return ET_SUCCESS;
}

// 버전 문자열 파싱
PluginError dependency_parse_version_string(const char* version_str, PluginVersion* version) {
    if (!version_str || !version) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(version, 0, sizeof(PluginVersion));

    int parsed = sscanf(version_str, "%hu.%hu.%hu.%hu",
                       &version->major, &version->minor, &version->patch, &version->build);

    if (parsed < 3) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 버전을 문자열로 변환
PluginError dependency_version_to_string(const PluginVersion* version, char* version_str, size_t buffer_size) {
    if (!version || !version_str || buffer_size < 16) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (version->build > 0) {
        snprintf(version_str, buffer_size, "%d.%d.%d.%d",
                version->major, version->minor, version->patch, version->build);
    } else {
        snprintf(version_str, buffer_size, "%d.%d.%d",
                version->major, version->minor, version->patch);
    }

    return ET_SUCCESS;
}

// 업데이트 확인
PluginError dependency_check_updates(PluginRegistry* registry, const char* update_server_url, UpdateInfo** updates, int* num_updates) {
    if (!registry || !update_server_url || !updates || !num_updates) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *updates = NULL;
    *num_updates = 0;

    // HTTP 요청 초기화
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ET_ERROR_RUNTIME;
    }

    HTTPResponse response = {0};

    // HTTP 요청 설정
    curl_easy_setopt(curl, CURLOPT_URL, update_server_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // 요청 수행
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response.data);
        return ET_ERROR_IO;
    }

    // JSON 파싱
    json_object* root = json_tokener_parse(response.data);
    free(response.data);

    if (!root) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    json_object* updates_array = NULL;
    if (!json_object_object_get_ex(root, "updates", &updates_array) ||
        !json_object_is_type(updates_array, json_type_array)) {
        json_object_put(root);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int array_length = json_object_array_length(updates_array);
    if (array_length == 0) {
        json_object_put(root);
        return ET_SUCCESS;
    }

    // 업데이트 정보 배열 할당
    UpdateInfo* update_infos = (UpdateInfo*)calloc(array_length, sizeof(UpdateInfo));
    if (!update_infos) {
        json_object_put(root);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int valid_updates = 0;

    // 각 업데이트 정보 파싱
    for (int i = 0; i < array_length; i++) {
        json_object* update_obj = json_object_array_get_idx(updates_array, i);
        if (!update_obj) continue;

        UpdateInfo* info = &update_infos[valid_updates];

        // 플러그인 이름
        json_object* name_obj = NULL;
        if (json_object_object_get_ex(update_obj, "name", &name_obj)) {
            const char* name = json_object_get_string(name_obj);
            strncpy(info->plugin_name, name, sizeof(info->plugin_name) - 1);
        }

        // 현재 버전
        json_object* current_version_obj = NULL;
        if (json_object_object_get_ex(update_obj, "current_version", &current_version_obj)) {
            const char* version_str = json_object_get_string(current_version_obj);
            dependency_parse_version_string(version_str, &info->current_version);
        }

        // 사용 가능한 버전
        json_object* available_version_obj = NULL;
        if (json_object_object_get_ex(update_obj, "available_version", &available_version_obj)) {
            const char* version_str = json_object_get_string(available_version_obj);
            dependency_parse_version_string(version_str, &info->available_version);
        }

        // 업데이트 URL
        json_object* url_obj = NULL;
        if (json_object_object_get_ex(update_obj, "download_url", &url_obj)) {
            const char* url = json_object_get_string(url_obj);
            strncpy(info->update_url, url, sizeof(info->update_url) - 1);
        }

        // 변경 로그
        json_object* changelog_obj = NULL;
        if (json_object_object_get_ex(update_obj, "changelog", &changelog_obj)) {
            const char* changelog = json_object_get_string(changelog_obj);
            strncpy(info->changelog, changelog, sizeof(info->changelog) - 1);
        }

        // 보안 업데이트 여부
        json_object* security_obj = NULL;
        if (json_object_object_get_ex(update_obj, "security_update", &security_obj)) {
            info->security_update = json_object_get_boolean(security_obj);
        }

        // 호환성 파괴 변경 여부
        json_object* breaking_obj = NULL;
        if (json_object_object_get_ex(update_obj, "breaking_changes", &breaking_obj)) {
            info->breaking_changes = json_object_get_boolean(breaking_obj);
        }

        // 해당 플러그인이 레지스트리에 있는지 확인
        PluginInstance* plugin = plugin_find_by_name(registry, info->plugin_name);
        if (plugin) {
            // 현재 버전과 비교하여 업데이트가 필요한지 확인
            if (dependency_compare_versions(&info->available_version, &plugin->metadata.version) > 0) {
                info->current_version = plugin->metadata.version;
                valid_updates++;
            }
        }
    }

    json_object_put(root);

    if (valid_updates == 0) {
        free(update_infos);
        return ET_SUCCESS;
    }

    // 유효한 업데이트만 포함하는 배열로 축소
    UpdateInfo* final_updates = (UpdateInfo*)realloc(update_infos, valid_updates * sizeof(UpdateInfo));
    if (!final_updates) {
        free(update_infos);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    *updates = final_updates;
    *num_updates = valid_updates;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("updates_available", NULL, final_updates, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 의존성 캐시 생성
DependencyCache* dependency_create_cache(const char* cache_dir) {
    if (!cache_dir) {
        return NULL;
    }

    DependencyCache* cache = (DependencyCache*)malloc(sizeof(DependencyCache));
    if (!cache) {
        return NULL;
    }

    strncpy(cache->cache_dir, cache_dir, sizeof(cache->cache_dir) - 1);
    cache->cache_dir[sizeof(cache->cache_dir) - 1] = '\0';
    cache->last_update = time(NULL);

    // 캐시 디렉토리 생성
    mkdir(cache_dir, 0755);

    return cache;
}

// 의존성 캐시 해제
void dependency_destroy_cache(DependencyCache* cache) {
    if (cache) {
        free(cache);
    }
}

// 설정 관리
PluginError dependency_set_config(const DependencyConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    g_dependency_config = *config;
    return ET_SUCCESS;
}

PluginError dependency_get_config(DependencyConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *config = g_dependency_config;
    return ET_SUCCESS;
}

// 이벤트 콜백 설정
void dependency_set_event_callback(DependencyEventCallback callback, void* user_data) {
    g_event_callback = callback;
    g_event_user_data = user_data;
}

// 의존성 리포트 생성
PluginError dependency_generate_report(DependencyGraph* graph, DependencyReport* report) {
    if (!graph || !report) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(report, 0, sizeof(DependencyReport));

    // 현재 시간 설정
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(report->report_timestamp, sizeof(report->report_timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    report->total_plugins = graph->num_nodes;

    // 모든 의존성 해결
    DependencyResult* results = NULL;
    int num_results = 0;
    PluginError error = dependency_resolve_all(graph, &results, &num_results);

    if (error == ET_SUCCESS && results) {
        // 의존성 상태 집계
        for (int i = 0; i < num_results; i++) {
            switch (results[i].status) {
                case DEPENDENCY_STATUS_RESOLVED:
                    report->resolved_dependencies++;
                    break;
                case DEPENDENCY_STATUS_UNRESOLVED:
                case DEPENDENCY_STATUS_MISSING:
                case DEPENDENCY_STATUS_INCOMPATIBLE:
                    report->unresolved_dependencies++;
                    break;
                case DEPENDENCY_STATUS_CIRCULAR:
                    report->circular_dependencies++;
                    break;
            }
        }
        free(results);
    }

    return ET_SUCCESS;
}

// 최적 매치 찾기
PluginError dependency_find_best_match(PluginRegistry* registry, const char* plugin_name,
                                      const PluginVersion* min_version, const PluginVersion* max_version,
                                      PluginInstance** best_match) {
    if (!registry || !plugin_name || !min_version || !best_match) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *best_match = NULL;
    PluginInstance* candidate = NULL;

    // 모든 플러그인 검사
    for (int i = 0; i < registry->num_plugins; i++) {
        PluginInstance* plugin = registry->plugins[i];

        // 이름 매치 확인
        if (strcmp(plugin->metadata.name, plugin_name) != 0) {
            continue;
        }

        // 버전 호환성 확인
        if (!dependency_is_version_satisfied(min_version, max_version, &plugin->metadata.version)) {
            continue;
        }

        // 첫 번째 후보이거나 더 나은 버전인 경우
        if (!candidate || dependency_compare_versions(&plugin->metadata.version, &candidate->metadata.version) > 0) {
            candidate = plugin;
        }
    }

    if (candidate) {
        *best_match = candidate;
        return ET_SUCCESS;
    }

    return ET_ERROR_NOT_FOUND;
}//
업데이트 다운로드 진행률 구조체
typedef struct {
    UpdateProgressCallback callback;
    void* user_data;
    const char* plugin_name;
} DownloadProgress;

// 다운로드 진행률 콜백
static int download_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    DownloadProgress* progress = (DownloadProgress*)clientp;

    if (dltotal > 0 && progress->callback) {
        float percent = (float)dlnow / (float)dltotal * 100.0f;
        progress->callback(progress->plugin_name, percent, progress->user_data);
    }

    return 0;
}

// 파일 쓰기 콜백
static size_t write_file_callback(void* contents, size_t size, size_t nmemb, FILE* file) {
    return fwrite(contents, size, nmemb, file);
}

// 업데이트 다운로드
PluginError dependency_download_update(const UpdateInfo* update, const char* download_path,
                                     UpdateProgressCallback progress_cb, void* user_data) {
    if (!update || !download_path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // HTTP 클라이언트 초기화
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ET_ERROR_RUNTIME;
    }

    // 다운로드 파일 열기
    FILE* file = fopen(download_path, "wb");
    if (!file) {
        curl_easy_cleanup(curl);
        return ET_ERROR_IO;
    }

    // 진행률 콜백 설정
    DownloadProgress progress = {
        .callback = progress_cb,
        .user_data = user_data,
        .plugin_name = update->plugin_name
    };

    // HTTP 요청 설정
    curl_easy_setopt(curl, CURLOPT_URL, update->update_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5분 타임아웃

    if (progress_cb) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    }

    // 다운로드 수행
    CURLcode res = curl_easy_perform(curl);

    fclose(file);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(download_path); // 실패 시 파일 삭제
        return ET_ERROR_IO;
    }

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("update_downloaded", update->plugin_name, (void*)download_path, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 업데이트 적용
PluginError dependency_apply_update(PluginRegistry* registry, const char* plugin_name,
                                  const char* update_path, UpdateCompleteCallback complete_cb, void* user_data) {
    if (!registry || !plugin_name || !update_path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기존 플러그인 찾기
    PluginInstance* old_plugin = plugin_find_by_name(registry, plugin_name);
    if (!old_plugin) {
        if (complete_cb) {
            complete_cb(plugin_name, false, "Plugin not found", user_data);
        }
        return ET_ERROR_NOT_FOUND;
    }

    // 백업 경로 생성
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s.backup", update_path);

    // 기존 플러그인 백업 (구현 필요)
    // 실제로는 기존 플러그인 파일을 백업해야 함

    // 새 플러그인 로드 시도
    PluginInstance* new_plugin = NULL;
    PluginError error = plugin_load_from_file(registry, update_path, &new_plugin);

    if (error != ET_SUCCESS) {
        if (complete_cb) {
            complete_cb(plugin_name, false, "Failed to load updated plugin", user_data);
        }
        return error;
    }

    // 플러그인 이름 확인
    if (strcmp(new_plugin->metadata.name, plugin_name) != 0) {
        plugin_unload(registry, new_plugin);
        if (complete_cb) {
            complete_cb(plugin_name, false, "Plugin name mismatch", user_data);
        }
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기존 플러그인 언로드
    plugin_unregister(registry, plugin_name);
    plugin_unload(registry, old_plugin);

    // 새 플러그인 등록
    error = plugin_register(registry, new_plugin);
    if (error != ET_SUCCESS) {
        plugin_unload(registry, new_plugin);
        if (complete_cb) {
            complete_cb(plugin_name, false, "Failed to register updated plugin", user_data);
        }
        return error;
    }

    // 성공 콜백 호출
    if (complete_cb) {
        complete_cb(plugin_name, true, NULL, user_data);
    }

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("update_applied", plugin_name, new_plugin, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 업데이트 롤백
PluginError dependency_rollback_update(PluginRegistry* registry, const char* plugin_name) {
    if (!registry || !plugin_name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 백업 파일 경로 구성
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s.backup", plugin_name);

    // 백업 파일 존재 확인
    struct stat st;
    if (stat(backup_path, &st) != 0) {
        return ET_ERROR_NOT_FOUND; // 백업 파일 없음
    }

    // 현재 플러그인 제거
    PluginInstance* current_plugin = plugin_find_by_name(registry, plugin_name);
    if (current_plugin) {
        plugin_unregister(registry, plugin_name);
        plugin_unload(registry, current_plugin);
    }

    // 백업에서 플러그인 복원
    PluginInstance* restored_plugin = NULL;
    PluginError error = plugin_load_from_file(registry, backup_path, &restored_plugin);

    if (error != ET_SUCCESS) {
        return error;
    }

    // 복원된 플러그인 등록
    error = plugin_register(registry, restored_plugin);
    if (error != ET_SUCCESS) {
        plugin_unload(registry, restored_plugin);
        return error;
    }

    // 백업 파일 삭제
    remove(backup_path);

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("update_rolled_back", plugin_name, restored_plugin, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 의존성 캐시에 저장
PluginError dependency_cache_store(DependencyCache* cache, const char* plugin_name,
                                 const DependencyResult* results, int num_results) {
    if (!cache || !plugin_name || !results || num_results <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 캐시 파일 경로 생성
    char cache_file[512];
    snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", cache->cache_dir, plugin_name);

    // 캐시 파일 열기
    FILE* file = fopen(cache_file, "wb");
    if (!file) {
        return ET_ERROR_IO;
    }

    // 헤더 쓰기
    uint32_t magic = 0x44455043; // "DEPC"
    uint32_t version = 1;
    uint32_t count = (uint32_t)num_results;
    time_t timestamp = time(NULL);

    fwrite(&magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&count, sizeof(count), 1, file);
    fwrite(&timestamp, sizeof(timestamp), 1, file);

    // 의존성 결과 쓰기
    fwrite(results, sizeof(DependencyResult), num_results, file);

    fclose(file);
    return ET_SUCCESS;
}

// 의존성 캐시에서 로드
PluginError dependency_cache_load(DependencyCache* cache, const char* plugin_name,
                                DependencyResult** results, int* num_results) {
    if (!cache || !plugin_name || !results || !num_results) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *results = NULL;
    *num_results = 0;

    // 캐시 파일 경로 생성
    char cache_file[512];
    snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", cache->cache_dir, plugin_name);

    // 캐시 파일 열기
    FILE* file = fopen(cache_file, "rb");
    if (!file) {
        return ET_ERROR_NOT_FOUND;
    }

    // 헤더 읽기
    uint32_t magic, version, count;
    time_t timestamp;

    if (fread(&magic, sizeof(magic), 1, file) != 1 || magic != 0x44455043 ||
        fread(&version, sizeof(version), 1, file) != 1 || version != 1 ||
        fread(&count, sizeof(count), 1, file) != 1 ||
        fread(&timestamp, sizeof(timestamp), 1, file) != 1) {
        fclose(file);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 캐시 만료 확인 (24시간)
    time_t now = time(NULL);
    if (now - timestamp > 24 * 3600) {
        fclose(file);
        remove(cache_file);
        return ET_ERROR_NOT_FOUND;
    }

    // 의존성 결과 읽기
    DependencyResult* cached_results = (DependencyResult*)malloc(count * sizeof(DependencyResult));
    if (!cached_results) {
        fclose(file);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    if (fread(cached_results, sizeof(DependencyResult), count, file) != count) {
        free(cached_results);
        fclose(file);
        return ET_ERROR_IO;
    }

    fclose(file);

    *results = cached_results;
    *num_results = (int)count;

    return ET_SUCCESS;
}

// 캐시 무효화
PluginError dependency_cache_invalidate(DependencyCache* cache, const char* plugin_name) {
    if (!cache || !plugin_name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char cache_file[512];
    snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", cache->cache_dir, plugin_name);

    if (remove(cache_file) == 0) {
        return ET_SUCCESS;
    }

    return ET_ERROR_NOT_FOUND;
}

// 캐시 전체 삭제
PluginError dependency_cache_clear(DependencyCache* cache) {
    if (!cache) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 캐시 디렉토리의 모든 .cache 파일 삭제
    DIR* dir = opendir(cache->cache_dir);
    if (!dir) {
        return ET_ERROR_IO;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".cache")) {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", cache->cache_dir, entry->d_name);
            remove(file_path);
        }
    }

    closedir(dir);
    return ET_SUCCESS;
}

// 보안 취약점 검사
PluginError dependency_check_security(PluginRegistry* registry, const char* security_db_url,
                                     SecurityVulnerability** vulnerabilities, int* num_vulnerabilities) {
    if (!registry || !security_db_url || !vulnerabilities || !num_vulnerabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *vulnerabilities = NULL;
    *num_vulnerabilities = 0;

    // HTTP 요청으로 보안 데이터베이스 조회
    CURL* curl = curl_easy_init();
    if (!curl) {
        return ET_ERROR_RUNTIME;
    }

    HTTPResponse response = {0};

    curl_easy_setopt(curl, CURLOPT_URL, security_db_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response.data);
        return ET_ERROR_IO;
    }

    // JSON 파싱
    json_object* root = json_tokener_parse(response.data);
    free(response.data);

    if (!root) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    json_object* vulns_array = NULL;
    if (!json_object_object_get_ex(root, "vulnerabilities", &vulns_array) ||
        !json_object_is_type(vulns_array, json_type_array)) {
        json_object_put(root);
        return ET_SUCCESS; // 취약점 없음
    }

    int array_length = json_object_array_length(vulns_array);
    if (array_length == 0) {
        json_object_put(root);
        return ET_SUCCESS;
    }

    // 취약점 정보 배열 할당
    SecurityVulnerability* vulns = (SecurityVulnerability*)calloc(array_length, sizeof(SecurityVulnerability));
    if (!vulns) {
        json_object_put(root);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int valid_vulns = 0;

    // 각 취약점 정보 파싱 및 해당 플러그인 확인
    for (int i = 0; i < array_length; i++) {
        json_object* vuln_obj = json_object_array_get_idx(vulns_array, i);
        if (!vuln_obj) continue;

        SecurityVulnerability* vuln = &vulns[valid_vulns];

        // 플러그인 이름
        json_object* name_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "plugin_name", &name_obj)) {
            const char* name = json_object_get_string(name_obj);
            strncpy(vuln->plugin_name, name, sizeof(vuln->plugin_name) - 1);
        }

        // 해당 플러그인이 설치되어 있는지 확인
        PluginInstance* plugin = plugin_find_by_name(registry, vuln->plugin_name);
        if (!plugin) {
            continue; // 설치되지 않은 플러그인은 건너뛰기
        }

        // 취약점 ID
        json_object* id_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "id", &id_obj)) {
            const char* id = json_object_get_string(id_obj);
            strncpy(vuln->vulnerability_id, id, sizeof(vuln->vulnerability_id) - 1);
        }

        // 심각도
        json_object* severity_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "severity", &severity_obj)) {
            const char* severity = json_object_get_string(severity_obj);
            strncpy(vuln->severity, severity, sizeof(vuln->severity) - 1);
        }

        // 설명
        json_object* desc_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "description", &desc_obj)) {
            const char* desc = json_object_get_string(desc_obj);
            strncpy(vuln->description, desc, sizeof(vuln->description) - 1);
        }

        // 영향받는 버전들
        json_object* affected_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "affected_versions", &affected_obj) &&
            json_object_is_type(affected_obj, json_type_array)) {

            int affected_count = json_object_array_length(affected_obj);
            vuln->num_affected_versions = (affected_count > 10) ? 10 : affected_count;

            for (int j = 0; j < vuln->num_affected_versions; j++) {
                json_object* version_obj = json_object_array_get_idx(affected_obj, j);
                if (version_obj) {
                    const char* version_str = json_object_get_string(version_obj);
                    dependency_parse_version_string(version_str, &vuln->affected_versions[j]);
                }
            }
        }

        // 수정된 버전
        json_object* fixed_obj = NULL;
        if (json_object_object_get_ex(vuln_obj, "fixed_version", &fixed_obj)) {
            const char* version_str = json_object_get_string(fixed_obj);
            dependency_parse_version_string(version_str, &vuln->fixed_version);
        }

        // 현재 설치된 버전이 영향받는지 확인
        bool is_affected = false;
        for (int j = 0; j < vuln->num_affected_versions; j++) {
            if (dependency_compare_versions(&plugin->metadata.version, &vuln->affected_versions[j]) == 0) {
                is_affected = true;
                break;
            }
        }

        if (is_affected) {
            valid_vulns++;
        }
    }

    json_object_put(root);

    if (valid_vulns == 0) {
        free(vulns);
        return ET_SUCCESS;
    }

    // 유효한 취약점만 포함하는 배열로 축소
    SecurityVulnerability* final_vulns = (SecurityVulnerability*)realloc(vulns, valid_vulns * sizeof(SecurityVulnerability));
    if (!final_vulns) {
        free(vulns);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    *vulnerabilities = final_vulns;
    *num_vulnerabilities = valid_vulns;

    // 이벤트 콜백 호출
    if (g_event_callback) {
        g_event_callback("security_vulnerabilities_found", NULL, final_vulns, g_event_user_data);
    }

    return ET_SUCCESS;
}

// 디지털 서명 검증
PluginError dependency_verify_signature(const char* plugin_path, const char* signature_path, const char* public_key_path) {
    if (!plugin_path || !signature_path || !public_key_path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 공개키 로드
    FILE* key_file = fopen(public_key_path, "r");
    if (!key_file) {
        return ET_ERROR_IO;
    }

    RSA* rsa = PEM_read_RSA_PUBKEY(key_file, NULL, NULL, NULL);
    fclose(key_file);

    if (!rsa) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 플러그인 파일 해시 계산
    FILE* plugin_file = fopen(plugin_path, "rb");
    if (!plugin_file) {
        RSA_free(rsa);
        return ET_ERROR_IO;
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), plugin_file)) > 0) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    fclose(plugin_file);

    // 서명 파일 읽기
    FILE* sig_file = fopen(signature_path, "rb");
    if (!sig_file) {
        RSA_free(rsa);
        return ET_ERROR_IO;
    }

    fseek(sig_file, 0, SEEK_END);
    long sig_size = ftell(sig_file);
    fseek(sig_file, 0, SEEK_SET);

    unsigned char* signature = (unsigned char*)malloc(sig_size);
    if (!signature) {
        fclose(sig_file);
        RSA_free(rsa);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    fread(signature, 1, sig_size, sig_file);
    fclose(sig_file);

    // 서명 검증
    int result = RSA_verify(NID_sha256, hash, SHA256_DIGEST_LENGTH, signature, sig_size, rsa);

    free(signature);
    RSA_free(rsa);

    return (result == 1) ? ET_SUCCESS : ET_ERROR_INVALID_ARGUMENT;
}

// 리포트 내보내기
PluginError dependency_export_report(const DependencyReport* report, const char* output_path, const char* format) {
    if (!report || !output_path || !format) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(output_path, "w");
    if (!file) {
        return ET_ERROR_IO;
    }

    if (strcmp(format, "json") == 0) {
        // JSON 형식으로 출력
        fprintf(file, "{\n");
        fprintf(file, "  \"timestamp\": \"%s\",\n", report->report_timestamp);
        fprintf(file, "  \"total_plugins\": %d,\n", report->total_plugins);
        fprintf(file, "  \"resolved_dependencies\": %d,\n", report->resolved_dependencies);
        fprintf(file, "  \"unresolved_dependencies\": %d,\n", report->unresolved_dependencies);
        fprintf(file, "  \"circular_dependencies\": %d,\n", report->circular_dependencies);
        fprintf(file, "  \"security_vulnerabilities\": %d,\n", report->security_vulnerabilities);
        fprintf(file, "  \"outdated_plugins\": %d\n", report->outdated_plugins);
        fprintf(file, "}\n");
    } else if (strcmp(format, "text") == 0) {
        // 텍스트 형식으로 출력
        fprintf(file, "Dependency Report\n");
        fprintf(file, "================\n\n");
        fprintf(file, "Generated: %s\n\n", report->report_timestamp);
        fprintf(file, "Total Plugins: %d\n", report->total_plugins);
        fprintf(file, "Resolved Dependencies: %d\n", report->resolved_dependencies);
        fprintf(file, "Unresolved Dependencies: %d\n", report->unresolved_dependencies);
        fprintf(file, "Circular Dependencies: %d\n", report->circular_dependencies);
        fprintf(file, "Security Vulnerabilities: %d\n", report->security_vulnerabilities);
        fprintf(file, "Outdated Plugins: %d\n", report->outdated_plugins);
    } else {
        fclose(file);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    fclose(file);
    return ET_SUCCESS;
}

// API 하위 호환성 검사
bool dependency_is_api_backward_compatible(const PluginVersion* old_api, const PluginVersion* new_api) {
    if (!old_api || !new_api) {
        return false;
    }

    // 주 버전이 다르면 호환되지 않음
    if (old_api->major != new_api->major) {
        return false;
    }

    // 부 버전이 증가한 경우는 하위 호환
    if (new_api->minor >= old_api->minor) {
        return true;
    }

    return false;
}

// 호환성 검사
PluginError dependency_check_compatibility(PluginInstance* plugin, PluginRegistry* registry,
                                          DependencyResult** results, int* num_results) {
    if (!plugin || !registry || !results || !num_results) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기본적으로 dependency_resolve_plugin과 동일하지만 추가 호환성 검사 수행
    PluginError error = dependency_resolve_plugin(NULL, plugin, results, num_results);
    if (error != ET_SUCCESS) {
        return error;
    }

    // API 호환성 추가 검사
    for (int i = 0; i < *num_results; i++) {
        DependencyResult* result = &(*results)[i];

        if (result->status == DEPENDENCY_STATUS_RESOLVED) {
            PluginInstance* dep_plugin = plugin_find_by_name(registry, result->dependency_name);
            if (dep_plugin) {
                // API 버전 호환성 검사
                if (!dependency_is_api_backward_compatible(&result->required_version, &dep_plugin->metadata.api_version)) {
                    result->status = DEPENDENCY_STATUS_INCOMPATIBLE;
                    snprintf(result->error_message, sizeof(result->error_message),
                            "API version incompatible: required %d.%d, available %d.%d",
                            result->required_version.major, result->required_version.minor,
                            dep_plugin->metadata.api_version.major, dep_plugin->metadata.api_version.minor);
                }
            }
        }
    }

    return ET_SUCCESS;
}