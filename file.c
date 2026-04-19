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
#include <linux/fs.h>
#include "cryptext4.h"

const struct file_operations cryptext4_file_ops = {
    .owner          = THIS_MODULE,
    .llseek         = generic_file_llseek,
    .read_iter      = generic_file_read_iter,
    .write_iter     = generic_file_write_iter,
    .mmap           = generic_file_mmap,
    .fsync          = generic_file_fsync,
    .splice_read    = generic_file_splice_read,
    .open           = generic_file_open,
};

const struct inode_operations cryptext4_file_inode_ops = {
    .getattr        = simple_getattr,
};