#ifndef NTH_TYPES_H
#define NTH_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <float.h>
#include <stddef.h>
#include <stdint.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef u8 b8;

typedef float f32;
typedef double f64;

typedef uintptr_t uptr;
typedef intptr_t  iptr;
typedef size_t    usize;
typedef ptrdiff_t isize;


#define U8_MIN  ((u8)0)
#define U8_MAX  UINT8_MAX
#define U16_MIN ((u16)0)
#define U16_MAX UINT16_MAX
#define U32_MIN ((u32)0)
#define U32_MAX UINT32_MAX
#define U64_MIN ((u64)0)
#define U64_MAX UINT64_MAX

#define I8_MIN  INT8_MIN
#define I8_MAX  INT8_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

#define F32_MIN     FLT_MIN
#define F32_MAX     FLT_MAX
#define F32_EPSILON FLT_EPSILON
#define F64_MIN     DBL_MIN
#define F64_MAX     DBL_MAX
#define F64_EPSILON DBL_EPSILON

#define USIZE_MAX SIZE_MAX
#define ISIZE_MIN PTRDIFF_MIN
#define ISIZE_MAX PTRDIFF_MAX
#define UPTR_MAX  UINTPTR_MAX
#define IPTR_MIN  INTPTR_MIN
#define IPTR_MAX  INTPTR_MAX


#ifdef __cplusplus
}
#endif

#endif /* NTH_TYPES_H */