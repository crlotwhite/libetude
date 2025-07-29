#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libetude/plugin.h"
#include "libetude/plugin_dependency.h"

// 이벤트 콜백 함수
void dependency_event_callback(const char* event_type, const char* plugin_name, void* event_data, void* user_data) {
    printf("[이벤트] %s: %s\n", event_type, plugin_name ? plugin_name : "N/A");

    if (strcmp(event_type, "dependencies_resolved") == 0) {
        DependencyResult* results = (DependencyResult*)event_data;
        if (results) {
            printf("  의존성 해결 완료\n");
        }
    } else if (strcmp(event_type, "updates_available") == 0) {
        UpdateInfo* updates = (UpdateInfo*)event_data;
        if (updates) {
            printf("  업데이트 사용 가능\n");
        }
    } else if (strcmp(event_type, "security_vulnerabilities_found") == 0) {
        SecurityVulnerability* vulns = (SecurityVulnerability*)event_data;
        if (vulns) {
            printf("  보안 취약점 발견\n");
        }
    }
}

// 업데이트 진행률 콜백
void update_progress_callback(const char* plugin_name, float progress, void* user_data) {
    printf("\r[업데이트] %s: %.1f%%", plugin_name, progress);
    fflush(stdout);
    if (progress >= 100.0f) {
        printf("\n");
    }
}

// 업데이트 완료 콜백
void update_complete_callback(const char* plugin_name, bool success, const char* error, void* user_data) {
    if (success) {
        printf("[업데이트 완료] %s: 성공\n", plugin_name);
    } else {
        printf("[업데이트 실패] %s: %s\n", plugin_name, error ? error : "알 수 없는 오류");
    }
}

// 버전 정보 출력
void print_version(const PluginVersion* version) {
    if (version->build > 0) {
        printf("%d.%d.%d.%d", version->major, version->minor, version->patch, version->build);
    } else {
        printf("%d.%d.%d", version->major, version->minor, version->patch);
    }
}

// 의존성 결과 출력
void print_dependency_results(const DependencyResult* results, int num_results) {
    printf("\n=== 의존성 해결 결과 ===\n");

    for (int i = 0; i < num_results; i++) {
        const DependencyResult* result = &results[i];

        printf("플러그인: %s\n", result->plugin_name);
        printf("  의존성: %s\n", result->dependency_name);
        printf("  요구 버전: ");
        print_version(&result->required_version);
        printf("\n");
        printf("  사용 가능한 버전: ");
        print_version(&result->available_version);
        printf("\n");

        switch (result->status) {
            case DEPENDENCY_STATUS_RESOLVED:
                printf("  상태: ✅ 해결됨\n");
                break;
            case DEPENDENCY_STATUS_MISSING:
                printf("  상태: ❌ 누락됨\n");
                break;
            case DEPENDENCY_STATUS_INCOMPATIBLE:
                printf("  상태: ⚠️  호환되지 않음\n");
                break;
            case DEPENDENCY_STATUS_CIRCULAR:
                printf("  상태: 🔄 순환 의존성\n");
                break;
            default:
                printf("  상태: ❓ 해결되지 않음\n");
                break;
        }

        if (strlen(result->error_message) > 0) {
            printf("  오류: %s\n", result->error_message);
        }
        printf("\n");
    }
}

// 업데이트 정보 출력
void print_update_info(const UpdateInfo* updates, int num_updates) {
    printf("\n=== 사용 가능한 업데이트 ===\n");

    for (int i = 0; i < num_updates; i++) {
        const UpdateInfo* update = &updates[i];

        printf("플러그인: %s\n", update->plugin_name);
        printf("  현재 버전: ");
        print_version(&update->current_version);
        printf("\n");
        printf("  사용 가능한 버전: ");
        print_version(&update->available_version);
        printf("\n");
        printf("  다운로드 URL: %s\n", update->update_url);

        if (update->security_update) {
            printf("  🔒 보안 업데이트\n");
        }

        if (update->breaking_changes) {
            printf("  ⚠️  호환성 파괴 변경\n");
        }

        if (strlen(update->changelog) > 0) {
            printf("  변경 사항: %s\n", update->changelog);
        }
        printf("\n");
    }
}

