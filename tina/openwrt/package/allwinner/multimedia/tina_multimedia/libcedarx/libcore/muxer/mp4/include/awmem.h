#ifndef AWUTIL_MEM_H
#define AWUTIL_MEM_H

#include <limits.h>
#include <stdint.h>

#include "awattributes.h"
#include "awintmath.h"

#define AWERROR(e) (-(e))

#if defined(__INTEL_COMPILER) && __INTEL_COMPILER < 1110 || defined(__SUNPRO_C)
    #define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
    #define DECLARE_ASM_CONST(n,t,v)    const t __attribute__ ((aligned (n))) v
#elif defined(__TI_COMPILER_VERSION__)
    #define DECLARE_ALIGNED(n,t,v)                      \
        AW_PRAGMA(DATA_ALIGN(v,n))                      \
        t __attribute__((aligned(n))) v
    #define DECLARE_ASM_CONST(n,t,v)                    \
        AW_PRAGMA(DATA_ALIGN(v,n))                      \
        static const t __attribute__((aligned(n))) v
#elif defined(__DJGPP__)
    #define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (AWMIN(n, 16)))) v
    #define DECLARE_ASM_CONST(n,t,v)    static const t mux_used __attribute__ ((aligned (AWMIN(n, 16)))) v
#elif defined(__GNUC__) || defined(__clang__)
    #define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
    #define DECLARE_ASM_CONST(n,t,v)    static const t mux_used __attribute__ ((aligned (n))) v
#elif defined(_MSC_VER)
    #define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
    #define DECLARE_ASM_CONST(n,t,v)    __declspec(align(n)) static const t v
#else
    #define DECLARE_ALIGNED(n,t,v)      t v
    #define DECLARE_ASM_CONST(n,t,v)    static const t v
#endif

/**
 * @}
 */

/**
 * @defgroup lavu_mem_attrs Function Attributes
 * Function attributes applicable to memory handling functions.
 *
 * These function attributes can help compilers emit more useful warnings, or
 * generate better code.
 * @{
 */

/**
 * @def mux_malloc_attrib
 * Function attribute denoting a malloc-like function.
 *
 * @see <a href="https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-g_t_0040code_007bmalloc_007d-function-attribute-3251">Function attribute `malloc` in GCC's documentation</a>
 */

#if AW_GCC_VERSION_AT_LEAST(3,1)
    #define mux_malloc_attrib __attribute__((__malloc__))
#else
    #define mux_malloc_attrib
#endif

/**
 * @def mux_alloc_size(...)
 * Function attribute used on a function that allocates memory, whose size is
 * given by the specified parameter(s).
 *
 * @code{.c}
 * void *mux_malloc(size_t size) mux_alloc_size(1);
 * void *mux_calloc(size_t nmemb, size_t size) mux_alloc_size(1, 2);
 * @endcode
 *
 * @param ... One or two parameter indexes, separated by a comma
 *
 * @see <a href="https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-g_t_0040code_007balloc_005fsize_007d-function-attribute-3220">Function attribute `alloc_size` in GCC's documentation</a>
 */

#if AW_GCC_VERSION_AT_LEAST(4,3)
    #define mux_alloc_size(...) __attribute__((alloc_size(__VA_ARGS__)))
#else
    #define mux_alloc_size(...)
#endif

/**
 * @}
 */

/**
 * @defgroup lavu_mem_funcs Heap Management
 * Functions responsible for allocating, freeing, and copying memory.
 *
 * All memory allocation functions have a built-in upper limit of `INT_MAX`
 * bytes. This may be changed with mux_max_alloc(), although exercise extreme
 * caution when doing so.
 *
 * @{
 */

/**
 * Allocate a memory block with alignment suitable for all memory accesses
 * (including vectors if available on the CPU).
 *
 * @param size Size in bytes for the memory block to be allocated
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 * @see mux_mallocz()
 */
void *mux_malloc(size_t size) mux_malloc_attrib mux_alloc_size(1);

/**
 * Allocate a memory block with alignment suitable for all memory accesses
 * (including vectors if available on the CPU) and zero all the bytes of the
 * block.
 *
 * @param size Size in bytes for the memory block to be allocated
 * @return Pointer to the allocated block, or `NULL` if it cannot be allocated
 * @see mux_malloc()
 */
void *mux_mallocz(size_t size) mux_malloc_attrib mux_alloc_size(1);

