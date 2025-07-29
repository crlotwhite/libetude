#ifndef LIBETUDE_PLUGIN_DEPENDENCY_H
#define LIBETUDE_PLUGIN_DEPENDENCY_H

#include "plugin.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// 의존성 해결 상태
typedef enum {
    DEPENDENCY_STATUS_UNRESOLVED = 0,    // 해결되지 않음
    DEPENDENCY_STATUS_RESOLVED = 1,      // 해결됨
    DEPENDENCY_STATUS_MISSING = 2,       // 누락됨
    DEPENDENCY_STATUS_INCOMPATIBLE = 3,  // 호환되지 않음
    DEPENDENCY_STATUS_CIRCULAR = 4       // 순환 의존성
} DependencyStatus;

// 의존성 해결 결과
typedef struct {
    char plugin_name[64];                // 플러그인 이름
    char dependency_name[64];            // 의존성 이름
    DependencyStatus status;             // 해결 상태
    PluginVersion required_version;      // 요구 버전
    PluginVersion available_version;     // 사용 가능한 버전
    char error_message[256];             // 오류 메시지
} DependencyResult;

// 의존성 그래프 노드
typedef struct DependencyNode {
    PluginInstance* plugin;              // 플러그인 인스턴스
    struct DependencyNode** dependencies; // 의존성 노드 배열
    int num_dependencies;                // 의존성 수
    struct DependencyNode** dependents;  // 종속 노드 배열
    int num_dependents;                  // 종속 수
    bool visited;                        // 방문 플래그 (순환 검사용)
    bool resolved;                       // 해결 플래그
} DependencyNode;

// 의존성 그래프
typedef struct {
    DependencyNode** nodes;              // 노드 배열
    int num_nodes;                       // 노드 수
    int capacity;                        // 용량
    PluginRegistry* registry;            // 플러그인 레지스트리 참조
} DependencyGraph;

// 업데이트 정보
typedef struct {
    char plugin_name[64];                // 플러그인 이름
    PluginVersion current_version;       // 현재 버전
    PluginVersion available_version;     // 사용 가능한 버전
    char update_url[256];                // 업데이트 URL
    char changelog[512];                 // 변경 로그
    bool security_update;                // 보안 업데이트 여부
    bool breaking_changes;               // 호환성 파괴 변경 여부
} UpdateInfo;

// 업데이트 콜백
typedef void (*UpdateProgressCallback)(const char* plugin_name, float progress, void* user_data);
typedef void (*UpdateCompleteCallback)(const char* plugin_name, bool success, const char* error, void* user_data);

// 의존성 그래프 관리
DependencyGraph* dependency_create_graph(PluginRegistry* registry);
void dependency_destroy_graph(DependencyGraph* graph);
PluginError dependency_add_plugin(DependencyGraph* graph, PluginInstance* plugin);
PluginError dependency_remove_plugin(DependencyGraph* graph, PluginInstance* plugin);

// 의존성 해결
PluginError dependency_resolve_all(DependencyGraph* graph, DependencyResult** results, int* num_results);
PluginError dependency_resolve_plugin(DependencyGraph* graph, PluginInstance* plugin, DependencyResult** results, int* num_results);
PluginError dependency_check_circular(DependencyGraph* graph, bool* has_circular);
PluginError dependency_get_load_order(DependencyGraph* graph, PluginInstance*** load_order, int* num_plugins);

// 버전 호환성 검증
bool dependency_is_version_satisfied(const PluginVersion* required_min, const PluginVersion* required_max, const PluginVersion* available);
PluginError dependency_check_compatibility(PluginInstance* plugin, PluginRegistry* registry, DependencyResult** results, int* num_results);
bool dependency_is_api_backward_compatible(const PluginVersion* old_api, const PluginVersion* new_api);

