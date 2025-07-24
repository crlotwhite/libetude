/**
 * @file tensor_ops.c
 * @brief LibEtude 텐서 연산 구현
 *
 * 텐서 간의 수학 연산을 구현합니다.
 * 인플레이스 연산과 브로드캐스팅을 지원합니다.
 */

#include "libetude/tensor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

// =============================================================================
// 내부 유틸리티 함수
// =============================================================================

/**
 * @brief 브로드캐스팅된 출력 모양 계산
 */
static bool compute_broadcast_shape(const ETTensor* a, const ETTensor* b,
                                   size_t* out_ndim, size_t* out_shape) {
    if (!a || !b || !out_ndim || !out_shape) return false;

    size_t max_ndim = a->ndim > b->ndim ? a->ndim : b->ndim;
    *out_ndim = max_ndim;

    for (size_t i = 0; i < max_ndim; i++) {
        size_t dim_a = (i < a->ndim) ? a->shape[a->ndim - 1 - i] : 1;
        size_t dim_b = (i < b->ndim) ? b->shape[b->ndim - 1 - i] : 1;

        if (dim_a == dim_b) {
            out_shape[max_ndim - 1 - i] = dim_a;
        } else if (dim_a == 1) {
            out_shape[max_ndim - 1 - i] = dim_b;
        } else if (dim_b == 1) {
            out_shape[max_ndim - 1 - i] = dim_a;
        } else {
            return false; // 브로드캐스팅 불가능
        }
    }

    return true;
}

/**
 * @brief 브로드캐스팅된 인덱스 계산
 */
static void compute_broadcast_indices(const size_t* out_indices, size_t out_ndim,
                                     const ETTensor* tensor, size_t* tensor_indices) {
    for (size_t i = 0; i < tensor->ndim; i++) {
        size_t out_idx = out_ndim - tensor->ndim + i;
        if (tensor->shape[i] == 1) {
            tensor_indices[i] = 0; // 브로드캐스팅
        } else {
            tensor_indices[i] = out_indices[out_idx];
        }
    }
}

/**
 * @brief 요소별 연산 수행 (일반적인 구현)
 */
typedef float (*ElementWiseOp)(float a, float b);

static ETTensor* elementwise_op(const ETTensor* a, const ETTensor* b, ETTensor* out,
                               ElementWiseOp op, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(a) || !et_validate_tensor(b)) return NULL;

    // 브로드캐스팅 가능 여부 확인
    if (!et_can_broadcast(a, b)) return NULL;

    // 출력 모양 계산
    size_t out_ndim;
    size_t out_shape[ET_MAX_TENSOR_DIMS];
    if (!compute_broadcast_shape(a, b, &out_ndim, out_shape)) return NULL;

    // 출력 텐서 생성 또는 검증
    bool created_output = false;
    if (!out) {
        ETMemoryPool* pool = options && options->output_pool ? options->output_pool : a->pool;
        out = et_create_tensor(pool, a->dtype, out_ndim, out_shape);
        if (!out) return NULL;
        created_output = true;
    } else {
        // 출력 텐서 모양 검증
        if (out->ndim != out_ndim || !et_same_shape(out, out)) {
            if (created_output) et_destroy_tensor(out);
            return NULL;
        }
    }

    // 요소별 연산 수행
    size_t out_indices[ET_MAX_TENSOR_DIMS] = {0};
    size_t a_indices[ET_MAX_TENSOR_DIMS] = {0};
    size_t b_indices[ET_MAX_TENSOR_DIMS] = {0};

    for (size_t i = 0; i < out->size; i++) {
        // 브로드캐스팅된 인덱스 계산
        compute_broadcast_indices(out_indices, out_ndim, a, a_indices);
        compute_broadcast_indices(out_indices, out_ndim, b, b_indices);

        // 값 가져오기 및 연산 수행
        float val_a = et_get_float(a, a_indices);
        float val_b = et_get_float(b, b_indices);
        float result = op(val_a, val_b);

        // 결과 저장
        et_set_float(out, out_indices, result);

        // 다음 인덱스 계산
        for (int j = (int)out_ndim - 1; j >= 0; j--) {
            out_indices[j]++;
            if (out_indices[j] < out_shape[j]) break;
            out_indices[j] = 0;
        }
    }

    return out;
}

