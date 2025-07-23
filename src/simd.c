#include "simd.h"
#include <string.h>

// Basic scalar implementations
void simd_filter_eq_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] == value) ? 1 : 0;
    }
}

void simd_filter_gt_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] > value) ? 1 : 0;
    }
}

void simd_filter_lt_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] < value) ? 1 : 0;
    }
}

void simd_filter_eq_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] == value) ? 1 : 0;
    }
}

void simd_filter_gt_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] > value) ? 1 : 0;
    }
}

void simd_filter_lt_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] < value) ? 1 : 0;
    }
}

void simd_filter_eq_f64(const double *column, size_t count, double value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] == value) ? 1 : 0;
    }
}

void simd_filter_gt_f64(const double *column, size_t count, double value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] > value) ? 1 : 0;
    }
}

void simd_filter_lt_f64(const double *column, size_t count, double value, uint8_t *bitmap) {
    for (size_t i = 0; i < count; i++) {
        bitmap[i] = (column[i] < value) ? 1 : 0;
    }
}

void simd_bitmap_and(const uint8_t *a, const uint8_t *b, uint8_t *result, size_t count) {
    for (size_t i = 0; i < count; i++) {
        result[i] = a[i] & b[i];
    }
}

void simd_bitmap_or(const uint8_t *a, const uint8_t *b, uint8_t *result, size_t count) {
    for (size_t i = 0; i < count; i++) {
        result[i] = a[i] | b[i];
    }
}

size_t simd_count_set_bits(const uint8_t *bitmap, size_t count) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += bitmap[i] ? 1 : 0;
    }
    return total;
}

#ifdef __clang__
// SIMD optimized versions using Clang vector extensions

void simd_filter_eq_i32_vectorized(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    size_t vector_count = count / 4;
    size_t remainder = count % 4;
    (void)remainder; // Suppress unused variable warning - may be used in future implementations
    
    v4i target = {value, value, value, value};
    
    for (size_t i = 0; i < vector_count; i++) {
        v4i vals = *((v4i*)&column[i * 4]);
        v4i cmp = vals == target;
        
        // Extract results
        bitmap[i * 4 + 0] = cmp[0] ? 1 : 0;
        bitmap[i * 4 + 1] = cmp[1] ? 1 : 0;
        bitmap[i * 4 + 2] = cmp[2] ? 1 : 0;
        bitmap[i * 4 + 3] = cmp[3] ? 1 : 0;
        
        // Prefetch next cache line
        if (i + 4 < vector_count) {
            __builtin_prefetch(&column[(i + 4) * 4], 0, 3);
        }
    }
    
    // Handle remainder
    for (size_t i = vector_count * 4; i < count; i++) {
        bitmap[i] = (column[i] == value) ? 1 : 0;
    }
}

void simd_filter_gt_i32_vectorized(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    size_t vector_count = count / 4;
    size_t remainder = count % 4;
    (void)remainder; // Suppress unused variable warning - may be used in future implementations
    
    v4i target = {value, value, value, value};
    
    for (size_t i = 0; i < vector_count; i++) {
        v4i vals = *((v4i*)&column[i * 4]);
        v4i cmp = vals > target;
        
        bitmap[i * 4 + 0] = cmp[0] ? 1 : 0;
        bitmap[i * 4 + 1] = cmp[1] ? 1 : 0;
        bitmap[i * 4 + 2] = cmp[2] ? 1 : 0;
        bitmap[i * 4 + 3] = cmp[3] ? 1 : 0;
        
        if (i + 4 < vector_count) {
            __builtin_prefetch(&column[(i + 4) * 4], 0, 3);
        }
    }
    
    for (size_t i = vector_count * 4; i < count; i++) {
        bitmap[i] = (column[i] > value) ? 1 : 0;
    }
}

void simd_filter_eq_f64_vectorized(const double *column, size_t count, double value, uint8_t *bitmap) {
    size_t vector_count = count / 2;
    
    v2d target = {value, value};
    
    for (size_t i = 0; i < vector_count; i++) {
        v2d vals = *((v2d*)&column[i * 2]);
        v2d cmp = vals == target;
        
        bitmap[i * 2 + 0] = cmp[0] ? 1 : 0;
        bitmap[i * 2 + 1] = cmp[1] ? 1 : 0;
        
        if (i + 4 < vector_count) {
            __builtin_prefetch(&column[(i + 4) * 2], 0, 3);
        }
    }
    
    for (size_t i = vector_count * 2; i < count; i++) {
        bitmap[i] = (column[i] == value) ? 1 : 0;
    }
}

// Use vectorized versions by default on supported platforms
void simd_filter_eq_i32_fast(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    if (count >= 16) {
        simd_filter_eq_i32_vectorized(column, count, value, bitmap);
    } else {
        simd_filter_eq_i32(column, count, value, bitmap);
    }
}

void simd_filter_gt_i32_fast(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap) {
    if (count >= 16) {
        simd_filter_gt_i32_vectorized(column, count, value, bitmap);
    } else {
        simd_filter_gt_i32(column, count, value, bitmap);
    }
}

#endif
