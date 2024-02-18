#ifndef AWUTIL_AWASSERT_H
#define AWUTIL_AWASSERT_H

#include <stdlib.h>
#include "cdx_log.h"

/**
 * assert() equivalent, that is always enabled.
 */
#define aw_assert0(cond) do {                                           \
    if (!(cond)) {                                                      \
        logd("failed at %s:%d",    \
               __FILE__, __LINE__);                 \
        abort();                                                        \
    }                                                                   \
} while (0)


/**
 * assert() equivalent, that does not lie in speed critical code.
 * These asserts() thus can be enabled without fearing speed loss.
 */
#if defined(ASSERT_LEVEL) && ASSERT_LEVEL > 0
#define aw_assert1(cond) aw_assert0(cond)
#else
#define aw_assert1(cond) ((void)0)
#endif


/**
 * assert() equivalent, that does lie in speed critical code.
 */
#if defined(ASSERT_LEVEL) && ASSERT_LEVEL > 1
#define aw_assert2(cond) aw_assert0(cond)
#define aw_assert2_fpu() aw_assert0_fpu()
#else
#define aw_assert2(cond) ((void)0)
#define aw_assert2_fpu() ((void)0)
#endif

/**
 * Assert that floating point opperations can be executed.
 *
 * This will aw_assert0() that the cpu is not in MMX state on X86
 */
void aw_assert0_fpu(void);

#endif /* AWUTIL_AWASSERT_H */