/**
 * Allocate a memory block for an array with mux_malloc().
 *
 * The allocated memory will have size `size * nmemb` bytes.
 *
 * @param nmemb Number of element
 * @param size  Size of a single element
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 * @see mux_malloc()
 */
mux_alloc_size(1, 2) static inline void *mux_malloc_array(size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return mux_malloc(nmemb * size);
}

/**
 * Allocate a memory block for an array with mux_mallocz().
 *
 * The allocated memory will have size `size * nmemb` bytes.
 *
 * @param nmemb Number of elements
 * @param size  Size of the single element
 * @return Pointer to the allocated block, or `NULL` if the block cannot
 *         be allocated
 *
 * @see mux_mallocz()
 * @see mux_malloc_array()
 */
mux_alloc_size(1, 2) static inline void *mux_mallocz_array(size_t nmemb, size_t size)
{
    if (!size || nmemb >= INT_MAX / size)
        return NULL;
    return mux_mallocz(nmemb * size);
}

/**
 * Non-inlined equivalent of mux_mallocz_array().
 *
 * Created for symmetry with the calloc() C function.
 */
void *mux_calloc(size_t nmemb, size_t size) mux_malloc_attrib;

/**
 * Allocate, reallocate, or free a block of memory.
 *
 * If `ptr` is `NULL` and `size` > 0, allocate a new block. If `size` is
 * zero, free the memory block pointed to by `ptr`. Otherwise, expand or
 * shrink that block of memory according to `size`.
 *
 * @param ptr  Pointer to a memory block already allocated with
 *             mux_realloc() or `NULL`
 * @param size Size in bytes of the memory block to be allocated or
 *             reallocated
 *
 * @return Pointer to a newly-reallocated block or `NULL` if the block
 *         cannot be reallocated or the function is used to free the memory block
 *
 * @warning Unlike mux_malloc(), the returned pointer is not guaranteed to be
 *          correctly aligned.
 * @see mux_fast_realloc()
 * @see mux_reallocp()
 */
void *mux_realloc(void *ptr, size_t size) mux_alloc_size(2);

/**
 * Allocate, reallocate, or free a block of memory through a pointer to a
 * pointer.
 *
 * If `*ptr` is `NULL` and `size` > 0, allocate a new block. If `size` is
 * zero, free the memory block pointed to by `*ptr`. Otherwise, expand or
 * shrink that block of memory according to `size`.
 *
 * @param[in,out] ptr  Pointer to a pointer to a memory block already allocated
 *                     with mux_realloc(), or a pointer to `NULL`. The pointer
 *                     is updated on success, or freed on failure.
 * @param[in]     size Size in bytes for the memory block to be allocated or
 *                     reallocated
 *
 * @return Zero on success, an AWERROR error code on failure
 *
 * @warning Unlike mux_malloc(), the allocated memory is not guaranteed to be
 *          correctly aligned.
 */
int mux_reallocp(void *ptr, size_t size);

/**
 * Allocate, reallocate, or free a block of memory.
 *
 * This function does the same thing as mux_realloc(), except:
 * - It takes two size arguments and allocates `nelem * elsize` bytes,
 *   after checking the result of the multiplication for integer overflow.
 * - It frees the input block in case of failure, thus avoiding the memory
 *   leak with the classic
 *   @code{.c}
 *   buf = realloc(buf);
 *   if (!buf)
 *       return -1;
 *   @endcode
 *   pattern.
 */
void *mux_realloc_f(void *ptr, size_t nelem, size_t elsize);

/**
 * Allocate, reallocate, or free an array.
 *
 * If `ptr` is `NULL` and `nmemb` > 0, allocate a new block. If
 * `nmemb` is zero, free the memory block pointed to by `ptr`.
 *
 * @param ptr   Pointer to a memory block already allocated with
 *              mux_realloc() or `NULL`
 * @param nmemb Number of elements in the array
 * @param size  Size of the single element of the array
 *
 * @return Pointer to a newly-reallocated block or NULL if the block
 *         cannot be reallocated or the function is used to free the memory block
 *
 * @warning Unlike mux_malloc(), the allocated memory is not guaranteed to be
 *          correctly aligned.
 * @see mux_reallocp_array()
 */
mux_alloc_size(2, 3) void *mux_realloc_array(void *ptr, size_t nmemb, size_t size);

