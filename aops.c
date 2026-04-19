/*
 * aops.c - Description
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
#include <linux/mpage.h>
#include <linux/pagemap.h>
#include "cryptext4.h"

static int cryptext4_readpage(struct file *file, struct page *page)
{
    return block_read_full_page(page, cryptext4_get_block);
}

static int cryptext4_writepage(struct page *page,
                               struct writeback_control *wbc)
{
    return block_write_full_page(page, cryptext4_get_block, wbc);
}

static int cryptext4_write_begin(struct file *file,
                                 struct address_space *mapping,
                                 loff_t pos, unsigned int len,
                                 unsigned int flags,
                                 struct page **pagep, void **fsdata)
{
    return block_write_begin(mapping, pos, len, flags,
                             pagep, cryptext4_get_block);
}

static sector_t cryptext4_bmap(struct address_space *mapping,
                               sector_t block)
{
    return generic_block_bmap(mapping, block, cryptext4_get_block);
}

/* ====================== get_block - 最核心函数 ====================== */
int cryptext4_get_block(struct inode *inode, sector_t iblock,
                        struct buffer_head *bh_result, int create)
{
    struct cryptext4_inode_info *ci = CRYPTEXT4_I(inode);
    struct super_block *sb = inode->i_sb;
    unsigned long phys_block = 0;

    pr_info("cryptext4_get_block: inode=%lu, logical_block=%llu, create=%d\n",
            inode->i_ino, (u64)iblock, create);
    
    if(iblock < CRYPTEXT4_NDIR_BLOCKS) {
        phys_block = le32_to_cpu(ci->i_block[iblock]);
        if (phys_block == 0 && create) {
            phys_block = cryptext4_alloc_block(sb, inode->i_ino);
            if (phys_block == 0) {
                pr_err("cryptext4: block allocation failed for inode %lu\n", inode->i_ino);
                return -ENOSPC;
            }

            ci->i_block[iblock] = cpu_to_le32(phys_block);
            mark_inode_dirty(inode);        // 重要：标记 inode 需要写回磁盘
            pr_info("cryptext4: allocated direct block %lu for iblock %llu\n", 
                    phys_block, (u64)iblock);
        }
    }else {
        pr_warn("cryptext4_get_block: indirect blocks not implemented yet (iblock=%llu)\n", (u64)iblock);
        if(create) {
            pr_err("cryptext4: cannot create indirect blocks yet\n");
            return -ENOSPC;
        }
        return 0;
    }

    if(phys_block == 0) {
        pr_info("cryptext4_get_block: block not allocated for iblock %llu\n", (u64)iblock);
        return 0; // block not allocated
    }
    map_bh(bh_result, sb, phys_block);
    if(create) {
        set_buffer_new(bh_result);
    }
    return 0;
}

unsigned long cryptext4_alloc_block(struct super_block *sb, ino_t ino)
{
    pr_warn("cryptext4_alloc_block: block allocation not implemented yet for inode %lu\n", ino);
    return 0; // TODO: implement block allocation logic
}

/* ====================== Address Space Operations ====================== */
const struct address_space_operations cryptext4_aops = {
    .readpage       = cryptext4_readpage,
    .write_begin    = cryptext4_write_begin,
    .write_end      = block_write_end,
    .writepage      = cryptext4_writepage,
    .bmap           = cryptext4_bmap,
    .set_page_dirty = __set_page_dirty_buffers,
};