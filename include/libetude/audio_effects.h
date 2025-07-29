#ifndef LIBETUDE_AUDIO_EFFECTS_H
#define LIBETUDE_AUDIO_EFFECTS_H

#include "plugin.h"
#include "types.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// 오디오 효과 타입 정의
typedef enum {
    AUDIO_EFFECT_REVERB = 0,         // 리버브 효과
    AUDIO_EFFECT_EQUALIZER = 1,      // 이퀄라이저 효과
    AUDIO_EFFECT_DELAY = 2,          // 딜레이 효과
    AUDIO_EFFECT_CHORUS = 3,         // 코러스 효과
    AUDIO_EFFECT_COMPRESSOR = 4,     // 컴프레서 효과
    AUDIO_EFFECT_DISTORTION = 5,     // 디스토션 효과
    AUDIO_EFFECT_FILTER = 6,         // 필터 효과
    AUDIO_EFFECT_PITCH_SHIFT = 7,    // 피치 시프트 효과
    AUDIO_EFFECT_CUSTOM = 255        // 사용자 정의 효과
} AudioEffectType;

// 오디오 효과 품질 설정
typedef enum {
    AUDIO_QUALITY_LOW = 0,           // 낮은 품질 (빠른 처리)
    AUDIO_QUALITY_MEDIUM = 1,        // 중간 품질
    AUDIO_QUALITY_HIGH = 2,          // 높은 품질 (느린 처리)
    AUDIO_QUALITY_ULTRA = 3          // 최고 품질
} AudioQuality;

// 공통 오디오 효과 설정
typedef struct {
    float sample_rate;               // 샘플링 레이트
    int num_channels;                // 채널 수
    int buffer_size;                 // 버퍼 크기
    AudioQuality quality;            // 품질 설정
    bool bypass;                     // 바이패스 여부
    float wet_dry_mix;               // 웨트/드라이 믹스 (0.0 = 완전 드라이, 1.0 = 완전 웨트)
} AudioEffectConfig;

// 리버브 효과 파라미터
typedef struct {
    float room_size;                 // 룸 크기 (0.0 - 1.0)
    float damping;                   // 댐핑 (0.0 - 1.0)
    float pre_delay;                 // 프리 딜레이 (ms)
    float decay_time;                // 디케이 타임 (초)
    float early_reflections;         // 초기 반사음 레벨 (0.0 - 1.0)
    float late_reverb;               // 후기 리버브 레벨 (0.0 - 1.0)
    float high_cut;                  // 고주파 컷오프 (Hz)
    float low_cut;                   // 저주파 컷오프 (Hz)
} ReverbParams;

// 이퀄라이저 밴드 정보
typedef struct {
    float frequency;                 // 중심 주파수 (Hz)
    float gain;                      // 게인 (dB)
    float q_factor;                  // Q 팩터 (대역폭)
    bool enabled;                    // 밴드 활성화 여부
} EQBand;

// 이퀄라이저 효과 파라미터
typedef struct {
    EQBand* bands;                   // EQ 밴드 배열
    int num_bands;                   // 밴드 수
    float overall_gain;              // 전체 게인 (dB)
    bool auto_gain;                  // 자동 게인 보정
} EqualizerParams;

// 딜레이 효과 파라미터
typedef struct {
    float delay_time;                // 딜레이 시간 (ms)
    float feedback;                  // 피드백 (0.0 - 0.99)
    float high_cut;                  // 고주파 컷오프 (Hz)
    float low_cut;                   // 저주파 컷오프 (Hz)
    bool sync_to_tempo;              // 템포 동기화 여부
    float tempo_bpm;                 // BPM (템포 동기화 시 사용)
} DelayParams;

// 코러스 효과 파라미터
typedef struct {
    float rate;                      // LFO 레이트 (Hz)
    float depth;                     // 모듈레이션 깊이 (0.0 - 1.0)
    float delay_time;                // 기본 딜레이 시간 (ms)
    float feedback;                  // 피드백 (0.0 - 0.99)
    int num_voices;                  // 보이스 수 (1-8)
    float stereo_width;              // 스테레오 폭 (0.0 - 1.0)
} ChorusParams;

// 컴프레서 효과 파라미터
typedef struct {
    float threshold;                 // 임계값 (dB)
    float ratio;                     // 압축 비율 (1:1 - 20:1)
    float attack_time;               // 어택 타임 (ms)
    float release_time;              // 릴리즈 타임 (ms)
    float knee;                      // 니 (0.0 = 하드, 1.0 = 소프트)
    float makeup_gain;               // 메이크업 게인 (dB)
    bool auto_makeup;                // 자동 메이크업 게인
} CompressorParams;

