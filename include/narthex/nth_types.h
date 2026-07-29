#ifndef NTH_TYPES_H
#define NTH_TYPES_H

#include <float.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint8_t nth_u8;
typedef uint16_t nth_u16;
typedef uint32_t nth_u32;
typedef uint64_t nth_u64;

typedef int8_t nth_i8;
typedef int16_t nth_i16;
typedef int32_t nth_i32;
typedef int64_t nth_i64;

typedef nth_u8 nth_b8;

typedef float nth_f32;
typedef double nth_f64;

typedef uintptr_t nth_uptr;
typedef intptr_t  nth_iptr;
typedef size_t    nth_usize;
typedef ptrdiff_t nth_isize;


#define NTH_U8_MIN  ((nth_u8)0)
#define NTH_U8_MAX  UINT8_MAX
#define NTH_U16_MIN ((nth_u16)0)
#define NTH_U16_MAX UINT16_MAX
#define NTH_U32_MIN ((nth_u32)0)
#define NTH_U32_MAX UINT32_MAX
#define NTH_U64_MIN ((nth_u64)0)
#define NTH_U64_MAX UINT64_MAX

#define NTH_I8_MIN  INT8_MIN
#define NTH_I8_MAX  INT8_MAX
#define NTH_I16_MIN INT16_MIN
#define NTH_I16_MAX INT16_MAX
#define NTH_I32_MIN INT32_MIN
#define NTH_I32_MAX INT32_MAX
#define NTH_I64_MIN INT64_MIN
#define NTH_I64_MAX INT64_MAX

#define NTH_TRUE  ((nth_b8)1)
#define NTH_FALSE ((nth_b8)0)

#define NTH_F32_MIN     FLT_MIN
#define NTH_F32_MAX     FLT_MAX
#define NTH_F32_EPSILON FLT_EPSILON
#define NTH_F64_MIN     DBL_MIN
#define NTH_F64_MAX     DBL_MAX
#define NTH_F64_EPSILON DBL_EPSILON

#define NTH_USIZE_MAX SIZE_MAX
#define NTH_ISIZE_MIN PTRDIFF_MIN
#define NTH_ISIZE_MAX PTRDIFF_MAX
#define NTH_UPTR_MAX  UINTPTR_MAX
#define NTH_IPTR_MIN  INTPTR_MIN
#define NTH_IPTR_MAX  INTPTR_MAX


#ifdef __cplusplus
}
#endif

#endif /* NTH_TYPES_H */


#if defined(NTH_SHORT_TYPES) && !defined(NTH_TYPES_SHORT_DEFINED)
#define NTH_TYPES_SHORT_DEFINED

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u8  u8;
typedef nth_u16 u16;
typedef nth_u32 u32;
typedef nth_u64 u64;

typedef nth_i8  i8;
typedef nth_i16 i16;
typedef nth_i32 i32;
typedef nth_i64 i64;

typedef nth_b8 b8;

typedef nth_f32 f32;
typedef nth_f64 f64;

typedef nth_uptr  uptr;
typedef nth_iptr  iptr;
typedef nth_usize usize;
typedef nth_isize isize;


#define U8_MIN  NTH_U8_MIN
#define U8_MAX  NTH_U8_MAX
#define U16_MIN NTH_U16_MIN
#define U16_MAX NTH_U16_MAX
#define U32_MIN NTH_U32_MIN
#define U32_MAX NTH_U32_MAX
#define U64_MIN NTH_U64_MIN
#define U64_MAX NTH_U64_MAX

#define I8_MIN  NTH_I8_MIN
#define I8_MAX  NTH_I8_MAX
#define I16_MIN NTH_I16_MIN
#define I16_MAX NTH_I16_MAX
#define I32_MIN NTH_I32_MIN
#define I32_MAX NTH_I32_MAX
#define I64_MIN NTH_I64_MIN
#define I64_MAX NTH_I64_MAX

#define F32_MIN     NTH_F32_MIN
#define F32_MAX     NTH_F32_MAX
#define F32_EPSILON NTH_F32_EPSILON
#define F64_MIN     NTH_F64_MIN
#define F64_MAX     NTH_F64_MAX
#define F64_EPSILON NTH_F64_EPSILON

#define USIZE_MAX NTH_USIZE_MAX
#define ISIZE_MIN NTH_ISIZE_MIN
#define ISIZE_MAX NTH_ISIZE_MAX
#define UPTR_MAX  NTH_UPTR_MAX
#define IPTR_MIN  NTH_IPTR_MIN
#define IPTR_MAX  NTH_IPTR_MAX


#ifdef __cplusplus
}
#endif

#endif /* NTH_SHORT_TYPES */
