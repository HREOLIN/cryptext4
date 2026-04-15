/*
 * file.c - Description
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
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <linux/time.h>

static ssize_t cryptext4_read(struct file *file,
                               char __user *buf,
                               size_t len,
                               loff_t *ppos)
{
    return 0; // 最小实现：空读
}

static ssize_t cryptext4_write(struct file *file,
                                const char __user *buf,
                                size_t len,
                                loff_t *ppos)
{
    return len; // 假装写成功
}

// 目录 inode 操作集
const struct inode_operations cryptext4_file_inode_ops = {
    .getattr = simple_getattr,
};

// 目录文件操作集
const struct file_operations cryptext4_file_ops = {
    .owner = THIS_MODULE,
    .read  = cryptext4_read,
    .write = cryptext4_write,
    .llseek = default_llseek,
};