// 필터 타입
typedef enum {
    FILTER_LOWPASS = 0,              // 로우패스 필터
    FILTER_HIGHPASS = 1,             // 하이패스 필터
    FILTER_BANDPASS = 2,             // 밴드패스 필터
    FILTER_BANDSTOP = 3,             // 밴드스톱 필터
    FILTER_ALLPASS = 4,              // 올패스 필터
    FILTER_PEAKING = 5,              // 피킹 필터
    FILTER_LOWSHELF = 6,             // 로우셸프 필터
    FILTER_HIGHSHELF = 7             // 하이셸프 필터
} FilterType;

// 필터 효과 파라미터
typedef struct {
    FilterType type;                 // 필터 타입
    float frequency;                 // 컷오프/중심 주파수 (Hz)
    float resonance;                 // 레조넌스/Q 팩터
    float gain;                      // 게인 (dB, 피킹/셸프 필터용)
    int order;                       // 필터 차수 (1-8)
} FilterParams;

// 오디오 효과 플러그인 생성 함수들
PluginInstance* create_reverb_plugin(const ReverbParams* params);
PluginInstance* create_equalizer_plugin(const EqualizerParams* params);
PluginInstance* create_delay_plugin(const DelayParams* params);
PluginInstance* create_chorus_plugin(const ChorusParams* params);
PluginInstance* create_compressor_plugin(const CompressorParams* params);
PluginInstance* create_filter_plugin(const FilterParams* params);

// 오디오 효과 파라미터 설정/조회 함수들
ETErrorCode set_reverb_params(PluginInstance* plugin, const ReverbParams* params);
ETErrorCode get_reverb_params(PluginInstance* plugin, ReverbParams* params);
ETErrorCode set_equalizer_params(PluginInstance* plugin, const EqualizerParams* params);
ETErrorCode get_equalizer_params(PluginInstance* plugin, EqualizerParams* params);
ETErrorCode set_delay_params(PluginInstance* plugin, const DelayParams* params);
ETErrorCode get_delay_params(PluginInstance* plugin, DelayParams* params);
ETErrorCode set_chorus_params(PluginInstance* plugin, const ChorusParams* params);
ETErrorCode get_chorus_params(PluginInstance* plugin, ChorusParams* params);
ETErrorCode set_compressor_params(PluginInstance* plugin, const CompressorParams* params);
ETErrorCode get_compressor_params(PluginInstance* plugin, CompressorParams* params);
ETErrorCode set_filter_params(PluginInstance* plugin, const FilterParams* params);
ETErrorCode get_filter_params(PluginInstance* plugin, FilterParams* params);

// 실시간 파라미터 조정 함수들
ETErrorCode set_effect_wet_dry_mix(PluginInstance* plugin, float mix);
ETErrorCode set_effect_bypass(PluginInstance* plugin, bool bypass);
ETErrorCode get_effect_latency(PluginInstance* plugin, int* latency_samples);
ETErrorCode get_effect_tail_time(PluginInstance* plugin, float* tail_time_seconds);

// 오디오 효과 파이프라인 관리
typedef struct AudioEffectPipeline AudioEffectPipeline;

AudioEffectPipeline* create_audio_effect_pipeline(int max_effects);
void destroy_audio_effect_pipeline(AudioEffectPipeline* pipeline);
ETErrorCode add_effect_to_pipeline(AudioEffectPipeline* pipeline, PluginInstance* effect);
ETErrorCode remove_effect_from_pipeline(AudioEffectPipeline* pipeline, PluginInstance* effect);
ETErrorCode process_audio_pipeline(AudioEffectPipeline* pipeline, const float* input, float* output, int num_samples);
ETErrorCode set_pipeline_bypass(AudioEffectPipeline* pipeline, bool bypass);
ETErrorCode reorder_pipeline_effects(AudioEffectPipeline* pipeline, int* new_order, int num_effects);

// 프리셋 관리
typedef struct {
    char name[64];                   // 프리셋 이름
    AudioEffectType effect_type;     // 효과 타입
    void* params;                    // 파라미터 데이터
    size_t params_size;              // 파라미터 크기
} AudioEffectPreset;

ETErrorCode save_effect_preset(PluginInstance* plugin, const char* name, AudioEffectPreset* preset);
ETErrorCode load_effect_preset(PluginInstance* plugin, const AudioEffectPreset* preset);
ETErrorCode export_preset_to_file(const AudioEffectPreset* preset, const char* filename);
ETErrorCode import_preset_from_file(AudioEffectPreset* preset, const char* filename);

// 실시간 분석 및 시각화 지원
typedef struct {
    float* spectrum;                 // 주파수 스펙트럼 (FFT 결과)
    int spectrum_size;               // 스펙트럼 크기
    float peak_level;                // 피크 레벨 (dB)
    float rms_level;                 // RMS 레벨 (dB)
    float gain_reduction;            // 게인 리덕션 (컴프레서용, dB)
} AudioAnalysisData;

ETErrorCode get_effect_analysis_data(PluginInstance* plugin, AudioAnalysisData* data);
ETErrorCode enable_effect_analysis(PluginInstance* plugin, bool enable);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_AUDIO_EFFECTS_H