// =============================================================================
// 요소별 연산 함수들
// =============================================================================

static float add_op(float a, float b) { return a + b; }
static float sub_op(float a, float b) { return a - b; }
static float mul_op(float a, float b) { return a * b; }
static float div_op(float a, float b) { return b != 0.0f ? a / b : 0.0f; }

// =============================================================================
// 공개 텐서 연산 함수
// =============================================================================

ETTensor* et_add(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options) {
    return elementwise_op(a, b, out, add_op, options);
}

ETTensor* et_sub(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options) {
    return elementwise_op(a, b, out, sub_op, options);
}

ETTensor* et_mul(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options) {
    return elementwise_op(a, b, out, mul_op, options);
}

ETTensor* et_div(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options) {
    return elementwise_op(a, b, out, div_op, options);
}

ETTensor* et_add_scalar(const ETTensor* tensor, float scalar, ETTensor* out, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(tensor)) return NULL;

    // 스칼라를 1x1 텐서로 변환
    size_t scalar_shape[1] = {1};
    ETTensor* scalar_tensor = et_create_tensor(tensor->pool, tensor->dtype, 1, scalar_shape);
    if (!scalar_tensor) return NULL;

    et_set_float(scalar_tensor, (size_t[]){0}, scalar);

    ETTensor* result = et_add(tensor, scalar_tensor, out, options);
    et_destroy_tensor(scalar_tensor);

    return result;
}

ETTensor* et_mul_scalar(const ETTensor* tensor, float scalar, ETTensor* out, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(tensor)) return NULL;

    // 스칼라를 1x1 텐서로 변환
    size_t scalar_shape[1] = {1};
    ETTensor* scalar_tensor = et_create_tensor(tensor->pool, tensor->dtype, 1, scalar_shape);
    if (!scalar_tensor) return NULL;

    et_set_float(scalar_tensor, (size_t[]){0}, scalar);

    ETTensor* result = et_mul(tensor, scalar_tensor, out, options);
    et_destroy_tensor(scalar_tensor);

    return result;
}

ETTensor* et_matmul(const ETTensor* a, const ETTensor* b, ETTensor* out, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(a) || !et_validate_tensor(b)) return NULL;

    // 2D 행렬 곱셈만 지원 (확장 가능)
    if (a->ndim != 2 || b->ndim != 2) return NULL;

    size_t M = a->shape[0];  // A의 행 수
    size_t K = a->shape[1];  // A의 열 수 = B의 행 수
    size_t N = b->shape[1];  // B의 열 수

    if (K != b->shape[0]) return NULL; // 행렬 곱셈 불가능

    // 출력 텐서 생성 또는 검증
    bool created_output = false;
    if (!out) {
        ETMemoryPool* pool = options && options->output_pool ? options->output_pool : a->pool;
        size_t out_shape[2] = {M, N};
        out = et_create_tensor(pool, a->dtype, 2, out_shape);
        if (!out) return NULL;
        created_output = true;
    } else {
        if (out->ndim != 2 || out->shape[0] != M || out->shape[1] != N) {
            if (created_output) et_destroy_tensor(out);
            return NULL;
        }
    }

    // 행렬 곱셈 수행 (기본적인 구현)
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; k++) {
                float val_a = et_get_float(a, (size_t[]){i, k});
                float val_b = et_get_float(b, (size_t[]){k, j});
                sum += val_a * val_b;
            }
            et_set_float(out, (size_t[]){i, j}, sum);
        }
    }

    return out;
}

