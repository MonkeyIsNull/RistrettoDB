#ifndef RISTRETTO_SIMD_H
#define RISTRETTO_SIMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __clang__
typedef int32_t v4i __attribute__((vector_size(16)));
typedef int64_t v2i64 __attribute__((vector_size(16)));
typedef float v4f __attribute__((vector_size(16)));
typedef double v2d __attribute__((vector_size(16)));
#endif

void simd_filter_eq_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap);
void simd_filter_gt_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap);
void simd_filter_lt_i32(const int32_t *column, size_t count, int32_t value, uint8_t *bitmap);

void simd_filter_eq_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap);
void simd_filter_gt_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap);
void simd_filter_lt_i64(const int64_t *column, size_t count, int64_t value, uint8_t *bitmap);

void simd_filter_eq_f64(const double *column, size_t count, double value, uint8_t *bitmap);
void simd_filter_gt_f64(const double *column, size_t count, double value, uint8_t *bitmap);
void simd_filter_lt_f64(const double *column, size_t count, double value, uint8_t *bitmap);

void simd_bitmap_and(const uint8_t *a, const uint8_t *b, uint8_t *result, size_t count);
void simd_bitmap_or(const uint8_t *a, const uint8_t *b, uint8_t *result, size_t count);

size_t simd_count_set_bits(const uint8_t *bitmap, size_t count);

#endif
