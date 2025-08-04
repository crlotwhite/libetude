/**
 * @file types.h
 * @brief LibEtude 공통 타입 정의
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude에서 사용하는 공통 데이터 타입과 상수를 정의합니다.
 */

#ifndef LIBETUDE_TYPES_H
#define LIBETUDE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 오류 코드 정의
// ============================================================================

/**
 * @brief LibEtude 오류 코드
 */
typedef enum {
    LIBETUDE_SUCCESS = 0,                    /**< 성공 */
    LIBETUDE_ERROR_INVALID_ARGUMENT = -1,    /**< 잘못된 인수 */
    LIBETUDE_ERROR_OUT_OF_MEMORY = -2,       /**< 메모리 부족 */
    LIBETUDE_ERROR_IO = -3,                  /**< 입출력 오류 */
    LIBETUDE_ERROR_NOT_IMPLEMENTED = -4,     /**< 구현되지 않음 */
    LIBETUDE_ERROR_RUNTIME = -5,             /**< 런타임 오류 */
    LIBETUDE_ERROR_HARDWARE = -6,            /**< 하드웨어 오류 */
    LIBETUDE_ERROR_MODEL = -7,               /**< 모델 오류 */
    LIBETUDE_ERROR_TIMEOUT = -8,             /**< 시간 초과 */
    LIBETUDE_ERROR_NOT_INITIALIZED = -9,     /**< 초기화되지 않음 */
    LIBETUDE_ERROR_ALREADY_INITIALIZED = -10, /**< 이미 초기화됨 */
    LIBETUDE_ERROR_UNSUPPORTED = -11,        /**< 지원되지 않음 */
    LIBETUDE_ERROR_NOT_FOUND = -12,          /**< 찾을 수 없음 */
    LIBETUDE_ERROR_INVALID_STATE = -13,      /**< 잘못된 상태 */
    LIBETUDE_ERROR_BUFFER_FULL = -14,        /**< 버퍼 가득 참 */
    LIBETUDE_ERROR_SYSTEM = -15,             /**< 시스템 오류 */
    ET_ERROR_THREAD = -16,                   /**< 스레드 관련 오류 */
    ET_ERROR_AUDIO = -17,                    /**< 오디오 관련 오류 */
    ET_ERROR_COMPRESSION = -18,              /**< 압축 관련 오류 */
    ET_ERROR_QUANTIZATION = -19,             /**< 양자화 관련 오류 */
    ET_ERROR_GRAPH = -20,                    /**< 그래프 관련 오류 */
    ET_ERROR_KERNEL = -21,                   /**< 커널 관련 오류 */
    ET_ERROR_UNKNOWN = -999                  /**< 알 수 없는 오류 */
} LibEtudeErrorCode;

/**
 * @brief 결과 타입 (오류 코드와 동일)
 */
typedef LibEtudeErrorCode ETResult;
typedef LibEtudeErrorCode LibEtudeResult;

// 편의를 위한 별칭
#define ET_SUCCESS LIBETUDE_SUCCESS
#define ET_ERROR_INVALID_ARGUMENT LIBETUDE_ERROR_INVALID_ARGUMENT
#define ET_ERROR_OUT_OF_MEMORY LIBETUDE_ERROR_OUT_OF_MEMORY
#define ET_ERROR_IO LIBETUDE_ERROR_IO
#define ET_ERROR_NOT_IMPLEMENTED LIBETUDE_ERROR_NOT_IMPLEMENTED
#define ET_ERROR_RUNTIME LIBETUDE_ERROR_RUNTIME
#define ET_ERROR_HARDWARE LIBETUDE_ERROR_HARDWARE
#define ET_ERROR_MODEL LIBETUDE_ERROR_MODEL
#define ET_ERROR_TIMEOUT LIBETUDE_ERROR_TIMEOUT
#define ET_ERROR_NOT_INITIALIZED LIBETUDE_ERROR_NOT_INITIALIZED
#define ET_ERROR_ALREADY_INITIALIZED LIBETUDE_ERROR_ALREADY_INITIALIZED
#define ET_ERROR_UNSUPPORTED LIBETUDE_ERROR_UNSUPPORTED
#define ET_ERROR_NOT_FOUND LIBETUDE_ERROR_NOT_FOUND
#define ET_ERROR_INVALID_STATE LIBETUDE_ERROR_INVALID_STATE
#define ET_ERROR_BUFFER_FULL LIBETUDE_ERROR_BUFFER_FULL

// ============================================================================
// 데이터 타입 정의
// ============================================================================

/**
 * @brief 데이터 타입 열거형
 */
typedef enum {
    LIBETUDE_DTYPE_FLOAT32 = 0,  /**< 32비트 부동소수점 */
    LIBETUDE_DTYPE_FLOAT16 = 1,  /**< 16비트 부동소수점 */
    LIBETUDE_DTYPE_BFLOAT16 = 2, /**< BFloat16 */
    LIBETUDE_DTYPE_INT8 = 3,     /**< 8비트 정수 */
    LIBETUDE_DTYPE_INT4 = 4,     /**< 4비트 정수 */
    LIBETUDE_DTYPE_UINT8 = 5,    /**< 8비트 부호없는 정수 */
    LIBETUDE_DTYPE_INT32 = 6     /**< 32비트 정수 */
} LibEtudeDataType;

/**
 * @brief 메모리 타입 열거형
 */
typedef enum {
    LIBETUDE_MEM_CPU = 0,     /**< CPU 메모리 */
    LIBETUDE_MEM_GPU = 1,     /**< GPU 메모리 */
    LIBETUDE_MEM_SHARED = 2   /**< 공유 메모리 */
} LibEtudeMemoryType;

// ============================================================================
// SIMD 기능 플래그
// ============================================================================