ETTensor* et_softmax(const ETTensor* input, ETTensor* out, int axis, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(input)) return NULL;

    // 축 정규화
    if (axis < 0) axis += (int)input->ndim;
    if (axis < 0 || axis >= (int)input->ndim) return NULL;

    // 출력 텐서 생성 또는 검증
    bool created_output = false;
    if (!out) {
        ETMemoryPool* pool = options && options->output_pool ? options->output_pool : input->pool;
        out = et_create_tensor(pool, input->dtype, input->ndim, input->shape);
        if (!out) return NULL;
        created_output = true;
    } else {
        if (!et_same_shape(input, out)) {
            if (created_output) et_destroy_tensor(out);
            return NULL;
        }
    }

    // 소프트맥스 계산을 위한 임시 변수들
    size_t axis_size = input->shape[axis];
    size_t outer_size = 1;
    size_t inner_size = 1;

    // 외부 및 내부 크기 계산
    for (size_t i = 0; i < (size_t)axis; i++) {
        outer_size *= input->shape[i];
    }
    for (size_t i = axis + 1; i < input->ndim; i++) {
        inner_size *= input->shape[i];
    }

    // 소프트맥스 계산
    for (size_t outer = 0; outer < outer_size; outer++) {
        for (size_t inner = 0; inner < inner_size; inner++) {
            // 최대값 찾기 (수치 안정성을 위해)
            float max_val = -INFINITY;
            for (size_t i = 0; i < axis_size; i++) {
                size_t indices[ET_MAX_TENSOR_DIMS];

                // 인덱스 계산
                size_t temp_outer = outer;
                for (int j = axis - 1; j >= 0; j--) {
                    indices[j] = temp_outer % input->shape[j];
                    temp_outer /= input->shape[j];
                }
                indices[axis] = i;
                size_t temp_inner = inner;
                for (size_t j = axis + 1; j < input->ndim; j++) {
                    indices[j] = temp_inner % input->shape[j];
                    temp_inner /= input->shape[j];
                }

                float val = et_get_float(input, indices);
                if (val > max_val) max_val = val;
            }

            // 지수 계산 및 합계
            float sum = 0.0f;
            for (size_t i = 0; i < axis_size; i++) {
                size_t indices[ET_MAX_TENSOR_DIMS];

                // 인덱스 계산 (위와 동일)
                size_t temp_outer = outer;
                for (int j = axis - 1; j >= 0; j--) {
                    indices[j] = temp_outer % input->shape[j];
                    temp_outer /= input->shape[j];
                }
                indices[axis] = i;
                size_t temp_inner = inner;
                for (size_t j = axis + 1; j < input->ndim; j++) {
                    indices[j] = temp_inner % input->shape[j];
                    temp_inner /= input->shape[j];
                }

                float val = et_get_float(input, indices);
                float exp_val = expf(val - max_val);
                sum += exp_val;
                et_set_float(out, indices, exp_val);
            }

            // 정규화
            for (size_t i = 0; i < axis_size; i++) {
                size_t indices[ET_MAX_TENSOR_DIMS];

                // 인덱스 계산 (위와 동일)
                size_t temp_outer = outer;
                for (int j = axis - 1; j >= 0; j--) {
                    indices[j] = temp_outer % input->shape[j];
                    temp_outer /= input->shape[j];
                }
                indices[axis] = i;
                size_t temp_inner = inner;
                for (size_t j = axis + 1; j < input->ndim; j++) {
                    indices[j] = temp_inner % input->shape[j];
                    temp_inner /= input->shape[j];
                }

                float val = et_get_float(out, indices);
                et_set_float(out, indices, val / sum);
            }
        }
    }

    return out;
}

// =============================================================================
// 축소 연산 함수
// =============================================================================

