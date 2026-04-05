/*
 * disk_format.h - Description
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

#ifndef _CRYP_TEXT4_DISK_FORMAT_H  
#define _CRYP_TEXT4_DISK_FORMAT_H

#include <linux/types.h>

#define CRYPTEXT4_MAGIC 0xC7E4C7E4U
#define CRYPEXT4_VERSION 0x00000001U

#define CRYPTEXT4_SB_OFFSET 1024 /* Superblock offset in bytes */

/* super block struct (store offset 1024 byte, generally block one) */
struct cryptext4_super_block {
    __le32 s_magic; /* magic number */
    __le32 s_version; /* version number */
    __le32 s_blocksize; /* block size in byte */
    __le32 s_blocks_count; /* total blocks count */
    __le32 s_inodes_count; /* total inodes count */
    __le32 s_free_blocks; /* free blocks count */
    __le32 s_free_inodes; /* free inodes count */
    unsigned char s_salt[32]; /* salt for encryption */
    __le32 s_encrypt_algo;  /* encryption algorithm, 1 = AES-XTS */
    __le32 s_reserved[16]; /* reserved for future use */
};

#define CRYPTEXT4_SB_SIZE    sizeof(struct cryptext4_super_block)

/* Simplify inode (stage 2 only defines the structure, not used yet) */
struct cryptext4_inode {
    __le16 i_mode; /* file mode */
    __le32 i_uid; /* owner uid */
    __le32 i_size; /* size in byte */
    __le32 i_atime; /* access time */
    __le32 i_ctime; /* creation time */
    __le32 i_mtime; /* modification time */
    __le32 i_dtime; /* deletion time */
    __le32 i_gid; /* group id */
    __le32 i_links_count; /* links count */
    __le32 i_blocks; /* blocks count */
    __le32 i_flags; /* file flags */
    __le32 i_block[15]; /* pointers to blocks, 12 direct, 1 single indirect, 1 double indirect, 1 triple indirect */
    unsigned char i_file_salt[16]; /* per-file salt for encryption */
};

#endif /* _CRYP_TEXT4_DISK_FORMAT_H  */