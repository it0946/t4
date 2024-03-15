#ifndef T4_H_
#define T4_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// TODO setup compiler specific defines, such as T4_INLINE to __attribute__((always_inline)) and such and T4_API

#if defined(T4_DEBUG)

#define t4_debug_assert(expr) assert((expr))

#else

#define t4_debug_assert(expr)

#endif /* T4_DEBUG */

#endif /* T4_H_ */