/**
 * Allocate, reallocate, or free an array through a pointer to a pointer.
 *
 * If `*ptr` is `NULL` and `nmemb` > 0, allocate a new block. If `nmemb` is
 * zero, free the memory block pointed to by `*ptr`.
 *
 * @param[in,out] ptr   Pointer to a pointer to a memory block already
 *                      allocated with mux_realloc(), or a pointer to `NULL`.
 *                      The pointer is updated on success, or freed on failure.
 * @param[in]     nmemb Number of elements
 * @param[in]     size  Size of the single element
 *
 * @return Zero on success, an AWERROR error code on failure
 *
 * @warning Unlike mux_malloc(), the allocated memory is not guaranteed to be
 *          correctly aligned.
 */
mux_alloc_size(2, 3) int mux_reallocp_array(void *ptr, size_t nmemb, size_t size);

/**
 * Reallocate the given buffer if it is not large enough, otherwise do nothing.
 *
 * If the given buffer is `NULL`, then a new uninitialized buffer is allocated.
 *
 * If the given buffer is not large enough, and reallocation fails, `NULL` is
 * returned and `*size` is set to 0, but the original buffer is not changed or
 * freed.
 *
 * A typical use pattern follows:
 *
 * @code{.c}
 * uint8_t *buf = ...;
 * uint8_t *new_buf = mux_fast_realloc(buf, &current_size, size_needed);
 * if (!new_buf) {
 *     // Allocation failed; clean up original buffer
 *     mux_freep(&buf);
 *     return AWERROR(ENOMEM);
 * }
 * @endcode
 *
 * @param[in,out] ptr      Already allocated buffer, or `NULL`
 * @param[in,out] size     Pointer to current size of buffer `ptr`. `*size` is
 *                         changed to `min_size` in case of success or 0 in
 *                         case of failure
 * @param[in]     min_size New size of buffer `ptr`
 * @return `ptr` if the buffer is large enough, a pointer to newly reallocated
 *         buffer if the buffer was not large enough, or `NULL` in case of
 *         error
 * @see mux_realloc()
 * @see mux_fast_malloc()
 */
void *mux_fast_realloc(void *ptr, unsigned int *size, size_t min_size);

/**
 * Allocate a buffer, reusing the given one if large enough.
 *
 * Contrary to mux_fast_realloc(), the current buffer contents might not be
 * preserved and on error the old buffer is freed, thus no special handling to
 * avoid memleaks is necessary.
 *
 * `*ptr` is allowed to be `NULL`, in which case allocation always happens if
 * `size_needed` is greater than 0.
 *
 * @code{.c}
 * uint8_t *buf = ...;
 * mux_fast_malloc(&buf, &current_size, size_needed);
 * if (!buf) {
 *     // Allocation failed; buf already freed
 *     return AWERROR(ENOMEM);
 * }
 * @endcode
 *
 * @param[in,out] ptr      Pointer to pointer to an already allocated buffer.
 *                         `*ptr` will be overwritten with pointer to new
 *                         buffer on success or `NULL` on failure
 * @param[in,out] size     Pointer to current size of buffer `*ptr`. `*size` is
 *                         changed to `min_size` in case of success or 0 in
 *                         case of failure
 * @param[in]     min_size New size of buffer `*ptr`
 * @see mux_realloc()
 * @see mux_fast_mallocz()
 */
void mux_fast_malloc(void *ptr, unsigned int *size, size_t min_size);

/**
 * Allocate and clear a buffer, reusing the given one if large enough.
 *
 * Like mux_fast_malloc(), but all newly allocated space is initially cleared.
 * Reused buffer is not cleared.
 *
 * `*ptr` is allowed to be `NULL`, in which case allocation always happens if
 * `size_needed` is greater than 0.
 *
 * @param[in,out] ptr      Pointer to pointer to an already allocated buffer.
 *                         `*ptr` will be overwritten with pointer to new
 *                         buffer on success or `NULL` on failure
 * @param[in,out] size     Pointer to current size of buffer `*ptr`. `*size` is
 *                         changed to `min_size` in case of success or 0 in
 *                         case of failure
 * @param[in]     min_size New size of buffer `*ptr`
 * @see mux_fast_malloc()
 */
void mux_fast_mallocz(void *ptr, unsigned int *size, size_t min_size);

/**
 * Free a memory block which has been allocated with a function of mux_malloc()
 * or mux_realloc() family.
 *
 * @param ptr Pointer to the memory block which should be freed.
 *
 * @note `ptr = NULL` is explicitly allowed.
 * @note It is recommended that you use mux_freep() instead, to prevent leaving
 *       behind dangling pointers.
 * @see mux_freep()
 */
