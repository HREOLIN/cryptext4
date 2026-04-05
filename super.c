/*
 * super.c - Description
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

/* cryptext4_super.c */
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/kernel.h>

static struct inode *cryptext4_alloc_inode(struct super_block *sb)
{
    struct inode *inode;

    inode = new_inode(sb);  // 使用内核默认分配器
    if (!inode)
        return NULL;

    /* 初始化 inode 时间 */
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    return inode;
}

/* destroy_inode 期望返回 void，而 generic_delete_inode 返回 int */
static void cryptext4_destroy_inode(struct inode *inode)
{
    generic_delete_inode(inode);
}


/* statfs 回调，返回文件系统统计信息 */
static int cryptext4_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    return simple_statfs(dentry, buf);
}

/* put_super 回调 */
static void cryptext4_put_super(struct super_block *sb)
{
    printk(KERN_INFO "cryptext4: superblock released\n");
    kill_block_super(sb);  // 默认释放块设备 superblock
}

/* stage 2: superblock operations */
static const struct super_operations cryptext4_sops = {
    .alloc_inode   = cryptext4_alloc_inode,
    .destroy_inode = cryptext4_destroy_inode,
    .drop_inode    = generic_delete_inode,
    .statfs        = cryptext4_statfs,
    .put_super     = cryptext4_put_super,
};
