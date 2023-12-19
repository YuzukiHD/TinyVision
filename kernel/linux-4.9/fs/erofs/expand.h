/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017-2018 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 */
#ifndef __EROFS_EXPAND_H
#define __EROFS_EXPAND_H

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "erofs_fs.h"
#include "xarray.h"
#include <linux/overflow.h>
#include "fs_types.h"

/*custom*/
#define bio_set_dev(bio, bdev) ((bio)->bi_bdev = (bdev))

static inline void *kvzalloc(size_t size, gfp_t flags)
{
	void *ret;

	ret = kzalloc(size, flags | __GFP_NOWARN);
	if (!ret)
		ret = __vmalloc(size, flags | __GFP_ZERO, PAGE_KERNEL);
	return ret;
}

static inline void *kvmalloc(size_t size, gfp_t flags)
{
	void *ret;

	ret = kmalloc(size, flags | __GFP_NOWARN);
	if (!ret)
		ret = __vmalloc(size, flags, PAGE_KERNEL);
	return ret;
}

static inline void *kvmalloc_array(size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;

	return kvmalloc(bytes, flags);
}

#define lru_to_page(head) (list_entry((head)->prev, struct page, lru))

/**
 *  * clear_and_wake_up_bit - clear a bit and wake up anyone waiting on that bit
 *   *
 *    * @bit: the bit of the word being waited on
 *     * @word: the word being waited on, a kernel virtual address
 *      *
 *       * You can use this helper if bitflags are manipulated atomically rather than
 *        * non-atomically under a lock.
 *         */
static inline void clear_and_wake_up_bit(int bit, void *word)
{
	clear_bit_unlock(bit, word);
	/* See wake_up_bit() for which memory barrier you need to use. */
	smp_mb__after_atomic();
	wake_up_bit(word, bit);
}

#define FT_DIR               2

/*errno.h*/
#define ENOPARAM        519     /* Parameter not supported */

/*fs.h*/
/*
    + * sb->s_flags.  Note that these mirror the equivalent MS_* flags where
    + * represented in both.
    + */
#define SB_RDONLY       1      /* Mount read-only */
#define SB_NOSUID       2      /* Ignore suid and sgid bits */
#define SB_NODEV        4      /* Disallow access to device special files */
#define SB_NOEXEC       8      /* Disallow program execution */
#define SB_SYNCHRONOUS 16      /* Writes are synced at once */
#define SB_MANDLOCK    64      /* Allow mandatory locks on an FS */
#define SB_DIRSYNC     128     /* Directory modifications are synchronous */
#define SB_NOATIME     1024    /* Do not update access times. */
#define SB_NODIRATIME  2048    /* Do not update directory access times */
#define SB_SILENT      32768
#define SB_POSIXACL    (1<<16) /* VFS does not apply the umask */
#define SB_INLINECRYPT (1<<17) /* Use blk-crypto for encrypted files */
#define SB_KERNMOUNT   (1<<22) /* this is a kern_mount call */
#define SB_I_VERSION   (1<<23) /* Update inode I_version field */
#define SB_LAZYTIME    (1<<25) /* Update the on-disk [acm]times lazily */

/* These sb flags are internal to the kernel */
#define SB_SUBMOUNT     (1<<26)
#define SB_FORCE       (1<<27)
#define SB_NOSEC       (1<<28)
#define SB_BORN                (1<<29)
#define SB_ACTIVE      (1<<30)
#define SB_NOUSER      (1<<31)

/* These flags relate to encoding and casefolding */
#define SB_ENC_STRICT_MODE_FL  (1 << 0)
static inline bool sb_rdonly(const struct super_block *sb) { return sb->s_flags & SB_RDONLY; }

/*iomap.h*/
/*
 * Magic value for addr:
 */
#define IOMAP_NULL_ADDR -1ULL   /* addr is not valid */

/*magic.h*/
#define EROFS_SUPER_MAGIC_V1    0xE0F5E1E2

#endif	/* __EROFS_EXPAND_H */