// 보안 취약점 정보 출력
void print_security_vulnerabilities(const SecurityVulnerability* vulns, int num_vulns) {
    printf("\n=== 보안 취약점 ===\n");

    for (int i = 0; i < num_vulns; i++) {
        const SecurityVulnerability* vuln = &vulns[i];

        printf("플러그인: %s\n", vuln->plugin_name);
        printf("  취약점 ID: %s\n", vuln->vulnerability_id);
        printf("  심각도: %s\n", vuln->severity);
        printf("  설명: %s\n", vuln->description);

        printf("  영향받는 버전: ");
        for (int j = 0; j < vuln->num_affected_versions; j++) {
            if (j > 0) printf(", ");
            print_version(&vuln->affected_versions[j]);
        }
        printf("\n");

        printf("  수정된 버전: ");
        print_version(&vuln->fixed_version);
        printf("\n\n");
    }
}

// 의존성 리포트 출력
void print_dependency_report(const DependencyReport* report) {
    printf("\n=== 의존성 리포트 ===\n");
    printf("생성 시간: %s\n", report->report_timestamp);
    printf("총 플러그인 수: %d\n", report->total_plugins);
    printf("해결된 의존성: %d\n", report->resolved_dependencies);
    printf("해결되지 않은 의존성: %d\n", report->unresolved_dependencies);
    printf("순환 의존성: %d\n", report->circular_dependencies);
    printf("보안 취약점: %d\n", report->security_vulnerabilities);
    printf("구버전 플러그인: %d\n", report->outdated_plugins);
    printf("\n");
}

