/**
 * @file tensor.c
 * @brief LibEtude 텐서 엔진 구현
 *
 * 다차원 텐서 데이터 구조와 기본 연산을 구현합니다.
 * 메모리 효율성과 인플레이스 연산을 지원합니다.
 */

#include "libetude/tensor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

// 매직 넘버 (텐서 손상 감지용)
#define ET_TENSOR_MAGIC 0x54454E53  // "TENS" in little-endian

// =============================================================================
// 데이터 타입 유틸리티 함수
// =============================================================================

size_t et_dtype_size(ETDataType dtype) {
    switch (dtype) {
        case ET_FLOAT32: return sizeof(float);
        case ET_FLOAT16: return 2;
        case ET_BFLOAT16: return 2;
        case ET_INT8: return sizeof(int8_t);
        case ET_INT4: return 1; // 패킹된 형태, 실제로는 2개 요소당 1바이트
        case ET_UINT8: return sizeof(uint8_t);
        case ET_INT32: return sizeof(int32_t);
        case ET_INT64: return sizeof(int64_t);
        default: return 0;
    }
}

const char* et_dtype_name(ETDataType dtype) {
    switch (dtype) {
        case ET_FLOAT32: return "float32";
        case ET_FLOAT16: return "float16";
        case ET_BFLOAT16: return "bfloat16";
        case ET_INT8: return "int8";
        case ET_INT4: return "int4";
        case ET_UINT8: return "uint8";
        case ET_INT32: return "int32";
        case ET_INT64: return "int64";
        default: return "unknown";
    }
}

bool et_dtype_is_float(ETDataType dtype) {
    return dtype == ET_FLOAT32 || dtype == ET_FLOAT16 || dtype == ET_BFLOAT16;
}

bool et_dtype_is_int(ETDataType dtype) {
    return dtype == ET_INT8 || dtype == ET_INT4 || dtype == ET_UINT8 ||
           dtype == ET_INT32 || dtype == ET_INT64;
}

// =============================================================================
// 내부 유틸리티 함수
// =============================================================================

/**
 * @brief 텐서 구조체 초기화
 */
static void init_tensor_struct(ETTensor* tensor) {
    memset(tensor, 0, sizeof(ETTensor));
    tensor->magic = ET_TENSOR_MAGIC;
    tensor->ref_count = 1;
    tensor->owns_data = true;
    tensor->is_contiguous = true;
}

/**
 * @brief 텐서 유효성 검사 (내부용)
 */
static bool validate_tensor_internal(const ETTensor* tensor) {
    if (!tensor) return false;
    if (tensor->magic != ET_TENSOR_MAGIC) return false;
    if (!tensor->data && tensor->size > 0) return false;
    if (!tensor->shape && tensor->ndim > 0) return false;
    if (!tensor->strides && tensor->ndim > 0) return false;
    if (tensor->ndim > ET_MAX_TENSOR_DIMS) return false;
    if (tensor->ref_count <= 0) return false;

    return true;
}

// =============================================================================
// 메모리 레이아웃 최적화 함수
// =============================================================================

void et_compute_strides(const size_t* shape, size_t ndim, ETDataType dtype, size_t* strides) {
    if (!shape || !strides || ndim == 0) return;

    size_t element_size = et_dtype_size(dtype);

    // C-order (row-major) 스트라이드 계산
    strides[ndim - 1] = element_size;
    for (int i = (int)ndim - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * shape[i + 1];
    }
}

size_t et_compute_size(const size_t* shape, size_t ndim) {
    if (!shape || ndim == 0) return 0;

    size_t total_size = 1;
    for (size_t i = 0; i < ndim; i++) {
        total_size *= shape[i];
    }
    return total_size;
}

size_t et_compute_offset(const size_t* indices, const size_t* strides, size_t ndim) {
    if (!indices || !strides || ndim == 0) return 0;

    size_t offset = 0;
    for (size_t i = 0; i < ndim; i++) {
        offset += indices[i] * strides[i];
    }
    return offset;
}

