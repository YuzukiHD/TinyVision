#ifndef AWUTIL_MACROS_H
#define AWUTIL_MACROS_H

#define AW_STRINGIFY(s)         AW_TOSTRING(s)
#define AW_TOSTRING(s) #s

#define AW_GLUE(a, b) a ## b
#define AW_JOIN(a, b) AW_GLUE(a, b)

/**
 * @}
 */

#define AW_PRAGMA(s) _Pragma(#s)

#define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))

#endif /* AWUTIL_MACROS_H */