void mux_free(void *ptr);

/**
 * Free a memory block which has been allocated with a function of mux_malloc()
 * or mux_realloc() family, and set the pointer pointing to it to `NULL`.
 *
 * @code{.c}
 * uint8_t *buf = mux_malloc(16);
 * mux_free(buf);
 * // buf now contains a dangling pointer to freed memory, and accidental
 * // dereference of buf will result in a use-after-free, which may be a
 * // security risk.
 *
 * uint8_t *buf = mux_malloc(16);
 * mux_freep(&buf);
 * // buf is now NULL, and accidental dereference will only result in a
 * // NULL-pointer dereference.
 * @endcode
 *
 * @param ptr Pointer to the pointer to the memory block which should be freed
 * @note `*ptr = NULL` is safe and leads to no action.
 * @see mux_free()
 */
void mux_freep(void *ptr);

/**
 * Duplicate a string.
 *
 * @param s String to be duplicated
 * @return Pointer to a newly-allocated string containing a
 *         copy of `s` or `NULL` if the string cannot be allocated
 * @see mux_strndup()
 */
char *mux_strdup(const char *s) mux_malloc_attrib;

/**
 * Duplicate a substring of a string.
 *
 * @param s   String to be duplicated
 * @param len Maximum length of the resulting string (not counting the
 *            terminating byte)
 * @return Pointer to a newly-allocated string containing a
 *         substring of `s` or `NULL` if the string cannot be allocated
 */
char *mux_strndup(const char *s, size_t len) mux_malloc_attrib;

/**
 * Duplicate a buffer with mux_malloc().
 *
 * @param p    Buffer to be duplicated
 * @param size Size in bytes of the buffer copied
 * @return Pointer to a newly allocated buffer containing a
 *         copy of `p` or `NULL` if the buffer cannot be allocated
 */
void *mux_memdup(const void *p, size_t size);

/**
 * Overlapping memcpy() implementation.
 *
 * @param dst  Destination buffer
 * @param back Number of bytes back to start copying (i.e. the initial size of
 *             the overlapping window); must be > 0
 * @param cnt  Number of bytes to copy; must be >= 0
 *
 * @note `cnt > back` is valid, this will copy the bytes we just copied,
 *       thus creating a repeating pattern with a period length of `back`.
 */
void mux_memcpy_backptr(uint8_t *dst, int back, int cnt);

/**
 * @}
 */

/**
 * @defgroup lavu_mem_dynarray Dynamic Array
 *
 * Utilities to make an array grow when needed.
 *
 * Sometimes, the programmer would want to have an array that can grow when
 * needed. The libavutil dynamic array utilities fill that need.
 *
 * libavutil supports two systems of appending elements onto a dynamically
 * allocated array, the first one storing the pointer to the value in the
 * array, and the second storing the value directly. In both systems, the
 * caller is responsible for maintaining a variable containing the length of
 * the array, as well as freeing of the array after use.
 *
 * The first system stores pointers to values in a block of dynamically
 * allocated memory. Since only pointers are stored, the function does not need
 * to know the size of the type. Both mux_dynarray_add() and
 * mux_dynarray_add_nofree() implement this system.
 *
 * @code
 * type **array = NULL; //< an array of pointers to values
 * int    nb    = 0;    //< a variable to keep track of the length of the array
 *
 * type to_be_added  = ...;
 * type to_be_added2 = ...;
 *
 * mux_dynarray_add(&array, &nb, &to_be_added);
 * if (nb == 0)
 *     return AWERROR(ENOMEM);
 *
 * mux_dynarray_add(&array, &nb, &to_be_added2);
 * if (nb == 0)
 *     return AWERROR(ENOMEM);
 *
 * // Now:
 * //  nb           == 2
 * // &to_be_added  == array[0]
 * // &to_be_added2 == array[1]
 *
 * mux_freep(&array);
 * @endcode
 *
 * The second system stores the value directly in a block of memory. As a
 * result, the function has to know the size of the type. mux_dynarray2_add()
 * implements this mechanism.
 *
 * @code
 * type *array = NULL; //< an array of values
 * int   nb    = 0;    //< a variable to keep track of the length of the array
 *
 * type to_be_added  = ...;
 * type to_be_added2 = ...;
 *
 * type *addr = mux_dynarray2_add((void **)&array, &nb, sizeof(*array), NULL);
 * if (!addr)
 *     return AWERROR(ENOMEM);
 * memcpy(addr, &to_be_added, sizeof(to_be_added));
 *
 * // Shortcut of the above.
 * type *addr = mux_dynarray2_add((void **)&array, &nb, sizeof(*array),
 *                               (const void *)&to_be_added2);
 * if (!addr)
 *     return AWERROR(ENOMEM);
 *
 * // Now:
 * //  nb           == 2
 * //  to_be_added  == array[0]
 * //  to_be_added2 == array[1]
 *
 * mux_freep(&array);
 * @endcode
 *
 * @{
 */