// 동적 업데이트 지원
PluginError dependency_check_updates(PluginRegistry* registry, const char* update_server_url, UpdateInfo** updates, int* num_updates);
PluginError dependency_download_update(const UpdateInfo* update, const char* download_path, UpdateProgressCallback progress_cb, void* user_data);
PluginError dependency_apply_update(PluginRegistry* registry, const char* plugin_name, const char* update_path, UpdateCompleteCallback complete_cb, void* user_data);
PluginError dependency_rollback_update(PluginRegistry* registry, const char* plugin_name);

// 의존성 캐시 관리
typedef struct DependencyCache DependencyCache;

DependencyCache* dependency_create_cache(const char* cache_dir);
void dependency_destroy_cache(DependencyCache* cache);
PluginError dependency_cache_store(DependencyCache* cache, const char* plugin_name, const DependencyResult* results, int num_results);
PluginError dependency_cache_load(DependencyCache* cache, const char* plugin_name, DependencyResult** results, int* num_results);
PluginError dependency_cache_invalidate(DependencyCache* cache, const char* plugin_name);
PluginError dependency_cache_clear(DependencyCache* cache);

// 의존성 정책 관리
typedef enum {
    DEPENDENCY_POLICY_STRICT = 0,        // 엄격한 버전 매칭
    DEPENDENCY_POLICY_COMPATIBLE = 1,    // 호환 가능한 버전 허용
    DEPENDENCY_POLICY_LATEST = 2         // 최신 버전 선호
} DependencyPolicy;

typedef struct {
    DependencyPolicy version_policy;     // 버전 정책
    bool allow_prerelease;               // 프리릴리스 허용
    bool auto_update;                    // 자동 업데이트
    bool require_signature;              // 서명 검증 요구
    int max_dependency_depth;            // 최대 의존성 깊이
    char trusted_sources[10][256];       // 신뢰할 수 있는 소스
    int num_trusted_sources;             // 신뢰할 수 있는 소스 수
} DependencyConfig;

PluginError dependency_set_config(const DependencyConfig* config);
PluginError dependency_get_config(DependencyConfig* config);

// 의존성 분석 및 리포팅
typedef struct {
    int total_plugins;                   // 총 플러그인 수
    int resolved_dependencies;           // 해결된 의존성 수
    int unresolved_dependencies;         // 해결되지 않은 의존성 수
    int circular_dependencies;           // 순환 의존성 수
    int security_vulnerabilities;        // 보안 취약점 수
    int outdated_plugins;                // 구버전 플러그인 수
    char report_timestamp[32];           // 리포트 생성 시간
} DependencyReport;

PluginError dependency_generate_report(DependencyGraph* graph, DependencyReport* report);
PluginError dependency_export_report(const DependencyReport* report, const char* output_path, const char* format);

// 보안 검증
typedef struct {
    char plugin_name[64];                // 플러그인 이름
    char vulnerability_id[32];           // 취약점 ID
    char severity[16];                   // 심각도 (LOW, MEDIUM, HIGH, CRITICAL)
    char description[256];               // 설명
    PluginVersion affected_versions[10]; // 영향받는 버전들
    int num_affected_versions;           // 영향받는 버전 수
    PluginVersion fixed_version;         // 수정된 버전
} SecurityVulnerability;

PluginError dependency_check_security(PluginRegistry* registry, const char* security_db_url, SecurityVulnerability** vulnerabilities, int* num_vulnerabilities);
PluginError dependency_verify_signature(const char* plugin_path, const char* signature_path, const char* public_key_path);

// 유틸리티 함수
PluginError dependency_parse_version_string(const char* version_str, PluginVersion* version);
PluginError dependency_version_to_string(const PluginVersion* version, char* version_str, size_t buffer_size);
int dependency_compare_versions(const PluginVersion* v1, const PluginVersion* v2);
PluginError dependency_find_best_match(PluginRegistry* registry, const char* plugin_name, const PluginVersion* min_version, const PluginVersion* max_version, PluginInstance** best_match);

// 이벤트 콜백
typedef void (*DependencyEventCallback)(const char* event_type, const char* plugin_name, void* event_data, void* user_data);

void dependency_set_event_callback(DependencyEventCallback callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLUGIN_DEPENDENCY_H