#pragma once

#ifndef _RESTRICT_
#if defined(restrict) || \
    ((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)))
#define _RESTRICT_ restrict
#elif defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#define _RESTRICT_ __restrict
#else
#define _RESTRICT_
#endif
#endif

#ifndef _HAS_BUILTIN_
#ifdef __has_builtin
#define _HAS_BUILTIN_(x) __has_builtin(x)
#else
#define _HAS_BUILTIN_(x) 0
#endif
#endif

#ifndef _DEPRECATED
#if defined(__GNUC__) && \
    (__GNUC__ >= 4) /* technically, this arrived in gcc 3.1, but oh well. */
#define _DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define _DEPRECATED __declspec(deprecated)
#else
#define _DEPRECATED
#endif
#endif

#ifndef _NO_INLINE_
#if defined(__GNUC__)
#define _NO_INLINE_ __attribute__((noinline))
#elif defined(_MSC_VER)
#define _NO_INLINE_ __declspec(noinline)
#else
#define _NO_INLINE_
#endif
#endif

#ifndef _ALWAYS_INLINE_
#if defined(__GNUC__)
#define _ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _ALWAYS_INLINE_ __forceinline
#else
#define _ALWAYS_INLINE_ inline
#endif
#endif

// Should always inline, except in dev builds because it makes debugging harder,
#ifndef _FORCE_INLINE_
#if defined(VR_DEBUG)
#define _FORCE_INLINE_ inline
#else
#define _FORCE_INLINE_ _ALWAYS_INLINE_
#endif
#endif

#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#ifndef _STR
#define _STR(m_x) #m_x
#define _MKSTR(m_x) _STR(m_x)
#endif