/**
 * Add the pointer to an element to a dynamic array.
 *
 * The array to grow is supposed to be an array of pointers to
 * structures, and the element to add must be a pointer to an already
 * allocated structure.
 *
 * The array is reallocated when its size reaches powers of 2.
 * Therefore, the amortized cost of adding an element is constant.
 *
 * In case of success, the pointer to the array is updated in order to
 * point to the new grown array, and the number pointed to by `nb_ptr`
 * is incremented.
 * In case of failure, the array is freed, `*tab_ptr` is set to `NULL` and
 * `*nb_ptr` is set to 0.
 *
 * @param[in,out] tab_ptr Pointer to the array to grow
 * @param[in,out] nb_ptr  Pointer to the number of elements in the array
 * @param[in]     elem    Element to add
 * @see mux_dynarray_add_nofree(), mux_dynarray2_add()
 */
void mux_dynarray_add(void *tab_ptr, int *nb_ptr, void *elem);

/**
 * Add an element to a dynamic array.
 *
 * Function has the same functionality as mux_dynarray_add(),
 * but it doesn't free memory on fails. It returns error code
 * instead and leave current buffer untouched.
 *
 * @return >=0 on success, negative otherwise
 * @see mux_dynarray_add(), mux_dynarray2_add()
 */
int mux_dynarray_add_nofree(void *tab_ptr, int *nb_ptr, void *elem);
#if 0
/**
 * Add an element of size `elem_size` to a dynamic array.
 *
 * The array is reallocated when its number of elements reaches powers of 2.
 * Therefore, the amortized cost of adding an element is constant.
 *
 * In case of success, the pointer to the array is updated in order to
 * point to the new grown array, and the number pointed to by `nb_ptr`
 * is incremented.
 * In case of failure, the array is freed, `*tab_ptr` is set to `NULL` and
 * `*nb_ptr` is set to 0.
 *
 * @param[in,out] tab_ptr   Pointer to the array to grow
 * @param[in,out] nb_ptr    Pointer to the number of elements in the array
 * @param[in]     elem_size Size in bytes of an element in the array
 * @param[in]     elem_data Pointer to the data of the element to add. If
 *                          `NULL`, the space of the newly added element is
 *                          allocated but left uninitialized.
 *
 * @return Pointer to the data of the element to copy in the newly allocated
 *         space
 * @see mux_dynarray_add(), mux_dynarray_add_nofree()
 */
void *mux_dynarray2_add(void **tab_ptr, int *nb_ptr, size_t elem_size,
                       const uint8_t *elem_data);
#endif
/**
 * @}
 */

/**
 * @defgroup lavu_mem_misc Miscellaneous Functions
 *
 * Other functions related to memory allocation.
 *
 * @{
 */

/**
 * Multiply two `size_t` values checking for overflow.
 *
 * @param[in]  a,b Operands of multiplication
 * @param[out] r   Pointer to the result of the operation
 * @return 0 on success, AWERROR(EINVAL) on overflow
 */
static inline int mux_size_mult(size_t a, size_t b, size_t *r)
{
    size_t t = a * b;
    /* Hack inspired from glibc: don't try the division if nelem and elsize
     * are both less than sqrt(SIZE_MAX). */
    if ((a | b) >= ((size_t)1 << (sizeof(size_t) * 4)) && a && t / a != b)
        return AWERROR(EINVAL);
    *r = t;
    return 0;
}

/**
 * Set the maximum size that may be allocated in one block.
 *
 * The value specified with this function is effective for all libavutil's @ref
 * lavu_mem_funcs "heap management functions."
 *
 * By default, the max value is defined as `INT_MAX`.
 *
 * @param max Value to be set as the new maximum size
 *
 * @warning Exercise extreme caution when using this function. Don't touch
 *          this if you do not understand the full consequence of doing so.
 */
void mux_max_alloc(size_t max);

/**
 * @}
 * @}
 */

#endif /* AWUTIL_MEM_H */