int main(int argc, char* argv[]) {
    printf("LibEtude 플러그인 의존성 관리 시스템 예제\n");
    printf("==========================================\n\n");

    // 이벤트 콜백 설정
    dependency_set_event_callback(dependency_event_callback, NULL);

    // 플러그인 레지스트리 생성
    PluginRegistry* registry = plugin_create_registry();
    if (!registry) {
        printf("❌ 플러그인 레지스트리 생성 실패\n");
        return 1;
    }

    // 의존성 그래프 생성
    DependencyGraph* graph = dependency_create_graph(registry);
    if (!graph) {
        printf("❌ 의존성 그래프 생성 실패\n");
        plugin_destroy_registry(registry);
        return 1;
    }

    // 의존성 캐시 생성
    const char* cache_dir = "/tmp/libetude_dependency_cache";
    DependencyCache* cache = dependency_create_cache(cache_dir);
    if (!cache) {
        printf("⚠️  의존성 캐시 생성 실패 (계속 진행)\n");
    }

    // 설정 관리 예제
    printf("1. 의존성 관리 설정\n");
    DependencyConfig config = {
        .version_policy = DEPENDENCY_POLICY_COMPATIBLE,
        .allow_prerelease = false,
        .auto_update = false,
        .require_signature = false,
        .max_dependency_depth = 10,
        .num_trusted_sources = 1
    };
    strcpy(config.trusted_sources[0], "https://plugins.libetude.org");

    if (dependency_set_config(&config) == ET_SUCCESS) {
        printf("✅ 의존성 설정 완료\n");
    } else {
        printf("❌ 의존성 설정 실패\n");
    }

    // 플러그인 검색 경로 추가
    printf("\n2. 플러그인 검색 경로 설정\n");
    plugin_add_search_path(registry, "./plugins");
    plugin_add_search_path(registry, "/usr/local/lib/libetude/plugins");
    plugin_add_search_path(registry, "~/.libetude/plugins");
    printf("✅ 검색 경로 설정 완료\n");

    // 실제 플러그인 파일이 없으므로 시뮬레이션
    printf("\n3. 플러그인 로딩 시뮬레이션\n");
    printf("(실제 환경에서는 plugin_scan_directory() 또는 plugin_load_from_file() 사용)\n");

    // 버전 파싱 예제
    printf("\n4. 버전 파싱 테스트\n");
    PluginVersion version;
    const char* version_strings[] = {"1.0.0", "2.1.5", "1.0.0.123"};

    for (int i = 0; i < 3; i++) {
        if (dependency_parse_version_string(version_strings[i], &version) == ET_SUCCESS) {
            printf("✅ '%s' -> %d.%d.%d.%d\n", version_strings[i],
                   version.major, version.minor, version.patch, version.build);
        } else {
            printf("❌ '%s' 파싱 실패\n", version_strings[i]);
        }
    }

    // 버전 비교 예제
    printf("\n5. 버전 비교 테스트\n");
    PluginVersion v1 = {1, 0, 0, 0};
    PluginVersion v2 = {1, 0, 1, 0};
    PluginVersion v3 = {1, 1, 0, 0};

    printf("1.0.0 vs 1.0.1: %d\n", dependency_compare_versions(&v1, &v2));
    printf("1.0.1 vs 1.0.0: %d\n", dependency_compare_versions(&v2, &v1));
    printf("1.0.0 vs 1.1.0: %d\n", dependency_compare_versions(&v1, &v3));

    // 버전 만족도 테스트
    printf("\n6. 버전 만족도 테스트\n");
    PluginVersion min_ver = {1, 0, 0, 0};
    PluginVersion max_ver = {1, 9, 9, 9};
    PluginVersion test_ver = {1, 5, 0, 0};

    if (dependency_is_version_satisfied(&min_ver, &max_ver, &test_ver)) {
        printf("✅ 버전 1.5.0은 요구사항 (1.0.0 - 1.9.9.9)을 만족합니다\n");
    } else {
        printf("❌ 버전 만족도 테스트 실패\n");
    }

    // 순환 의존성 검사 예제
    printf("\n7. 순환 의존성 검사\n");
    bool has_circular = false;
    if (dependency_check_circular(graph, &has_circular) == ET_SUCCESS) {
        if (has_circular) {
            printf("⚠️  순환 의존성이 발견되었습니다\n");
        } else {
            printf("✅ 순환 의존성이 없습니다\n");
        }
    } else {
        printf("❌ 순환 의존성 검사 실패\n");
    }

    // 의존성 리포트 생성
    printf("\n8. 의존성 리포트 생성\n");
    DependencyReport report;
    if (dependency_generate_report(graph, &report) == ET_SUCCESS) {
        print_dependency_report(&report);

        // 리포트 내보내기
        const char* json_path = "/tmp/dependency_report.json";
        const char* text_path = "/tmp/dependency_report.txt";

        if (dependency_export_report(&report, json_path, "json") == ET_SUCCESS) {
            printf("✅ JSON 리포트 저장: %s\n", json_path);
        }

        if (dependency_export_report(&report, text_path, "text") == ET_SUCCESS) {
            printf("✅ 텍스트 리포트 저장: %s\n", text_path);
        }
    } else {
        printf("❌ 의존성 리포트 생성 실패\n");
    }

    // 업데이트 확인 시뮬레이션
    printf("\n9. 업데이트 확인 (시뮬레이션)\n");
    printf("(실제 환경에서는 업데이트 서버 URL 필요)\n");
    printf("예: dependency_check_updates(registry, \"https://updates.libetude.org/api/v1/updates\", &updates, &num_updates)\n");

    // 보안 취약점 검사 시뮬레이션
    printf("\n10. 보안 취약점 검사 (시뮬레이션)\n");
    printf("(실제 환경에서는 보안 데이터베이스 URL 필요)\n");
    printf("예: dependency_check_security(registry, \"https://security.libetude.org/api/v1/vulnerabilities\", &vulns, &num_vulns)\n");

    // 캐시 테스트
    if (cache) {
        printf("\n11. 의존성 캐시 테스트\n");

        // 테스트 의존성 결과 생성
        DependencyResult test_results[] = {
            {
                .plugin_name = "TestPlugin",
                .dependency_name = "TestDependency",
                .status = DEPENDENCY_STATUS_RESOLVED,
                .required_version = {1, 0, 0, 0},
                .available_version = {1, 1, 0, 0},
                .error_message = ""
            }
        };

        // 캐시에 저장
        if (dependency_cache_store(cache, "TestPlugin", test_results, 1) == ET_SUCCESS) {
            printf("✅ 의존성 결과 캐시 저장 완료\n");

            // 캐시에서 로드
            DependencyResult* loaded_results = NULL;
            int num_loaded = 0;
            if (dependency_cache_load(cache, "TestPlugin", &loaded_results, &num_loaded) == ET_SUCCESS) {
                printf("✅ 의존성 결과 캐시 로드 완료 (%d개)\n", num_loaded);
                free(loaded_results);
            } else {
                printf("❌ 의존성 결과 캐시 로드 실패\n");
            }
        } else {
            printf("❌ 의존성 결과 캐시 저장 실패\n");
        }
    }

    printf("\n12. 정리\n");

    // 정리
    if (cache) {
        dependency_destroy_cache(cache);
        printf("✅ 의존성 캐시 정리 완료\n");
    }

    dependency_destroy_graph(graph);
    printf("✅ 의존성 그래프 정리 완료\n");

    plugin_destroy_registry(registry);
    printf("✅ 플러그인 레지스트리 정리 완료\n");

    printf("\n=== 예제 완료 ===\n");
    printf("실제 사용 시에는 다음 기능들을 활용할 수 있습니다:\n");
    printf("- 플러그인 자동 검색 및 로딩\n");
    printf("- 의존성 자동 해결\n");
    printf("- 업데이트 확인 및 자동 다운로드\n");
    printf("- 보안 취약점 검사\n");
    printf("- 디지털 서명 검증\n");
    printf("- 의존성 캐싱\n");
    printf("- 상세한 리포팅\n");

    return 0;
}