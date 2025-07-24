/**
 * @file tensor.h
 * @brief LibEtude 텐서 엔진
 *
 * 이 파일은 LibEtude의 텐서 시스템을 정의합니다.
 * 다차원 데이터 처리와 메모리 최적화를 제공합니다.
 */

#ifndef LIBETUDE_TENSOR_H
#define LIBETUDE_TENSOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// 최대 텐서 차원 수
#define ET_MAX_TENSOR_DIMS 8

// 데이터 타입 열거형
typedef enum {
    ET_FLOAT32 = 0,    // 32비트 부동소수점
    ET_FLOAT16 = 1,    // 16비트 부동소수점
    ET_BFLOAT16 = 2,   // BFloat16 (Brain Float)
    ET_INT8 = 3,       // 8비트 정수
    ET_INT4 = 4,       // 4비트 정수 (패킹됨)
    ET_UINT8 = 5,      // 8비트 부호없는 정수
    ET_INT32 = 6,      // 32비트 정수
    ET_INT64 = 7       // 64비트 정수
} ETDataType;

// 텐서 구조체
typedef struct {
    void* data;                    // 실제 데이터 포인터
    size_t* shape;                 // 각 차원의 크기
    size_t* strides;               // 각 차원의 스트라이드 (바이트 단위)
    size_t ndim;                   // 차원 수
    size_t size;                   // 총 요소 수
    size_t data_size;              // 실제 데이터 크기 (바이트)
    ETDataType dtype;              // 데이터 타입
    ETMemoryType mem_type;         // 메모리 타입
    ETMemoryPool* pool;            // 할당된 메모리 풀

    // 메모리 레이아웃 최적화
    bool is_contiguous;            // 연속 메모리 여부
    bool owns_data;                // 데이터 소유권 여부

    // 참조 카운팅 (메모리 공유용)
    int ref_count;                 // 참조 카운트

    // 디버그 정보
    const char* name;              // 텐서 이름 (디버그용)
    uint32_t magic;                // 매직 넘버 (손상 감지용)
} ETTensor;

// 텐서 슬라이스 정보
typedef struct {
    size_t start;                  // 시작 인덱스
    size_t end;                    // 끝 인덱스 (exclusive)
    size_t step;                   // 스텝 크기
} ETSlice;

// 텐서 연산 옵션
typedef struct {
    bool inplace;                  // 인플레이스 연산 여부
    bool broadcast;                // 브로드캐스팅 허용 여부
    ETMemoryPool* output_pool;     // 출력 텐서용 메모리 풀
} ETTensorOpOptions;

// =============================================================================
// 데이터 타입 유틸리티 함수
// =============================================================================

/**
 * @brief 데이터 타입의 바이트 크기 반환
 * @param dtype 데이터 타입
 * @return 바이트 크기
 */
size_t et_dtype_size(ETDataType dtype);

/**
 * @brief 데이터 타입 이름 반환
 * @param dtype 데이터 타입
 * @return 데이터 타입 이름 문자열
 */
const char* et_dtype_name(ETDataType dtype);

/**
 * @brief 데이터 타입이 부동소수점인지 확인
 * @param dtype 데이터 타입
 * @return 부동소수점이면 true, 아니면 false
 */
bool et_dtype_is_float(ETDataType dtype);

/**
 * @brief 데이터 타입이 정수인지 확인
 * @param dtype 데이터 타입
 * @return 정수면 true, 아니면 false
 */
bool et_dtype_is_int(ETDataType dtype);

// =============================================================================
// 텐서 생성 및 소멸 함수
// =============================================================================