void et_compute_indices(size_t offset, const size_t* shape, size_t ndim, size_t* indices) {
    if (!shape || !indices || ndim == 0) return;

    for (int i = (int)ndim - 1; i >= 0; i--) {
        indices[i] = offset % shape[i];
        offset /= shape[i];
    }
}

// =============================================================================
// 텐서 생성 및 소멸 함수
// =============================================================================

ETTensor* et_create_tensor(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape) {
    return et_create_tensor_named(pool, dtype, ndim, shape, NULL);
}

ETTensor* et_create_tensor_named(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape, const char* name) {
    if (!pool || !shape || ndim == 0 || ndim > ET_MAX_TENSOR_DIMS) {
        return NULL;
    }

    // 텐서 구조체 할당
    ETTensor* tensor = (ETTensor*)et_alloc_from_pool(pool, sizeof(ETTensor));
    if (!tensor) return NULL;

    init_tensor_struct(tensor);
    tensor->pool = pool;
    tensor->dtype = dtype;
    tensor->ndim = ndim;
    tensor->mem_type = pool->mem_type;

    // 이름 설정
    if (name) {
        size_t name_len = strlen(name) + 1;
        char* tensor_name = (char*)et_alloc_from_pool(pool, name_len);
        if (tensor_name) {
            strcpy(tensor_name, name);
            tensor->name = tensor_name;
        }
    }

    // 모양 배열 할당 및 복사
    tensor->shape = (size_t*)et_alloc_from_pool(pool, ndim * sizeof(size_t));
    if (!tensor->shape) {
        et_free_to_pool(pool, tensor);
        return NULL;
    }
    memcpy(tensor->shape, shape, ndim * sizeof(size_t));

    // 스트라이드 배열 할당 및 계산
    tensor->strides = (size_t*)et_alloc_from_pool(pool, ndim * sizeof(size_t));
    if (!tensor->strides) {
        et_free_to_pool(pool, tensor->shape);
        et_free_to_pool(pool, tensor);
        return NULL;
    }
    et_compute_strides(shape, ndim, dtype, tensor->strides);

    // 총 요소 수 및 데이터 크기 계산
    tensor->size = et_compute_size(shape, ndim);
    tensor->data_size = tensor->size * et_dtype_size(dtype);

    // 데이터 메모리 할당
    if (tensor->data_size > 0) {
        tensor->data = et_alloc_from_pool(pool, tensor->data_size);
        if (!tensor->data) {
            et_free_to_pool(pool, tensor->strides);
            et_free_to_pool(pool, tensor->shape);
            et_free_to_pool(pool, tensor);
            return NULL;
        }
    }

    return tensor;
}

ETTensor* et_create_tensor_from_data(void* data, ETDataType dtype, size_t ndim, const size_t* shape, const size_t* strides) {
    if (!data || !shape || ndim == 0 || ndim > ET_MAX_TENSOR_DIMS) {
        return NULL;
    }

    // 텐서 구조체 할당 (시스템 malloc 사용, 메모리 풀 없음)
    ETTensor* tensor = (ETTensor*)malloc(sizeof(ETTensor));
    if (!tensor) return NULL;

    init_tensor_struct(tensor);
    tensor->data = data;
    tensor->dtype = dtype;
    tensor->ndim = ndim;
    tensor->mem_type = ET_MEM_CPU;
    tensor->owns_data = false; // 외부 데이터이므로 소유권 없음
    tensor->pool = NULL;

    // 모양 배열 할당 및 복사
    tensor->shape = (size_t*)malloc(ndim * sizeof(size_t));
    if (!tensor->shape) {
        free(tensor);
        return NULL;
    }
    memcpy(tensor->shape, shape, ndim * sizeof(size_t));

    // 스트라이드 배열 할당
    tensor->strides = (size_t*)malloc(ndim * sizeof(size_t));
    if (!tensor->strides) {
        free(tensor->shape);
        free(tensor);
        return NULL;
    }

    if (strides) {
        // 제공된 스트라이드 사용
        memcpy(tensor->strides, strides, ndim * sizeof(size_t));
        // 연속성 검사
        size_t expected_strides[ET_MAX_TENSOR_DIMS];
        et_compute_strides(shape, ndim, dtype, expected_strides);
        tensor->is_contiguous = (memcmp(strides, expected_strides, ndim * sizeof(size_t)) == 0);
    } else {
        // 기본 스트라이드 계산
        et_compute_strides(shape, ndim, dtype, tensor->strides);
        tensor->is_contiguous = true;
    }

    // 총 요소 수 및 데이터 크기 계산
    tensor->size = et_compute_size(shape, ndim);
    tensor->data_size = tensor->size * et_dtype_size(dtype);

    return tensor;
}

