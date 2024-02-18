#ifndef AWUTIL_ATTRIBUTES_H
#define AWUTIL_ATTRIBUTES_H

#ifdef __GNUC__
#    define AW_GCC_VERSION_AT_LEAST(x,y) (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#    define AW_GCC_VERSION_AT_MOST(x,y)  (__GNUC__ < (x) || __GNUC__ == (x) && __GNUC_MINOR__ <= (y))
#else
#    define AW_GCC_VERSION_AT_LEAST(x,y) 0
#    define AW_GCC_VERSION_AT_MOST(x,y)  0
#endif

#ifndef aw_always_inline
#if AW_GCC_VERSION_AT_LEAST(3,1)
#    define aw_always_inline __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#    define aw_always_inline __forceinline
#else
#    define aw_always_inline inline
#endif
#endif

#ifndef aw_extern_inline
#if defined(__ICL) && __ICL >= 1210 || defined(__GNUC_STDC_INLINE__)
#    define aw_extern_inline extern inline
#else
#    define aw_extern_inline inline
#endif
#endif

#if AW_GCC_VERSION_AT_LEAST(3,4)
#    define aw_warn_unused_result __attribute__((warn_unused_result))
#else
#    define aw_warn_unused_result
#endif

#if AW_GCC_VERSION_AT_LEAST(3,1)
#    define aw_noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#    define aw_noinline __declspec(noinline)
#else
#    define aw_noinline
#endif

#if AW_GCC_VERSION_AT_LEAST(3,1)
#    define aw_pure __attribute__((pure))
#else
#    define aw_pure
#endif

#if AW_GCC_VERSION_AT_LEAST(2,6)
#    define aw_const __attribute__((const))
#else
#    define aw_const
#endif

#if AW_GCC_VERSION_AT_LEAST(4,3)
#    define aw_cold __attribute__((cold))
#else
#    define aw_cold
#endif

#if AW_GCC_VERSION_AT_LEAST(4,1) && !defined(__llvm__)
#    define aw_flatten __attribute__((flatten))
#else
#    define aw_flatten
#endif

#if AW_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_deprecated __attribute__((deprecated))
#elif defined(_MSC_VER)
#    define attribute_deprecated __declspec(deprecated)
#else
#    define attribute_deprecated
#endif

/**
 * Disable warnings about deprecated features
 * This is useful for sections of code kept for backward compatibility and
 * scheduled for removal.
 */
#ifndef AW_NOWARN_DEPRECATED
#if AW_GCC_VERSION_AT_LEAST(4,6)
#    define AW_NOWARN_DEPRECATED(code) \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"") \
        code \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#    define AW_NOWARN_DEPRECATED(code) \
        __pragma(warning(push)) \
        __pragma(warning(disable : 4996)) \
        code; \
        __pragma(warning(pop))
#else
#    define AW_NOWARN_DEPRECATED(code) code
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define aw_unused __attribute__((unused))
#else
#    define aw_unused
#endif

/**
 * Mark a variable as used and prevent the compiler from optimizing it
 * away.  This is useful for variables accessed only from inline
 * assembler without the compiler being aware.
 */
#if AW_GCC_VERSION_AT_LEAST(3,1) || defined(__clang__)
#    define aw_used __attribute__((used))
#else
#    define aw_used
#endif

#if AW_GCC_VERSION_AT_LEAST(3,3)
#   define aw_alias __attribute__((may_alias))
#else
#   define aw_alias
#endif

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && !defined(__clang__)
#    define aw_uninit(x) x=x
#else
#    define aw_uninit(x) x
#endif

#ifdef __GNUC__
#    define aw_builtin_constant_p __builtin_constant_p
#    define aw_printf_format(fmtpos, attrpos) __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#    define aw_builtin_constant_p(x) 0
#    define aw_printf_format(fmtpos, attrpos)
#endif

#if AW_GCC_VERSION_AT_LEAST(2,5)
#    define aw_noreturn __attribute__((noreturn))
#else
#    define aw_noreturn
#endif

#endif /* AWUTIL_ATTRIBUTES_H */
