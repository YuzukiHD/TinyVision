#include "awintmath.h"

/* undef these to get the function prototypes from common.h */
#undef aw_log2
#undef aw_log2_16bit
#include "awcommon.h"

int aw_log2(unsigned v)
{
    return aw_muxer_log2(v);
}

int aw_log2_16bit(unsigned v)
{
    return aw_muxer_log2_16bit(v);
}
