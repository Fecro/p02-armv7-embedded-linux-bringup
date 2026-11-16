/**
 * @file c23_compat.h
 * @brief Compatibility layer and feature-test macros for ISO/IEC 9899:2024
 * (C23).
 *
 * In modern toolchains with full C23 support (-std=c23, GCC 14+), features such
 * as <stdckdint.h>, nullptr, constexpr, [[nodiscard]], and bool are native
 * primitives. When compiling against earlier toolchains (-std=c2x, GCC 12/13),
 * this header provides standard-conforming polyfills and fallbacks.
 */
#ifndef SYSMON_C23_COMPAT_H
#define SYSMON_C23_COMPAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Checked arithmetic operations: import C23 header or fallback to compiler
 * builtins */
#if defined(__has_include) && __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif

/* Native C23 keyword fallbacks for pre-C23 compilers */
#if !defined(__cplusplus) &&                                                   \
    (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 202311L))
#ifndef nullptr
#define nullptr ((void *)0)
#endif
#ifndef constexpr
#define constexpr static const
#endif
#endif

/* Unused-result attribute abstraction */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
#define SYSMON_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#define SYSMON_NODISCARD __attribute__((warn_unused_result))
#else
#define SYSMON_NODISCARD
#endif

#endif /* SYSMON_C23_COMPAT_H */