/**
 * @brief SIMD 기능 플래그
 */
typedef enum {
    LIBETUDE_SIMD_NONE = 0,        /**< SIMD 지원 없음 */
    LIBETUDE_SIMD_SSE = 1 << 0,    /**< SSE 지원 */
    LIBETUDE_SIMD_SSE2 = 1 << 1,   /**< SSE2 지원 */
    LIBETUDE_SIMD_SSE3 = 1 << 2,   /**< SSE3 지원 */
    LIBETUDE_SIMD_SSSE3 = 1 << 3,  /**< SSSE3 지원 */
    LIBETUDE_SIMD_SSE4_1 = 1 << 4, /**< SSE4.1 지원 */
    LIBETUDE_SIMD_SSE4_2 = 1 << 5, /**< SSE4.2 지원 */
    LIBETUDE_SIMD_AVX = 1 << 6,    /**< AVX 지원 */
    LIBETUDE_SIMD_AVX2 = 1 << 7,   /**< AVX2 지원 */
    LIBETUDE_SIMD_NEON = 1 << 8    /**< ARM NEON 지원 */
} LibEtudeSIMDFeatures;

// ============================================================================
// GPU 백엔드 타입
// ============================================================================

/**
 * @brief GPU 백엔드 타입
 */
typedef enum {
    LIBETUDE_GPU_NONE = 0,    /**< GPU 사용 안함 */
    LIBETUDE_GPU_CUDA = 1,    /**< NVIDIA CUDA */
    LIBETUDE_GPU_OPENCL = 2,  /**< OpenCL */
    LIBETUDE_GPU_METAL = 3    /**< Apple Metal */
} LibEtudeGPUBackend;

// ============================================================================
// 오디오 관련 타입
// ============================================================================

/**
 * @brief 오디오 포맷 구조체
 */
typedef struct {
    uint32_t sample_rate;    /**< 샘플링 레이트 (Hz) */
    uint16_t bit_depth;      /**< 비트 깊이 */
    uint16_t num_channels;   /**< 채널 수 */
    uint32_t frame_size;     /**< 프레임 크기 */
    uint32_t buffer_size;    /**< 버퍼 크기 */
} LibEtudeAudioFormat;

// ============================================================================
// 텐서 관련 타입
// ============================================================================

/**
 * @brief 텐서 구조체 (불투명한 포인터)
 */
typedef struct LibEtudeTensor LibEtudeTensor;

/**
 * @brief 텐서 형태 정보
 */
typedef struct {
    size_t* shape;      /**< 각 차원의 크기 */
    size_t* strides;    /**< 각 차원의 스트라이드 */
    size_t ndim;        /**< 차원 수 */
    size_t size;        /**< 총 요소 수 */
} LibEtudeTensorShape;

// ============================================================================
// 모델 관련 타입
// ============================================================================

/**
 * @brief 모델 메타데이터
 */
typedef struct {
    char model_name[64];        /**< 모델 이름 */
    char model_version[16];     /**< 모델 버전 */
    char author[32];            /**< 제작자 */
    char description[128];      /**< 설명 */

    uint16_t input_dim;         /**< 입력 차원 */
    uint16_t output_dim;        /**< 출력 차원 */
    uint16_t hidden_dim;        /**< 은닉 차원 */
    uint16_t num_layers;        /**< 레이어 수 */

    uint16_t sample_rate;       /**< 샘플링 레이트 */
    uint16_t mel_channels;      /**< Mel 채널 수 */
    uint16_t hop_length;        /**< Hop 길이 */
    uint16_t win_length;        /**< 윈도우 길이 */
} LibEtudeModelMeta;

// ============================================================================
// 시스템 정보 관련 타입
// ============================================================================

/**
 * @brief 시스템 정보 구조체
 */
typedef struct {
    uint64_t total_memory;      /**< 총 메모리 크기 (바이트) */
    uint64_t available_memory;  /**< 사용 가능한 메모리 크기 (바이트) */
    uint32_t cpu_count;         /**< CPU 코어 수 */
    char system_name[64];       /**< 시스템 이름 */
} LibEtudeSystemInfo;

// ============================================================================
// 로그 관련 타입
// ============================================================================

/**
 * @brief 로그 레벨
 */
typedef enum {
    LIBETUDE_LOG_DEBUG = 0,    /**< 디버그 */
    LIBETUDE_LOG_INFO = 1,     /**< 정보 */
    LIBETUDE_LOG_WARNING = 2,  /**< 경고 */
    LIBETUDE_LOG_ERROR = 3,    /**< 오류 */
    LIBETUDE_LOG_FATAL = 4     /**< 치명적 오류 */
} LibEtudeLogLevel;

/**
 * @brief 로그 콜백 함수 타입
 */
typedef void (*LibEtudeLogCallback)(LibEtudeLogLevel level, const char* message, void* user_data);

// ============================================================================
// 상수 정의
// ============================================================================

/** 최대 텐서 차원 수 */
#define LIBETUDE_MAX_TENSOR_DIMS 8

/** 최대 모델 이름 길이 */
#define LIBETUDE_MAX_MODEL_NAME_LEN 64

/** 최대 오류 메시지 길이 */
#define LIBETUDE_MAX_ERROR_MESSAGE_LEN 256

/** 기본 샘플링 레이트 */
#define LIBETUDE_DEFAULT_SAMPLE_RATE 22050

/** 기본 Mel 채널 수 */
#define LIBETUDE_DEFAULT_MEL_CHANNELS 80

/** 기본 Hop 길이 */
#define LIBETUDE_DEFAULT_HOP_LENGTH 256

/** 기본 윈도우 길이 */
#define LIBETUDE_DEFAULT_WIN_LENGTH 1024

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_TYPES_H