/**
 * @brief 텐서 생성
 * @param pool 메모리 풀
 * @param dtype 데이터 타입
 * @param ndim 차원 수
 * @param shape 각 차원의 크기 배열
 * @return 생성된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_create_tensor(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape);

/**
 * @brief 이름을 가진 텐서 생성
 * @param pool 메모리 풀
 * @param dtype 데이터 타입
 * @param ndim 차원 수
 * @param shape 각 차원의 크기 배열
 * @param name 텐서 이름
 * @return 생성된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_create_tensor_named(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape, const char* name);

/**
 * @brief 외부 데이터를 사용한 텐서 생성 (데이터 복사 없음)
 * @param data 외부 데이터 포인터
 * @param dtype 데이터 타입
 * @param ndim 차원 수
 * @param shape 각 차원의 크기 배열
 * @param strides 스트라이드 배열 (NULL이면 자동 계산)
 * @return 생성된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_create_tensor_from_data(void* data, ETDataType dtype, size_t ndim, const size_t* shape, const size_t* strides);

/**
 * @brief 0으로 초기화된 텐서 생성
 * @param pool 메모리 풀
 * @param dtype 데이터 타입
 * @param ndim 차원 수
 * @param shape 각 차원의 크기 배열
 * @return 생성된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_create_zeros(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape);

/**
 * @brief 1로 초기화된 텐서 생성
 * @param pool 메모리 풀
 * @param dtype 데이터 타입
 * @param ndim 차원 수
 * @param shape 각 차원의 크기 배열
 * @return 생성된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_create_ones(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape);

/**
 * @brief 텐서 복사 생성
 * @param src 원본 텐서
 * @param pool 메모리 풀 (NULL이면 원본과 동일한 풀 사용)
 * @return 복사된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_copy_tensor(const ETTensor* src, ETMemoryPool* pool);

/**
 * @brief 텐서 참조 증가
 * @param tensor 텐서
 * @return 참조된 텐서 포인터
 */
ETTensor* et_retain_tensor(ETTensor* tensor);

/**
 * @brief 텐서 소멸 (참조 카운트 감소)
 * @param tensor 텐서
 */
void et_destroy_tensor(ETTensor* tensor);

// =============================================================================
// 텐서 조작 함수
// =============================================================================

/**
 * @brief 텐서 모양 변경
 * @param tensor 텐서
 * @param ndim 새로운 차원 수
 * @param new_shape 새로운 모양 배열
 * @return 모양이 변경된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_reshape_tensor(ETTensor* tensor, size_t ndim, const size_t* new_shape);

/**
 * @brief 텐서 슬라이싱
 * @param tensor 텐서
 * @param slices 슬라이스 정보 배열
 * @return 슬라이스된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_slice_tensor(ETTensor* tensor, const ETSlice* slices);

/**
 * @brief 텐서 전치 (2D 텐서용)
 * @param tensor 텐서
 * @return 전치된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_transpose_tensor(ETTensor* tensor);

/**
 * @brief 텐서 차원 순서 변경
 * @param tensor 텐서
 * @param axes 새로운 축 순서 배열
 * @return 차원이 변경된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_permute_tensor(ETTensor* tensor, const size_t* axes);

/**
 * @brief 텐서 차원 확장 (크기 1인 차원 추가)
 * @param tensor 텐서
 * @param axis 확장할 축
 * @return 차원이 확장된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_expand_dims(ETTensor* tensor, int axis);

/**
 * @brief 텐서 차원 축소 (크기 1인 차원 제거)
 * @param tensor 텐서
 * @param axis 축소할 축 (-1이면 모든 크기 1인 차원 제거)
 * @return 차원이 축소된 텐서 포인터, 실패시 NULL
 */
ETTensor* et_squeeze_tensor(ETTensor* tensor, int axis);

// =============================================================================
// 텐서 정보 조회 함수
// =============================================================================

/**
 * @brief 텐서 유효성 검사
 * @param tensor 텐서
 * @return 유효하면 true, 아니면 false
 */
bool et_validate_tensor(const ETTensor* tensor);

/**
 * @brief 텐서가 연속 메모리인지 확인
 * @param tensor 텐서
 * @return 연속 메모리면 true, 아니면 false
 */