ETTensor* et_create_zeros(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape) {
    ETTensor* tensor = et_create_tensor(pool, dtype, ndim, shape);
    if (tensor) {
        et_zero_tensor(tensor);
    }
    return tensor;
}

ETTensor* et_create_ones(ETMemoryPool* pool, ETDataType dtype, size_t ndim, const size_t* shape) {
    ETTensor* tensor = et_create_tensor(pool, dtype, ndim, shape);
    if (tensor) {
        et_fill_tensor(tensor, 1.0f);
    }
    return tensor;
}

ETTensor* et_copy_tensor(const ETTensor* src, ETMemoryPool* pool) {
    if (!validate_tensor_internal(src)) return NULL;

    // 메모리 풀이 지정되지 않으면 원본과 동일한 풀 사용
    if (!pool) pool = src->pool;

    ETTensor* dst = et_create_tensor(pool, src->dtype, src->ndim, src->shape);
    if (!dst) return NULL;

    // 이름 복사 (있는 경우)
    if (src->name) {
        size_t name_len = strlen(src->name) + 1;
        char* tensor_name = (char*)et_alloc_from_pool(pool, name_len);
        if (tensor_name) {
            strcpy(tensor_name, src->name);
            dst->name = tensor_name;
        }
    }

    // 데이터 복사
    if (src->data && dst->data) {
        memcpy(dst->data, src->data, src->data_size);
    }

    // 메타데이터 복사
    dst->is_contiguous = src->is_contiguous;
    if (!src->is_contiguous) {
        memcpy(dst->strides, src->strides, src->ndim * sizeof(size_t));
    }

    return dst;
}

ETTensor* et_retain_tensor(ETTensor* tensor) {
    if (!validate_tensor_internal(tensor)) return NULL;

    tensor->ref_count++;
    return tensor;
}

void et_destroy_tensor(ETTensor* tensor) {
    if (!tensor) return;

    tensor->ref_count--;
    if (tensor->ref_count > 0) return;

    // 데이터 해제 (소유권이 있는 경우만)
    if (tensor->owns_data && tensor->data && tensor->pool) {
        et_free_to_pool(tensor->pool, tensor->data);
    }

    // 메타데이터 해제
    if (tensor->pool) {
        if (tensor->shape) et_free_to_pool(tensor->pool, tensor->shape);
        if (tensor->strides) et_free_to_pool(tensor->pool, tensor->strides);
        if (tensor->name) et_free_to_pool(tensor->pool, (void*)tensor->name);
        et_free_to_pool(tensor->pool, tensor);
    } else {
        // 메모리 풀이 없는 경우 (외부 데이터 텐서)
        if (tensor->shape) free(tensor->shape);
        if (tensor->strides) free(tensor->strides);
        if (tensor->name) free((void*)tensor->name);
        free(tensor);
    }
}

// =============================================================================
// 텐서 조작 함수
// =============================================================================

