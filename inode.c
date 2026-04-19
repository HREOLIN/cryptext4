/*
 * inode.c - Description
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
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/printk.h>
#include "cryptext4.h"

static struct buffer_head *cryptext4_get_inode_loc(struct super_block *sb, ino_t ino, unsigned long *inode_offset)
{
    struct cryptext4_sb_info *sbi = CRYPTEXT4_SB(sb);
    struct buffer_head *bh;
    unsigned long inode_block;
    unsigned long inode_index;
    unsigned long inodes_per_block;
    unsigned long inode_size;
    
    if(ino == 0) {
        pr_err("cryptext4: invalid inode number %lu\n", ino);
        return ERR_PTR(-EINVAL);
    }

    inode_size = sizeof(struct cryptext4_inode);
    inodes_per_block = sb->s_blocksize / inode_size;
    inode_index = ino - 1; // Inode numbers start at 1
    inode_block = sbi->inode_table_start_block + (inode_index / inodes_per_block);
    *inode_offset = (inode_index % inodes_per_block) * inode_size;

    pr_info("cryptext4: inode %lu located at block %lu, offset %lu\n", ino, inode_block, *inode_offset);

    bh = sb_bread(sb, inode_block);
    if (!bh) {
        pr_err("cryptext4: failed to read inode block %lu for inode %lu\n", inode_block, ino);
        return ERR_PTR(-EIO);
    }
    return bh;
}

struct buffer_head *cryptext4_read_inode_bitmap_or_table(struct super_block *sb, 
                    ino_t ino, struct cryptext4_inode **disk_inode)
{
    struct buffer_head *bh;
    unsigned long offset;

    bh = cryptext4_get_inode_loc(sb, ino, &offset);
    if (IS_ERR(bh)) {
        return bh; /* Error already logged in cryptext4_get_inode_loc */
    }

    if (disk_inode) {
        *disk_inode = (struct cryptext4_inode *)(bh->b_data + offset);
    }
    return bh; /* Caller is responsible for brelse() */
}