bool et_is_contiguous(const ETTensor* tensor);

/**
 * @brief 텐서를 연속 메모리로 변환
 * @param tensor 텐서
 * @param pool 메모리 풀 (NULL이면 원본과 동일한 풀 사용)
 * @return 연속 메모리 텐서 포인터, 실패시 NULL
 */
ETTensor* et_make_contiguous(ETTensor* tensor, ETMemoryPool* pool);

/**
 * @brief 텐서 모양이 같은지 확인
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @return 모양이 같으면 true, 아니면 false
 */
bool et_same_shape(const ETTensor* a, const ETTensor* b);

/**
 * @brief 텐서 브로드캐스팅 가능 여부 확인
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @return 브로드캐스팅 가능하면 true, 아니면 false
 */
bool et_can_broadcast(const ETTensor* a, const ETTensor* b);

/**
 * @brief 텐서 정보 출력
 * @param tensor 텐서
 */
void et_print_tensor_info(const ETTensor* tensor);

// =============================================================================
// 메모리 레이아웃 최적화 함수
// =============================================================================

/**
 * @brief 스트라이드 계산 (C-order, row-major)
 * @param shape 모양 배열
 * @param ndim 차원 수
 * @param dtype 데이터 타입
 * @param strides 계산된 스트라이드를 저장할 배열
 */
void et_compute_strides(const size_t* shape, size_t ndim, ETDataType dtype, size_t* strides);

/**
 * @brief 총 요소 수 계산
 * @param shape 모양 배열
 * @param ndim 차원 수
 * @return 총 요소 수
 */
size_t et_compute_size(const size_t* shape, size_t ndim);

/**
 * @brief 다차원 인덱스를 1차원 오프셋으로 변환
 * @param indices 다차원 인덱스 배열
 * @param strides 스트라이드 배열
 * @param ndim 차원 수
 * @return 1차원 오프셋
 */
size_t et_compute_offset(const size_t* indices, const size_t* strides, size_t ndim);

/**
 * @brief 1차원 오프셋을 다차원 인덱스로 변환
 * @param offset 1차원 오프셋
 * @param shape 모양 배열
 * @param ndim 차원 수
 * @param indices 계산된 다차원 인덱스를 저장할 배열
 */
void et_compute_indices(size_t offset, const size_t* shape, size_t ndim, size_t* indices);

// =============================================================================
// 텐서 데이터 접근 함수
// =============================================================================

/**
 * @brief 텐서 요소 값 가져오기 (float32로 변환)
 * @param tensor 텐서
 * @param indices 다차원 인덱스 배열
 * @return 요소 값
 */
float et_get_float(const ETTensor* tensor, const size_t* indices);

/**
 * @brief 텐서 요소 값 설정 (float32에서 변환)
 * @param tensor 텐서
 * @param indices 다차원 인덱스 배열
 * @param value 설정할 값
 */
void et_set_float(ETTensor* tensor, const size_t* indices, float value);

/**
 * @brief 텐서 요소 포인터 가져오기
 * @param tensor 텐서
 * @param indices 다차원 인덱스 배열
 * @return 요소 포인터
 */
void* et_get_ptr(const ETTensor* tensor, const size_t* indices);

/**
 * @brief 텐서 데이터 포인터 가져오기 (타입 캐스팅)
 * @param tensor 텐서
 * @param dtype 원하는 데이터 타입
 * @return 타입 캐스팅된 데이터 포인터, 실패시 NULL
 */
void* et_get_data_ptr(const ETTensor* tensor, ETDataType dtype);

// =============================================================================
// 텐서 초기화 함수
// =============================================================================

/**
 * @brief 텐서를 특정 값으로 채우기
 * @param tensor 텐서
 * @param value 채울 값
 */
void et_fill_tensor(ETTensor* tensor, float value);

/**
 * @brief 텐서를 0으로 초기화
 * @param tensor 텐서
 */