ETTensor* et_reshape_tensor(ETTensor* tensor, size_t ndim, const size_t* new_shape) {
    if (!validate_tensor_internal(tensor) || !new_shape || ndim == 0 || ndim > ET_MAX_TENSOR_DIMS) {
        return NULL;
    }

    // 새로운 모양의 총 요소 수 계산
    size_t new_size = et_compute_size(new_shape, ndim);
    if (new_size != tensor->size) {
        return NULL; // 요소 수가 다르면 reshape 불가
    }

    // 연속 메모리가 아니면 연속 메모리로 변환
    if (!tensor->is_contiguous) {
        ETTensor* contiguous = et_make_contiguous(tensor, NULL);
        if (!contiguous) return NULL;

        // 원본 텐서 교체
        et_destroy_tensor(tensor);
        tensor = contiguous;
    }

    ETTensor* reshaped;
    if (tensor->pool) {
        reshaped = (ETTensor*)et_alloc_from_pool(tensor->pool, sizeof(ETTensor));
    } else {
        reshaped = (ETTensor*)malloc(sizeof(ETTensor));
    }
    if (!reshaped) return NULL;

    // 기본 정보 복사
    *reshaped = *tensor;
    reshaped->ndim = ndim;
    reshaped->ref_count = 1;

    // 새로운 모양 배열 할당
    if (tensor->pool) {
        reshaped->shape = (size_t*)et_alloc_from_pool(tensor->pool, ndim * sizeof(size_t));
        reshaped->strides = (size_t*)et_alloc_from_pool(tensor->pool, ndim * sizeof(size_t));
    } else {
        reshaped->shape = (size_t*)malloc(ndim * sizeof(size_t));
        reshaped->strides = (size_t*)malloc(ndim * sizeof(size_t));
    }

    if (!reshaped->shape || !reshaped->strides) {
        if (reshaped->shape) {
            if (tensor->pool) et_free_to_pool(tensor->pool, reshaped->shape);
            else free(reshaped->shape);
        }
        if (reshaped->strides) {
            if (tensor->pool) et_free_to_pool(tensor->pool, reshaped->strides);
            else free(reshaped->strides);
        }
        if (tensor->pool) et_free_to_pool(tensor->pool, reshaped);
        else free(reshaped);
        return NULL;
    }

    // 새로운 모양과 스트라이드 설정
    memcpy(reshaped->shape, new_shape, ndim * sizeof(size_t));
    et_compute_strides(new_shape, ndim, tensor->dtype, reshaped->strides);

    // 데이터는 공유 (참조 카운트 증가)
    tensor->ref_count++;

    return reshaped;
}

ETTensor* et_slice_tensor(ETTensor* tensor, const ETSlice* slices) {
    if (!validate_tensor_internal(tensor) || !slices) {
        return NULL;
    }

    // 슬라이싱 구현은 복잡하므로 기본적인 형태만 구현
    // 실제 구현에서는 더 정교한 슬라이싱 로직이 필요

    // 새로운 모양 계산
    size_t new_shape[ET_MAX_TENSOR_DIMS];
    size_t new_strides[ET_MAX_TENSOR_DIMS];
    size_t data_offset = 0;

    for (size_t i = 0; i < tensor->ndim; i++) {
        const ETSlice* slice = &slices[i];
        size_t start = slice->start;
        size_t end = slice->end;
        size_t step = slice->step > 0 ? slice->step : 1;

        // 범위 검사
        if (start >= tensor->shape[i]) start = tensor->shape[i] - 1;
        if (end > tensor->shape[i]) end = tensor->shape[i];
        if (start >= end) return NULL;

        new_shape[i] = (end - start + step - 1) / step;
        new_strides[i] = tensor->strides[i] * step;
        data_offset += start * tensor->strides[i];
    }

    // 새로운 텐서 생성 (데이터 공유)
    void* new_data = (char*)tensor->data + data_offset;
    ETTensor* sliced = et_create_tensor_from_data(new_data, tensor->dtype, tensor->ndim, new_shape, new_strides);
    if (!sliced) return NULL;

    // 원본 텐서 참조 증가
    tensor->ref_count++;

    return sliced;
}

