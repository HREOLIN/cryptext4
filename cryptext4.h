/*
 * cryptext4.h - Description
 *
 * Copyright (C) 2026 Carl.Hu <hcllht@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _CRYPTEXT4_H
#define _CRYPTEXT4_H

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include "disk_format.h"

/* ====================== In-memory inode private info ====================== */
struct cryptext4_inode_info {
    struct inode vfs_inode; /* VFS inode */

    __le32 i_block[15]; /* 15 direct blocks for simplicity */
    unsigned char i_file_salt[16]; /* per-file salt for encryption */
    __le32 i_encrypt_algo; /* encryption algorithm, 1 = AES-XTS */
    __le32 i_flags; /* custom flags for encryption, etc. */

     /* Stage 3: add more fields as needed, e.g. for block mapping, extent trees, etc. */
     uint32_t data_block; /* For simplicity, we use a single block for file data in this stage */
    /* optimized ... */
};

#define CRYPTEXT4_I(inode) container_of(inode, struct cryptext4_inode_info, vfs_inode)
#define CRYPTEXT4_SB(sb) ((struct cryptext4_sb_info *)(sb)->s_fs_info)

#define CRYPTEXT4_NDIR_BLOCKS 12

#define CRYPTEXT4_IND_BLOCKS 12
#define CRYPTEXT4_DIND_BLOCKS 13
#define CRYPTEXT4_TIND_BLOCKS 14


/* =============== Superblock information ============== */
struct cryptext4_sb_info {
    struct super_block *sb;
    struct cryptext4_super_block *raw_sb;   /* raw superblock from disk */
    void *private;                          /* reserved for encryption keys, etc. */

    /* Stage 3 */
    unsigned long *inode_bitmap; /* Bitmap for inode allocation */
    unsigned long *block_bitmap; /* Bitmap for block allocation */
    uint32_t inode_table_start_block; /* Starting block of inode table */
};

struct buffer_head *cryptext4_read_inode_bitmap_or_table(struct super_block *sb,  ino_t ino, struct cryptext4_inode **disk_inodeS);
extern const struct address_space_operations cryptext4_aops;   /* 声明 aops */
unsigned long cryptext4_alloc_block(struct super_block *sb, ino_t ino);

/* ======================= aops ========================= */
int cryptext4_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create);

#endif /* _CRYPTEXT4_H */