void et_zero_tensor(ETTensor* tensor);

/**
 * @brief 텐서를 랜덤 값으로 초기화 (균등 분포)
 * @param tensor 텐서
 * @param min_val 최소값
 * @param max_val 최대값
 */
void et_random_uniform(ETTensor* tensor, float min_val, float max_val);

/**
 * @brief 텐서를 랜덤 값으로 초기화 (정규 분포)
 * @param tensor 텐서
 * @param mean 평균
 * @param std 표준편차
 */
void et_random_normal(ETTensor* tensor, float mean, float std);

// =============================================================================
// 텐서 연산 함수
// =============================================================================

/**
 * @brief 텐서 덧셈
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_add(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 텐서 뺄셈
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_sub(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 텐서 곱셈 (요소별)
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_mul(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 텐서 나눗셈 (요소별)
 * @param a 첫 번째 텐서
 * @param b 두 번째 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_div(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 텐서와 스칼라 덧셈
 * @param tensor 텐서
 * @param scalar 스칼라 값
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_add_scalar(const ETTensor* tensor, float scalar, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 텐서와 스칼라 곱셈
 * @param tensor 텐서
 * @param scalar 스칼라 값
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_mul_scalar(const ETTensor* tensor, float scalar, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 행렬 곱셈
 * @param a 첫 번째 행렬 (2D 텐서)
 * @param b 두 번째 행렬 (2D 텐서)
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_matmul(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options);

/**
 * @brief 소프트맥스 함수
 * @param input 입력 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param axis 소프트맥스를 적용할 축
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_softmax(const ETTensor* input, ETTensor* out, int axis, const ETTensorOpOptions* options);

/**
 * @brief 텐서 합계
 * @param input 입력 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param axis 합계를 계산할 축 (-1이면 전체 합계)
 * @param keepdims 차원 유지 여부
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_sum(const ETTensor* input, ETTensor* out, int axis, bool keepdims, const ETTensorOpOptions* options);

/**
 * @brief 텐서 평균
 * @param input 입력 텐서
 * @param out 출력 텐서 (NULL이면 새로 생성)
 * @param axis 평균을 계산할 축 (-1이면 전체 평균)
 * @param keepdims 차원 유지 여부
 * @param options 연산 옵션
 * @return 결과 텐서, 실패시 NULL
 */
ETTensor* et_mean(const ETTensor* input, ETTensor* out, int axis, bool keepdims, const ETTensorOpOptions* options);

// =============================================================================
// 인플레이스 연산 함수
// =============================================================================

/**
 * @brief 인플레이스 텐서 덧셈
 * @param a 첫 번째 텐서 (결과도 여기에 저장)
 * @param b 두 번째 텐서
 * @return 결과 텐서 (a와 동일), 실패시 NULL
 */
ETTensor* et_add_inplace(ETTensor* a, const ETTensor* b);

/**
 * @brief 인플레이스 텐서 곱셈
 * @param a 첫 번째 텐서 (결과도 여기에 저장)
 * @param b 두 번째 텐서
 * @return 결과 텐서 (a와 동일), 실패시 NULL
 */
ETTensor* et_mul_inplace(ETTensor* a, const ETTensor* b);

/**
 * @brief 인플레이스 스칼라 덧셈
 * @param tensor 텐서 (결과도 여기에 저장)
 * @param scalar 스칼라 값
 * @return 결과 텐서 (tensor와 동일), 실패시 NULL
 */
ETTensor* et_add_scalar_inplace(ETTensor* tensor, float scalar);

/**
 * @brief 인플레이스 스칼라 곱셈
 * @param tensor 텐서 (결과도 여기에 저장)
 * @param scalar 스칼라 값
 * @return 결과 텐서 (tensor와 동일), 실패시 NULL
 */
ETTensor* et_mul_scalar_inplace(ETTensor* tensor, float scalar);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_TENSOR_H