ETTensor* et_transpose_tensor(ETTensor* tensor) {
    if (!validate_tensor_internal(tensor) || tensor->ndim != 2) {
        return NULL;
    }

    size_t new_shape[2] = {tensor->shape[1], tensor->shape[0]};
    size_t new_strides[2] = {tensor->strides[1], tensor->strides[0]};

    ETTensor* transposed = et_create_tensor_from_data(tensor->data, tensor->dtype, 2, new_shape, new_strides);
    if (!transposed) return NULL;

    transposed->is_contiguous = false; // 전치된 텐서는 일반적으로 비연속
    tensor->ref_count++;

    return transposed;
}

ETTensor* et_permute_tensor(ETTensor* tensor, const size_t* axes) {
    if (!validate_tensor_internal(tensor) || !axes) {
        return NULL;
    }

    // 축 순서 유효성 검사
    bool used[ET_MAX_TENSOR_DIMS] = {false};
    for (size_t i = 0; i < tensor->ndim; i++) {
        if (axes[i] >= tensor->ndim || used[axes[i]]) {
            return NULL; // 잘못된 축 순서
        }
        used[axes[i]] = true;
    }

    // 새로운 모양과 스트라이드 계산
    size_t new_shape[ET_MAX_TENSOR_DIMS];
    size_t new_strides[ET_MAX_TENSOR_DIMS];

    for (size_t i = 0; i < tensor->ndim; i++) {
        new_shape[i] = tensor->shape[axes[i]];
        new_strides[i] = tensor->strides[axes[i]];
    }

    ETTensor* permuted = et_create_tensor_from_data(tensor->data, tensor->dtype, tensor->ndim, new_shape, new_strides);
    if (!permuted) return NULL;

    // 연속성 검사
    size_t expected_strides[ET_MAX_TENSOR_DIMS];
    et_compute_strides(new_shape, tensor->ndim, tensor->dtype, expected_strides);
    permuted->is_contiguous = (memcmp(new_strides, expected_strides, tensor->ndim * sizeof(size_t)) == 0);

    tensor->ref_count++;
    return permuted;
}

ETTensor* et_expand_dims(ETTensor* tensor, int axis) {
    if (!validate_tensor_internal(tensor)) return NULL;

    size_t new_ndim = tensor->ndim + 1;
    if (new_ndim > ET_MAX_TENSOR_DIMS) return NULL;

    // 축 정규화
    if (axis < 0) axis += (int)new_ndim;
    if (axis < 0 || axis >= (int)new_ndim) return NULL;

    // 새로운 모양과 스트라이드 계산
    size_t new_shape[ET_MAX_TENSOR_DIMS];
    size_t new_strides[ET_MAX_TENSOR_DIMS];

    for (size_t i = 0, j = 0; i < new_ndim; i++) {
        if (i == (size_t)axis) {
            new_shape[i] = 1;
            new_strides[i] = (i < new_ndim - 1) ? tensor->strides[j] : et_dtype_size(tensor->dtype);
        } else {
            new_shape[i] = tensor->shape[j];
            new_strides[i] = tensor->strides[j];
            j++;
        }
    }

    ETTensor* expanded = et_create_tensor_from_data(tensor->data, tensor->dtype, new_ndim, new_shape, new_strides);
    if (!expanded) return NULL;

    expanded->is_contiguous = false; // 차원 확장된 텐서는 일반적으로 비연속
    tensor->ref_count++;

    return expanded;
}

