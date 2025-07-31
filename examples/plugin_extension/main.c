/**
 * @file main.c
 * @brief LibEtude 플러그인 확장 데모 애플리케이션
 *
 * 이 데모는 다음 기능을 제공합니다:
 * - 플러그인 로딩 및 사용
 * - 사용자 정의 효과 구현
 * - 확장 모델 적용
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>

#include "libetude/api.h"
#include "libetude/plugin.h"
#include "libetude/plugin_dependency.h"
#include "libetude/audio_effects.h"
#include "libetude/lef_format.h"
#include "libetude/error.h"

// 상수 정의
#define MAX_TEXT_LENGTH 1024
#define MAX_AUDIO_LENGTH 48000 * 10
#define MAX_PLUGINS 50
#define MAX_EXTENSIONS 20
#define MAX_EFFECTS_CHAIN 10

// 플러그인 타입
typedef enum {
    PLUGIN_TYPE_AUDIO_EFFECT = 0,
    PLUGIN_TYPE_VOICE_MODEL = 1,
    PLUGIN_TYPE_LANGUAGE_PACK = 2,
    PLUGIN_TYPE_CUSTOM_FILTER = 3
} PluginType;

// 플러그인 정보 구조체
typedef struct {
    char name[64];
    char version[16];
    char author[64];
    char description[256];
    PluginType type;
    bool loaded;
    bool enabled;
    void* handle;
    Plugin* plugin_instance;
} PluginInfo;

// 확장 모델 정보 구조체
typedef struct {
    char name[64];
    char path[256];
    char base_model[64];
    char description[128];
    bool loaded;
    bool active;
    void* model_data;
} ExtensionInfo;

// 효과 체인 항목
typedef struct {
    PluginInfo* plugin;
    void* effect_params;
    bool enabled;
} EffectChainItem;

// 애플리케이션 상태 구조체
typedef struct {
    LibEtudeEngine* engine;

    // 플러그인 관리
    PluginInfo plugins[MAX_PLUGINS];
    int num_plugins;

    // 확장 모델 관리
    ExtensionInfo extensions[MAX_EXTENSIONS];
    int num_extensions;

    // 효과 체인
    EffectChainItem effect_chain[MAX_EFFECTS_CHAIN];
    int effect_chain_length;

    // 현재 설정
    char current_base_model[256];
    char current_extension[64];
    bool effects_enabled;

    // 플러그인 디렉토리
    char plugin_dir[256];
    char extension_dir[256];

} PluginDemo;

// 도움말 출력
static void print_help() {
    printf("\n=== LibEtude 플러그인 확장 데모 ===\n");
    printf("명령어:\n");
    printf("  help              - 이 도움말 표시\n");
    printf("  scan              - 플러그인 및 확장 모델 스캔\n");
    printf("  plugins           - 사용 가능한 플러그인 목록\n");
    printf("  extensions        - 사용 가능한 확장 모델 목록\n");
    printf("  load <name>       - 플러그인 로드\n");
    printf("  unload <name>     - 플러그인 언로드\n");
    printf("  enable <name>     - 플러그인 활성화\n");
    printf("  disable <name>    - 플러그인 비활성화\n");
    printf("  extension <name>  - 확장 모델 적용\n");
    printf("  chain             - 현재 효과 체인 표시\n");
    printf("  add_effect <name> - 효과를 체인에 추가\n");
    printf("  remove_effect <n> - 체인에서 효과 제거 (인덱스)\n");
    printf("  effects on/off    - 효과 체인 활성화/비활성화\n");
    printf("  info <name>       - 플러그인 상세 정보\n");
    printf("  test <text>       - 텍스트로 음성 합성 테스트\n");
    printf("  quit              - 프로그램 종료\n");
    printf("\n");
}

// 플러그인 스캔
static int scan_plugins(PluginDemo* demo) {
    DIR* dir;
    struct dirent* entry;
    char plugin_path[512];

    printf("플러그인 스캔 중: %s\n", demo->plugin_dir);

    dir = opendir(demo->plugin_dir);
    if (!dir) {
        fprintf(stderr, "플러그인 디렉토리를 열 수 없습니다: %s\n", demo->plugin_dir);
        return -1;
    }

    demo->num_plugins = 0;

    while ((entry = readdir(dir)) != NULL && demo->num_plugins < MAX_PLUGINS) {
        // .so, .dll, .dylib 파일만 처리
        char* ext = strrchr(entry->d_name, '.');
        if (!ext) continue;

        if (strcmp(ext, ".so") != 0 && strcmp(ext, ".dll") != 0 && strcmp(ext, ".dylib") != 0) {
            continue;
        }

        snprintf(plugin_path, sizeof(plugin_path), "%s/%s", demo->plugin_dir, entry->d_name);

        // 플러그인 정보 추출 (실제 구현에서는 플러그인에서 메타데이터 읽기)
        PluginInfo* info = &demo->plugins[demo->num_plugins];

        // 파일명에서 플러그인 이름 추출
        strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
        char* dot = strrchr(info->name, '.');
        if (dot) *dot = '\0';

        strcpy(info->version, "1.0.0");
        strcpy(info->author, "Unknown");

        // 플러그인 타입 추정 (이름 기반)
        if (strstr(info->name, "reverb") || strstr(info->name, "echo") ||
            strstr(info->name, "compressor") || strstr(info->name, "equalizer")) {
            info->type = PLUGIN_TYPE_AUDIO_EFFECT;
            strcpy(info->description, "오디오 효과 플러그인");
        } else if (strstr(info->name, "voice") || strstr(info->name, "speaker")) {
            info->type = PLUGIN_TYPE_VOICE_MODEL;
            strcpy(info->description, "음성 모델 플러그인");
        } else if (strstr(info->name, "lang") || strstr(info->name, "language")) {
            info->type = PLUGIN_TYPE_LANGUAGE_PACK;
            strcpy(info->description, "언어팩 플러그인");
        } else {
            info->type = PLUGIN_TYPE_CUSTOM_FILTER;
            strcpy(info->description, "사용자 정의 필터 플러그인");
        }

        info->loaded = false;
        info->enabled = false;
        info->handle = NULL;
        info->plugin_instance = NULL;

        demo->num_plugins++;
    }

    closedir(dir);

    printf("발견된 플러그인: %d개\n", demo->num_plugins);
    return 0;
}

// 확장 모델 스캔
static int scan_extensions(PluginDemo* demo) {
    DIR* dir;
    struct dirent* entry;
    char extension_path[512];

    printf("확장 모델 스캔 중: %s\n", demo->extension_dir);

    dir = opendir(demo->extension_dir);
    if (!dir) {
        fprintf(stderr, "확장 모델 디렉토리를 열 수 없습니다: %s\n", demo->extension_dir);
        return -1;
    }

    demo->num_extensions = 0;

    while ((entry = readdir(dir)) != NULL && demo->num_extensions < MAX_EXTENSIONS) {
        // .lefx 파일만 처리
        char* ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".lefx") != 0) {
            continue;
        }

        snprintf(extension_path, sizeof(extension_path), "%s/%s", demo->extension_dir, entry->d_name);

        ExtensionInfo* info = &demo->extensions[demo->num_extensions];

        // 파일명에서 확장 이름 추출
        strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
        char* dot = strrchr(info->name, '.');
        if (dot) *dot = '\0';

        strncpy(info->path, extension_path, sizeof(info->path) - 1);

        // 확장 모델 정보 추정 (실제로는 LEFX 헤더에서 읽기)
        if (strstr(info->name, "female")) {
            strcpy(info->base_model, "base_female");
            strcpy(info->description, "여성 음성 확장 모델");
        } else if (strstr(info->name, "male")) {
            strcpy(info->base_model, "base_male");
            strcpy(info->description, "남성 음성 확장 모델");
        } else if (strstr(info->name, "child")) {
            strcpy(info->base_model, "base_child");
            strcpy(info->description, "아동 음성 확장 모델");
        } else {
            strcpy(info->base_model, "base_default");
            strcpy(info->description, "기본 확장 모델");
        }

        info->loaded = false;
        info->active = false;
        info->model_data = NULL;

        demo->num_extensions++;
    }

    closedir(dir);

    printf("발견된 확장 모델: %d개\n", demo->num_extensions);
    return 0;
}

// 플러그인 목록 출력
static void print_plugins(PluginDemo* demo) {
    printf("\n=== 사용 가능한 플러그인 ===\n");

    if (demo->num_plugins == 0) {
        printf("플러그인이 없습니다. 'scan' 명령어로 스캔하세요.\n");
        return;
    }

    const char* type_names[] = {"오디오 효과", "음성 모델", "언어팩", "사용자 정의"};

    for (int i = 0; i < demo->num_plugins; i++) {
        PluginInfo* info = &demo->plugins[i];
        char status[32] = "";

        if (info->loaded && info->enabled) {
            strcpy(status, "[로드됨, 활성화]");
        } else if (info->loaded) {
            strcpy(status, "[로드됨, 비활성화]");
        } else {
            strcpy(status, "[언로드됨]");
        }

        printf("%2d. %s %s\n", i + 1, info->name, status);
        printf("    타입: %s\n", type_names[info->type]);
        printf("    버전: %s, 제작자: %s\n", info->version, info->author);
        printf("    설명: %s\n", info->description);
        printf("\n");
    }
}

// 확장 모델 목록 출력
static void print_extensions(PluginDemo* demo) {
    printf("\n=== 사용 가능한 확장 모델 ===\n");

    if (demo->num_extensions == 0) {
        printf("확장 모델이 없습니다. 'scan' 명령어로 스캔하세요.\n");
        return;
    }

    for (int i = 0; i < demo->num_extensions; i++) {
        ExtensionInfo* info = &demo->extensions[i];
        char status[32] = "";

        if (info->active) {
            strcpy(status, "[활성화]");
        } else if (info->loaded) {
            strcpy(status, "[로드됨]");
        } else {
            strcpy(status, "[언로드됨]");
        }

        printf("%2d. %s %s\n", i + 1, info->name, status);
        printf("    기본 모델: %s\n", info->base_model);
        printf("    경로: %s\n", info->path);
        printf("    설명: %s\n", info->description);
        printf("\n");
    }
}

// 플러그인 찾기
static PluginInfo* find_plugin(PluginDemo* demo, const char* name) {
    for (int i = 0; i < demo->num_plugins; i++) {
        if (strcmp(demo->plugins[i].name, name) == 0) {
            return &demo->plugins[i];
        }
    }
    return NULL;
}

// 확장 모델 찾기
static ExtensionInfo* find_extension(PluginDemo* demo, const char* name) {
    for (int i = 0; i < demo->num_extensions; i++) {
        if (strcmp(demo->extensions[i].name, name) == 0) {
            return &demo->extensions[i];
        }
    }
    return NULL;
}

// 플러그인 로드
static int load_plugin(PluginDemo* demo, const char* name) {
    PluginInfo* info = find_plugin(demo, name);
    if (!info) {
        printf("플러그인을 찾을 수 없습니다: %s\n", name);
        return -1;
    }

    if (info->loaded) {
        printf("플러그인이 이미 로드되어 있습니다: %s\n", name);
        return 0;
    }

    printf("플러그인 로드 중: %s\n", name);

    // 동적 라이브러리 로드 (시뮬레이션)
    char plugin_path[512];
    snprintf(plugin_path, sizeof(plugin_path), "%s/%s.so", demo->plugin_dir, name);

    // 실제 구현에서는 dlopen 사용
    // info->handle = dlopen(plugin_path, RTLD_LAZY);
    // if (!info->handle) {
    //     fprintf(stderr, "플러그인 로드 실패: %s\n", dlerror());
    //     return -1;
    // }

    // 플러그인 인스턴스 생성 (시뮬레이션)
    info->plugin_instance = plugin_create(name, info->type);
    if (!info->plugin_instance) {
        printf("플러그인 인스턴스 생성 실패: %s\n", name);
        return -1;
    }

    // 의존성 확인
    if (plugin_check_dependencies(info->plugin_instance) != 0) {
        printf("경고: 플러그인 의존성 문제가 있을 수 있습니다: %s\n", name);
    }

    info->loaded = true;
    printf("플러그인 로드 완료: %s\n", name);

    return 0;
}

// 플러그인 언로드
static int unload_plugin(PluginDemo* demo, const char* name) {
    PluginInfo* info = find_plugin(demo, name);
    if (!info) {
        printf("플러그인을 찾을 수 없습니다: %s\n", name);
        return -1;
    }

    if (!info->loaded) {
        printf("플러그인이 로드되어 있지 않습니다: %s\n", name);
        return 0;
    }

    printf("플러그인 언로드 중: %s\n", name);

    // 효과 체인에서 제거
    for (int i = 0; i < demo->effect_chain_length; i++) {
        if (demo->effect_chain[i].plugin == info) {
            // 체인에서 제거
            for (int j = i; j < demo->effect_chain_length - 1; j++) {
                demo->effect_chain[j] = demo->effect_chain[j + 1];
            }
            demo->effect_chain_length--;
            break;
        }
    }

    // 플러그인 인스턴스 해제
    if (info->plugin_instance) {
        plugin_destroy(info->plugin_instance);
        info->plugin_instance = NULL;
    }

    // 동적 라이브러리 언로드
    if (info->handle) {
        // dlclose(info->handle);
        info->handle = NULL;
    }

    info->loaded = false;
    info->enabled = false;

    printf("플러그인 언로드 완료: %s\n", name);
    return 0;
}

// 플러그인 활성화/비활성화
static int toggle_plugin(PluginDemo* demo, const char* name, bool enable) {
    PluginInfo* info = find_plugin(demo, name);
    if (!info) {
        printf("플러그인을 찾을 수 없습니다: %s\n", name);
        return -1;
    }

    if (!info->loaded) {
        printf("플러그인이 로드되어 있지 않습니다: %s\n", name);
        return -1;
    }

    info->enabled = enable;
    printf("플러그인 %s: %s\n", name, enable ? "활성화" : "비활성화");

    return 0;
}

// 확장 모델 적용
static int apply_extension(PluginDemo* demo, const char* name) {
    ExtensionInfo* info = find_extension(demo, name);
    if (!info) {
        printf("확장 모델을 찾을 수 없습니다: %s\n", name);
        return -1;
    }

    printf("확장 모델 적용 중: %s\n", name);

    // 기존 활성화된 확장 비활성화
    for (int i = 0; i < demo->num_extensions; i++) {
        demo->extensions[i].active = false;
    }

    // 확장 모델 로드 (실제 구현에서는 LEFX 파일 로드)
    if (!info->loaded) {
        printf("확장 모델 로드 중: %s\n", info->path);

        // LEF 확장 모델 로드 시뮬레이션
        info->model_data = malloc(1024);  // 실제로는 모델 데이터
        if (!info->model_data) {
            printf("확장 모델 로드 실패: 메모리 부족\n");
            return -1;
        }

        info->loaded = true;
    }

    // 확장 모델 활성화
    info->active = true;
    strcpy(demo->current_extension, name);

    printf("확장 모델 적용 완료: %s\n", name);
    printf("기본 모델: %s\n", info->base_model);

    return 0;
}

// 효과 체인 출력
static void print_effect_chain(PluginDemo* demo) {
    printf("\n=== 현재 효과 체인 ===\n");
    printf("효과 체인 상태: %s\n", demo->effects_enabled ? "활성화" : "비활성화");

    if (demo->effect_chain_length == 0) {
        printf("효과 체인이 비어있습니다.\n");
        return;
    }

    for (int i = 0; i < demo->effect_chain_length; i++) {
        EffectChainItem* item = &demo->effect_chain[i];
        printf("%d. %s %s\n", i + 1, item->plugin->name,
               item->enabled ? "[활성화]" : "[비활성화]");
    }
    printf("\n");
}

// 효과 체인에 추가
static int add_effect_to_chain(PluginDemo* demo, const char* name) {
    PluginInfo* info = find_plugin(demo, name);
    if (!info) {
        printf("플러그인을 찾을 수 없습니다: %s\n", name);
        return -1;
    }

    if (!info->loaded) {
        printf("플러그인이 로드되어 있지 않습니다: %s\n", name);
        return -1;
    }

    if (info->type != PLUGIN_TYPE_AUDIO_EFFECT) {
        printf("오디오 효과 플러그인이 아닙니다: %s\n", name);
        return -1;
    }

    if (demo->effect_chain_length >= MAX_EFFECTS_CHAIN) {
        printf("효과 체인이 가득 찼습니다 (최대 %d개)\n", MAX_EFFECTS_CHAIN);
        return -1;
    }

    // 이미 체인에 있는지 확인
    for (int i = 0; i < demo->effect_chain_length; i++) {
        if (demo->effect_chain[i].plugin == info) {
            printf("플러그인이 이미 효과 체인에 있습니다: %s\n", name);
            return -1;
        }
    }

    EffectChainItem* item = &demo->effect_chain[demo->effect_chain_length];
    item->plugin = info;
    item->effect_params = NULL;  // 실제로는 효과 파라미터 설정
    item->enabled = true;

    demo->effect_chain_length++;

    printf("효과를 체인에 추가했습니다: %s (위치: %d)\n", name, demo->effect_chain_length);

    return 0;
}

// 효과 체인에서 제거
static int remove_effect_from_chain(PluginDemo* demo, int index) {
    if (index < 1 || index > demo->effect_chain_length) {
        printf("잘못된 인덱스입니다: %d (1-%d 범위)\n", index, demo->effect_chain_length);
        return -1;
    }

    int idx = index - 1;  // 0 기반 인덱스로 변환

    printf("효과를 체인에서 제거합니다: %s\n", demo->effect_chain[idx].plugin->name);

    // 체인에서 제거
    for (int i = idx; i < demo->effect_chain_length - 1; i++) {
        demo->effect_chain[i] = demo->effect_chain[i + 1];
    }

    demo->effect_chain_length--;

    printf("효과 제거 완료\n");
    return 0;
}

// 플러그인 상세 정보 출력
static void print_plugin_info(PluginDemo* demo, const char* name) {
    PluginInfo* info = find_plugin(demo, name);
    if (!info) {
        printf("플러그인을 찾을 수 없습니다: %s\n", name);
        return;
    }

    const char* type_names[] = {"오디오 효과", "음성 모델", "언어팩", "사용자 정의"};

    printf("\n=== 플러그인 상세 정보 ===\n");
    printf("이름: %s\n", info->name);
    printf("버전: %s\n", info->version);
    printf("제작자: %s\n", info->author);
    printf("타입: %s\n", type_names[info->type]);
    printf("설명: %s\n", info->description);
    printf("상태: %s\n", info->loaded ? (info->enabled ? "로드됨, 활성화" : "로드됨, 비활성화") : "언로드됨");

    if (info->loaded && info->plugin_instance) {
        // 플러그인 추가 정보 (실제 구현에서는 플러그인 API 호출)
        printf("메모리 사용량: 약 1.2 MB\n");
        printf("CPU 사용률: 약 2.5%%\n");
        printf("지원 샘플 레이트: 22050, 44100, 48000 Hz\n");

        if (info->type == PLUGIN_TYPE_AUDIO_EFFECT) {
            printf("효과 파라미터:\n");
            printf("  - 강도: 0.0 - 1.0 (기본값: 0.5)\n");
            printf("  - 주파수: 20 - 20000 Hz (기본값: 1000)\n");
            printf("  - 게인: -20 - +20 dB (기본값: 0)\n");
        }
    }

    printf("\n");
}

// 음성 합성 테스트
static int test_synthesis(PluginDemo* demo, const char* text) {
    if (!demo->engine) {
        printf("TTS 엔진이 초기화되지 않았습니다.\n");
        return -1;
    }

    printf("음성 합성 테스트: \"%s\"\n", text);

    if (strlen(demo->current_extension) > 0) {
        printf("사용 중인 확장 모델: %s\n", demo->current_extension);
    }

    if (demo->effects_enabled && demo->effect_chain_length > 0) {
        printf("적용될 효과 체인:\n");
        for (int i = 0; i < demo->effect_chain_length; i++) {
            if (demo->effect_chain[i].enabled) {
                printf("  %d. %s\n", i + 1, demo->effect_chain[i].plugin->name);
            }
        }
    }

    clock_t start_time = clock();

    // 음성 합성 수행
    float audio_buffer[MAX_AUDIO_LENGTH];
    int audio_length = MAX_AUDIO_LENGTH;

    int result = libetude_synthesize_text(demo->engine, text, audio_buffer, &audio_length);

    if (result != 0) {
        printf("음성 합성 실패: %d\n", result);
        return -1;
    }

    // 효과 체인 적용 (시뮬레이션)
    if (demo->effects_enabled) {
        for (int i = 0; i < demo->effect_chain_length; i++) {
            if (demo->effect_chain[i].enabled) {
                printf("효과 적용 중: %s\n", demo->effect_chain[i].plugin->name);

                // 실제 구현에서는 플러그인의 process 함수 호출
                // plugin_process_audio(demo->effect_chain[i].plugin->plugin_instance,
                //                      audio_buffer, audio_length);

                // 시뮬레이션을 위한 간단한 처리
                usleep(10000);  // 10ms 처리 시간 시뮬레이션
            }
        }
    }

    clock_t end_time = clock();
    double synthesis_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    printf("음성 합성 완료:\n");
    printf("  - 합성 시간: %.2f ms\n", synthesis_time);
    printf("  - 오디오 길이: %.2f 초\n", (float)audio_length / 22050.0f);
    printf("  - 적용된 효과: %d개\n", demo->effects_enabled ? demo->effect_chain_length : 0);

    // 오디오 재생 시뮬레이션
    printf("오디오 재생 중...\n");
    float playback_duration = (float)audio_length / 22050.0f;
    usleep((int)(playback_duration * 1000000));
    printf("재생 완료\n\n");

    return 0;
}

// 명령어 처리
static int process_command(PluginDemo* demo, const char* input) {
    char command[256];
    char args[768];

    if (sscanf(input, "%255s %767[^\n]", command, args) < 1) {
        return 0;
    }

    if (strcmp(command, "help") == 0) {
        print_help();
    }
    else if (strcmp(command, "scan") == 0) {
        scan_plugins(demo);
        scan_extensions(demo);
    }
    else if (strcmp(command, "plugins") == 0) {
        print_plugins(demo);
    }
    else if (strcmp(command, "extensions") == 0) {
        print_extensions(demo);
    }
    else if (strcmp(command, "load") == 0) {
        if (strlen(args) > 0) {
            load_plugin(demo, args);
        } else {
            printf("사용법: load <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "unload") == 0) {
        if (strlen(args) > 0) {
            unload_plugin(demo, args);
        } else {
            printf("사용법: unload <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "enable") == 0) {
        if (strlen(args) > 0) {
            toggle_plugin(demo, args, true);
        } else {
            printf("사용법: enable <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "disable") == 0) {
        if (strlen(args) > 0) {
            toggle_plugin(demo, args, false);
        } else {
            printf("사용법: disable <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "extension") == 0) {
        if (strlen(args) > 0) {
            apply_extension(demo, args);
        } else {
            printf("사용법: extension <확장 모델 이름>\n");
        }
    }
    else if (strcmp(command, "chain") == 0) {
        print_effect_chain(demo);
    }
    else if (strcmp(command, "add_effect") == 0) {
        if (strlen(args) > 0) {
            add_effect_to_chain(demo, args);
        } else {
            printf("사용법: add_effect <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "remove_effect") == 0) {
        int index;
        if (sscanf(args, "%d", &index) == 1) {
            remove_effect_from_chain(demo, index);
        } else {
            printf("사용법: remove_effect <인덱스>\n");
        }
    }
    else if (strcmp(command, "effects") == 0) {
        if (strcmp(args, "on") == 0) {
            demo->effects_enabled = true;
            printf("효과 체인 활성화\n");
        } else if (strcmp(args, "off") == 0) {
            demo->effects_enabled = false;
            printf("효과 체인 비활성화\n");
        } else {
            printf("사용법: effects on/off\n");
        }
    }
    else if (strcmp(command, "info") == 0) {
        if (strlen(args) > 0) {
            print_plugin_info(demo, args);
        } else {
            printf("사용법: info <플러그인 이름>\n");
        }
    }
    else if (strcmp(command, "test") == 0) {
        if (strlen(args) > 0) {
            test_synthesis(demo, args);
        } else {
            printf("사용법: test <텍스트>\n");
        }
    }
    else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
        return -1;
    }
    else {
        printf("알 수 없는 명령어: %s\n", command);
        printf("'help' 명령어로 사용법을 확인하세요.\n");
    }

    return 0;
}

// 데모 초기화
static int init_plugin_demo(PluginDemo* demo, const char* model_path) {
    printf("플러그인 데모 초기화 중...\n");

    memset(demo, 0, sizeof(PluginDemo));

    // 기본 경로 설정
    strcpy(demo->plugin_dir, "plugins");
    strcpy(demo->extension_dir, "extensions");
    strcpy(demo->current_base_model, model_path);
    demo->effects_enabled = true;

    // TTS 엔진 생성
    demo->engine = libetude_create_engine(model_path);
    if (!demo->engine) {
        fprintf(stderr, "TTS 엔진 생성 실패\n");
        return -1;
    }

    printf("플러그인 데모 초기화 완료\n");
    return 0;
}

// 데모 정리
static void cleanup_plugin_demo(PluginDemo* demo) {
    printf("플러그인 데모 정리 중...\n");

    // 모든 플러그인 언로드
    for (int i = 0; i < demo->num_plugins; i++) {
        if (demo->plugins[i].loaded) {
            unload_plugin(demo, demo->plugins[i].name);
        }
    }

    // 모든 확장 모델 언로드
    for (int i = 0; i < demo->num_extensions; i++) {
        if (demo->extensions[i].loaded && demo->extensions[i].model_data) {
            free(demo->extensions[i].model_data);
            demo->extensions[i].model_data = NULL;
        }
    }

    // TTS 엔진 해제
    if (demo->engine) {
        libetude_destroy_engine(demo->engine);
        demo->engine = NULL;
    }

    printf("플러그인 데모 정리 완료\n");
}

// 메인 함수
int main(int argc, char* argv[]) {
    PluginDemo demo;
    char input[MAX_TEXT_LENGTH];
    const char* model_path = "models/default.lef";

    printf("=== LibEtude 플러그인 확장 데모 ===\n");
    printf("버전: 1.0.0\n");
    printf("빌드 날짜: %s %s\n", __DATE__, __TIME__);
    printf("\n");

    // 명령행 인수 처리
    if (argc > 1) {
        model_path = argv[1];
    }

    printf("기본 모델: %s\n", model_path);

    // 데모 초기화
    if (init_plugin_demo(&demo, model_path) != 0) {
        fprintf(stderr, "데모 초기화 실패\n");
        return 1;
    }

    // 초기 스캔
    printf("\n초기 스캔을 수행합니다...\n");
    scan_plugins(&demo);
    scan_extensions(&demo);

    // 도움말 출력
    print_help();

    // 메인 루프
    printf("명령어를 입력하세요 ('help'로 도움말 확인):\n");
    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        // 개행 문자 제거
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) {
            continue;
        }

        // 명령어 처리
        if (process_command(&demo, input) < 0) {
            break;
        }
    }

    printf("\n프로그램을 종료합니다.\n");

    // 정리
    cleanup_plugin_demo(&demo);

    return 0;
}