ETTensor* et_sum(const ETTensor* input, ETTensor* out, int axis, bool keepdims, const ETTensorOpOptions* options) {
    if (!et_validate_tensor(input)) return NULL;

    // 전체 합계 (axis == -1)
    if (axis == -1) {
        ETMemoryPool* pool = options && options->output_pool ? options->output_pool : input->pool;
        size_t out_shape[1] = {1};
        if (!out) {
            out = et_create_tensor(pool, input->dtype, keepdims ? input->ndim : 1,
                                 keepdims ? NULL : out_shape);
            if (!out) return NULL;

            if (keepdims) {
                // 모든 차원을 1로 설정
                for (size_t i = 0; i < input->ndim; i++) {
                    out->shape[i] = 1;
                }
                et_compute_strides(out->shape, out->ndim, out->dtype, out->strides);
                out->size = 1;
            }
        }

        float sum = 0.0f;
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            sum += et_get_float(input, indices);

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }

        if (keepdims) {
            size_t zero_indices[ET_MAX_TENSOR_DIMS] = {0};
            et_set_float(out, zero_indices, sum);
        } else {
            et_set_float(out, (size_t[]){0}, sum);
        }

        return out;
    }

    // 특정 축에 대한 합계
    if (axis < 0) axis += (int)input->ndim;
    if (axis < 0 || axis >= (int)input->ndim) return NULL;

    // 출력 모양 계산
    size_t out_ndim = keepdims ? input->ndim : input->ndim - 1;
    size_t out_shape[ET_MAX_TENSOR_DIMS];

    if (keepdims) {
        memcpy(out_shape, input->shape, input->ndim * sizeof(size_t));
        out_shape[axis] = 1;
    } else {
        size_t out_idx = 0;
        for (size_t i = 0; i < input->ndim; i++) {
            if (i != (size_t)axis) {
                out_shape[out_idx++] = input->shape[i];
            }
        }
    }

    // 출력 텐서 생성
    if (!out) {
        ETMemoryPool* pool = options && options->output_pool ? options->output_pool : input->pool;
        out = et_create_tensor(pool, input->dtype, out_ndim, out_shape);
        if (!out) return NULL;
    }

    // 합계 계산 (간단한 구현)
    et_zero_tensor(out);

    size_t indices[ET_MAX_TENSOR_DIMS] = {0};
    for (size_t i = 0; i < input->size; i++) {
        // 출력 인덱스 계산
        size_t out_indices[ET_MAX_TENSOR_DIMS];
        if (keepdims) {
            memcpy(out_indices, indices, input->ndim * sizeof(size_t));
            out_indices[axis] = 0;
        } else {
            size_t out_idx = 0;
            for (size_t j = 0; j < input->ndim; j++) {
                if (j != (size_t)axis) {
                    out_indices[out_idx++] = indices[j];
                }
            }
        }

        float input_val = et_get_float(input, indices);
        float current_sum = et_get_float(out, out_indices);
        et_set_float(out, out_indices, current_sum + input_val);

        // 다음 인덱스 계산
        for (int j = (int)input->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < input->shape[j]) break;
            indices[j] = 0;
        }
    }

    return out;
}

ETTensor* et_mean(const ETTensor* input, ETTensor* out, int axis, bool keepdims, const ETTensorOpOptions* options) {
    ETTensor* sum_tensor = et_sum(input, out, axis, keepdims, options);
    if (!sum_tensor) return NULL;

    // 평균 계산을 위해 요소 수로 나누기
    float divisor;
    if (axis == -1) {
        divisor = (float)input->size;
    } else {
        if (axis < 0) axis += (int)input->ndim;
        divisor = (float)input->shape[axis];
    }

    // 모든 요소를 divisor로 나누기
    size_t indices[ET_MAX_TENSOR_DIMS] = {0};
    for (size_t i = 0; i < sum_tensor->size; i++) {
        float val = et_get_float(sum_tensor, indices);
        et_set_float(sum_tensor, indices, val / divisor);

        // 다음 인덱스 계산
        for (int j = (int)sum_tensor->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < sum_tensor->shape[j]) break;
            indices[j] = 0;
        }
    }

    return sum_tensor;
}

// =============================================================================
// 인플레이스 연산 함수
// =============================================================================

ETTensor* et_add_inplace(ETTensor* a, const ETTensor* b) {
    ETTensorOpOptions options = {.inplace = true};
    return et_add(a, b, a, &options);
}

ETTensor* et_mul_inplace(ETTensor* a, const ETTensor* b) {
    ETTensorOpOptions options = {.inplace = true};
    return et_mul(a, b, a, &options);
}

ETTensor* et_add_scalar_inplace(ETTensor* tensor, float scalar) {
    ETTensorOpOptions options = {.inplace = true};
    return et_add_scalar(tensor, scalar, tensor, &options);
}

ETTensor* et_mul_scalar_inplace(ETTensor* tensor, float scalar) {
    ETTensorOpOptions options = {.inplace = true};
    return et_mul_scalar(tensor, scalar, tensor, &options);
}