ETTensor* et_squeeze_tensor(ETTensor* tensor, int axis) {
    if (!validate_tensor_internal(tensor)) return NULL;

    if (axis == -1) {
        // 모든 크기 1인 차원 제거
        size_t new_shape[ET_MAX_TENSOR_DIMS];
        size_t new_strides[ET_MAX_TENSOR_DIMS];
        size_t new_ndim = 0;

        for (size_t i = 0; i < tensor->ndim; i++) {
            if (tensor->shape[i] != 1) {
                new_shape[new_ndim] = tensor->shape[i];
                new_strides[new_ndim] = tensor->strides[i];
                new_ndim++;
            }
        }

        if (new_ndim == 0) {
            // 모든 차원이 1이면 스칼라 (1차원, 크기 1)
            new_ndim = 1;
            new_shape[0] = 1;
            new_strides[0] = et_dtype_size(tensor->dtype);
        }

        ETTensor* squeezed = et_create_tensor_from_data(tensor->data, tensor->dtype, new_ndim, new_shape, new_strides);
        if (!squeezed) return NULL;

        // 연속성 검사
        size_t expected_strides[ET_MAX_TENSOR_DIMS];
        et_compute_strides(new_shape, new_ndim, tensor->dtype, expected_strides);
        squeezed->is_contiguous = (memcmp(new_strides, expected_strides, new_ndim * sizeof(size_t)) == 0);

        tensor->ref_count++;
        return squeezed;
    } else {
        // 특정 축 제거
        if (axis < 0) axis += (int)tensor->ndim;
        if (axis < 0 || axis >= (int)tensor->ndim) return NULL;
        if (tensor->shape[axis] != 1) return NULL; // 크기가 1이 아니면 제거 불가

        size_t new_shape[ET_MAX_TENSOR_DIMS];
        size_t new_strides[ET_MAX_TENSOR_DIMS];
        size_t new_ndim = tensor->ndim - 1;

        for (size_t i = 0, j = 0; i < tensor->ndim; i++) {
            if (i != (size_t)axis) {
                new_shape[j] = tensor->shape[i];
                new_strides[j] = tensor->strides[i];
                j++;
            }
        }

        if (new_ndim == 0) {
            // 스칼라가 되면 1차원, 크기 1로 설정
            new_ndim = 1;
            new_shape[0] = 1;
            new_strides[0] = et_dtype_size(tensor->dtype);
        }

        ETTensor* squeezed = et_create_tensor_from_data(tensor->data, tensor->dtype, new_ndim, new_shape, new_strides);
        if (!squeezed) return NULL;

        // 연속성 검사
        size_t expected_strides[ET_MAX_TENSOR_DIMS];
        et_compute_strides(new_shape, new_ndim, tensor->dtype, expected_strides);
        squeezed->is_contiguous = (memcmp(new_strides, expected_strides, new_ndim * sizeof(size_t)) == 0);

        tensor->ref_count++;
        return squeezed;
    }
}

// =============================================================================
// 텐서 정보 조회 함수
// =============================================================================

bool et_validate_tensor(const ETTensor* tensor) {
    return validate_tensor_internal(tensor);
}

bool et_is_contiguous(const ETTensor* tensor) {
    if (!validate_tensor_internal(tensor)) return false;
    return tensor->is_contiguous;
}

ETTensor* et_make_contiguous(ETTensor* tensor, ETMemoryPool* pool) {
    if (!validate_tensor_internal(tensor)) return NULL;

    if (tensor->is_contiguous) {
        return et_retain_tensor(tensor);
    }

    // 연속 메모리 텐서 생성
    if (!pool) pool = tensor->pool;
    ETTensor* contiguous = et_create_tensor_named(pool, tensor->dtype, tensor->ndim, tensor->shape, tensor->name);
    if (!contiguous) return NULL;

    // 데이터 복사 (스트라이드를 고려한 복사)
    // 간단한 구현: 요소별 복사
    size_t indices[ET_MAX_TENSOR_DIMS] = {0};
    size_t element_size = et_dtype_size(tensor->dtype);

    for (size_t i = 0; i < tensor->size; i++) {
        size_t src_offset = et_compute_offset(indices, tensor->strides, tensor->ndim);
        size_t dst_offset = i * element_size;

        memcpy((char*)contiguous->data + dst_offset, (char*)tensor->data + src_offset, element_size);

        // 다음 인덱스 계산
        for (int j = (int)tensor->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < tensor->shape[j]) break;
            indices[j] = 0;
        }
    }

    return contiguous;
}

bool et_same_shape(const ETTensor* a, const ETTensor* b) {
    if (!validate_tensor_internal(a) || !validate_tensor_internal(b)) return false;
    if (a->ndim != b->ndim) return false;

    return memcmp(a->shape, b->shape, a->ndim * sizeof(size_t)) == 0;
}

bool et_can_broadcast(const ETTensor* a, const ETTensor* b) {
    if (!validate_tensor_internal(a) || !validate_tensor_internal(b)) return false;

    // 브로드캐스팅 규칙: 뒤에서부터 차원을 비교하여
    // 1) 차원 크기가 같거나
    // 2) 한쪽이 1이면 브로드캐스팅 가능

    size_t max_ndim = a->ndim > b->ndim ? a->ndim : b->ndim;

    for (size_t i = 0; i < max_ndim; i++) {
        size_t dim_a = (i < a->ndim) ? a->shape[a->ndim - 1 - i] : 1;
        size_t dim_b = (i < b->ndim) ? b->shape[b->ndim - 1 - i] : 1;

        if (dim_a != dim_b && dim_a != 1 && dim_b != 1) {
            return false;
        }
    }

    return true;
}

void et_print_tensor_info(const ETTensor* tensor) {
    if (!validate_tensor_internal(tensor)) {
        printf("Invalid tensor\n");
        return;
    }

    printf("Tensor Info:\n");
    if (tensor->name) {
        printf("  Name: %s\n", tensor->name);
    }
    printf("  Data Type: %s\n", et_dtype_name(tensor->dtype));
    printf("  Dimensions: %zu\n", tensor->ndim);
    printf("  Shape: [");
    for (size_t i = 0; i < tensor->ndim; i++) {
        printf("%zu", tensor->shape[i]);
        if (i < tensor->ndim - 1) printf(", ");
    }
    printf("]\n");
    printf("  Strides: [");
    for (size_t i = 0; i < tensor->ndim; i++) {
        printf("%zu", tensor->strides[i]);
        if (i < tensor->ndim - 1) printf(", ");
    }
    printf("]\n");
    printf("  Total Elements: %zu\n", tensor->size);
    printf("  Data Size: %zu bytes\n", tensor->data_size);
    printf("  Memory Type: %s\n", tensor->mem_type == ET_MEM_CPU ? "CPU" :
                                  tensor->mem_type == ET_MEM_GPU ? "GPU" : "Shared");
    printf("  Contiguous: %s\n", tensor->is_contiguous ? "Yes" : "No");
    printf("  Owns Data: %s\n", tensor->owns_data ? "Yes" : "No");
    printf("  Reference Count: %d\n", tensor->ref_count);
}

// =============================================================================
// 텐서 데이터 접근 함수
// =============================================================================

float et_get_float(const ETTensor* tensor, const size_t* indices) {
    if (!validate_tensor_internal(tensor) || !indices) return 0.0f;

    void* ptr = et_get_ptr(tensor, indices);
    if (!ptr) return 0.0f;

    // 데이터 타입에 따른 변환
    switch (tensor->dtype) {
        case ET_FLOAT32:
            return *(float*)ptr;
        case ET_INT8:
            return (float)(*(int8_t*)ptr);
        case ET_UINT8:
            return (float)(*(uint8_t*)ptr);
        case ET_INT32:
            return (float)(*(int32_t*)ptr);
        case ET_INT64:
            return (float)(*(int64_t*)ptr);
        // TODO: FLOAT16, BFLOAT16, INT4 변환 구현
        default:
            return 0.0f;
    }
}

void et_set_float(ETTensor* tensor, const size_t* indices, float value) {
    if (!validate_tensor_internal(tensor) || !indices) return;

    void* ptr = et_get_ptr(tensor, indices);
    if (!ptr) return;

    // 데이터 타입에 따른 변환
    switch (tensor->dtype) {
        case ET_FLOAT32:
            *(float*)ptr = value;
            break;
        case ET_INT8:
            *(int8_t*)ptr = (int8_t)value;
            break;
        case ET_UINT8:
            *(uint8_t*)ptr = (uint8_t)value;
            break;
        case ET_INT32:
            *(int32_t*)ptr = (int32_t)value;
            break;
        case ET_INT64:
            *(int64_t*)ptr = (int64_t)value;
            break;
        // TODO: FLOAT16, BFLOAT16, INT4 변환 구현
        default:
            break;
    }
}

void* et_get_ptr(const ETTensor* tensor, const size_t* indices) {
    if (!validate_tensor_internal(tensor) || !indices) return NULL;

    // 인덱스 범위 검사
    for (size_t i = 0; i < tensor->ndim; i++) {
        if (indices[i] >= tensor->shape[i]) return NULL;
    }

    size_t offset = et_compute_offset(indices, tensor->strides, tensor->ndim);
    return (char*)tensor->data + offset;
}

void* et_get_data_ptr(const ETTensor* tensor, ETDataType dtype) {
    if (!validate_tensor_internal(tensor)) return NULL;
    if (tensor->dtype != dtype) return NULL; // 타입이 다르면 NULL 반환

    return tensor->data;
}

// =============================================================================
// 텐서 초기화 함수
// =============================================================================

void et_fill_tensor(ETTensor* tensor, float value) {
    if (!validate_tensor_internal(tensor)) return;

    size_t indices[ET_MAX_TENSOR_DIMS] = {0};

    for (size_t i = 0; i < tensor->size; i++) {
        et_set_float(tensor, indices, value);

        // 다음 인덱스 계산
        for (int j = (int)tensor->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < tensor->shape[j]) break;
            indices[j] = 0;
        }
    }
}

void et_zero_tensor(ETTensor* tensor) {
    if (!validate_tensor_internal(tensor)) return;

    // 연속 메모리인 경우 빠른 초기화
    if (tensor->is_contiguous) {
        memset(tensor->data, 0, tensor->data_size);
    } else {
        et_fill_tensor(tensor, 0.0f);
    }
}

void et_random_uniform(ETTensor* tensor, float min_val, float max_val) {
    if (!validate_tensor_internal(tensor)) return;

    size_t indices[ET_MAX_TENSOR_DIMS] = {0};
    float range = max_val - min_val;

    for (size_t i = 0; i < tensor->size; i++) {
        float random_val = min_val + range * ((float)rand() / RAND_MAX);
        et_set_float(tensor, indices, random_val);

        // 다음 인덱스 계산
        for (int j = (int)tensor->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < tensor->shape[j]) break;
            indices[j] = 0;
        }
    }
}

void et_random_normal(ETTensor* tensor, float mean, float std) {
    if (!validate_tensor_internal(tensor)) return;

    size_t indices[ET_MAX_TENSOR_DIMS] = {0};

    for (size_t i = 0; i < tensor->size; i++) {
        // Box-Muller 변환을 사용한 정규분포 생성 (간단한 구현)
        static bool has_spare = false;
        static float spare;

        float random_val;
        if (has_spare) {
            random_val = spare;
            has_spare = false;
        } else {
            float u = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float v = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float s = u * u + v * v;

            if (s >= 1.0f || s == 0.0f) {
                i--; // 다시 시도
                continue;
            }

            float mul = sqrtf(-2.0f * logf(s) / s);
            random_val = u * mul;
            spare = v * mul;
            has_spare = true;
        }

        random_val = random_val * std + mean;
        et_set_float(tensor, indices, random_val);

        // 다음 인덱스 계산
        for (int j = (int)tensor->ndim - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < tensor->shape[j]) break;
            indices[j] = 0;
        }
    }
}