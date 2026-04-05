/*
 * cryptext4.c - Stage 1: Simple kernel filesystem skeleton
 *
 * Copyright (C) 2026 Carl.Hu <hcllht@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/pagemap.h>
#include <linux/err.h>
#include <linux/namei.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/log2.h>
#include "disk_format.h"

// static struct inode *cryptext4_alloc_inode(struct super_block *sb)
// {
//     struct inode *inode;

//     inode = new_inode(sb);  // 使用内核默认分配器
//     if (!inode)
//         return NULL;

//     /* 初始化 inode 时间 */
//     inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

//     return inode;
// }

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

/* Stage 1: superblock operations */
static const struct super_operations cryptext4_sops = {
    // .alloc_inode   = cryptext4_alloc_inode,
    .destroy_inode = cryptext4_destroy_inode,
    .drop_inode    = generic_delete_inode,
    .statfs        = cryptext4_statfs,
    .put_super     = cryptext4_put_super,
};

struct cryptext4_sb_info {
    struct super_block *sb;
    struct cryptext4_super_block *raw_sb;   /* raw superblock from disk */
    void *private;                          /* reserved for encryption keys, etc. */
};

/* ========== directory iterates (ls) ============== */
static int cryptext4_iterate(struct file *filp, struct dir_context *ctx)
{
    if(ctx->pos) /* No entries for now, just return EOF */
        return 0;

    dir_emit_dots(filp, ctx); /* Emit . and .. */
    ctx->pos = 2; /* Mark that we've emitted entries */
    return 0;
}

/* ========== lookup (research file) ============== */
static struct dentry *cryptext4_lookup(struct inode *dir, 
        struct dentry *dentry, unsigned int flags)
{
    /* Placeholder for lookup implementation */
    return ERR_PTR(-ENOSYS);
}

/* ========== file operations for directory ============== */
static const struct file_operations cryptext4_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = cryptext4_iterate,
};

/* ========== inode operations for directory ============== */
static const struct inode_operations cryptext4_dir_inode_ops = {
    .lookup = cryptext4_lookup,
};

/* Kill superblock - cleanup */
static void cryptext4_kill_sb(struct super_block *sb)
{
    struct cryptext4_sb_info *sbi = sb->s_fs_info;

    if (sbi) {
        kfree(sbi->raw_sb);
        kfree(sbi);
        sb->s_fs_info = NULL;
    }
    kill_block_super(sb);
    pr_info("cryptext4: superblock killed\n");
}

void print_block_hex(const char *buf, size_t size) 
{
    size_t i;
    for (i = 0; i < size; i++) {
        printk(KERN_CONT "%02x ", (unsigned char)buf[i]);
        if ((i + 1) % 16 == 0)
            printk(KERN_CONT "\n");
    }
    if (i % 16 != 0)
        printk(KERN_CONT "\n");
}

/* Fill superblock (legacy API for 5.15 compatibility) */
static int cryptext4_fill_super(struct super_block *sb, void *data, int silent)
{
    struct cryptext4_sb_info *sbi;
    struct buffer_head *bh;
    struct inode *root;
    char *sb_data = NULL;

    sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;

    sb->s_fs_info = sbi;
    sbi->sb = sb;

    /* Read superblock from disk (usually at block 1, offset 1024) */
    bh = sb_bread(sb, 0);
    if (!bh) {
        pr_err("cryptext4: failed to read superblock\n");
        kfree(sbi);
        return -EIO;
    }

    print_block_hex((char *)bh->b_data + CRYPTEXT4_SB_OFFSET, 128); /* Debug: print raw superblock data */
    sb_data = (char *)bh->b_data + CRYPTEXT4_SB_OFFSET;

    sbi->raw_sb = kmemdup(sb_data, CRYPTEXT4_SB_SIZE, GFP_KERNEL);
    if(sbi->raw_sb != NULL && le32_to_cpu(sbi->raw_sb->s_magic) == CRYPTEXT4_MAGIC) {
        pr_info("cryptext4: valid superblock found on disk\n");
    } else {
        pr_err("cryptext4: invalid superblock (magic mismatch or read error)\n");
        brelse(bh);
        kfree(sbi);
        return -EINVAL;
    }
    brelse(bh);

    /* Setup superblock */
    sb->s_magic = le32_to_cpu(sbi->raw_sb->s_magic);
    sb->s_blocksize = le32_to_cpu(sbi->raw_sb->s_blocksize);
    sb->s_blocksize_bits = ilog2(sb->s_blocksize);
    sb->s_maxbytes = 1LL << 40;          /* 1TB limit for now, adjustable later */
    sb->s_op = &cryptext4_sops;                     /* Will be set in later stages */

    /* ======= create root inode ============= */

    root = new_inode(sb);
    if (!root) {
        pr_err("cryptext4: failed to allocate root inode\n");
        kfree(sbi->raw_sb);
        kfree(sbi);
        return -ENOMEM;
    }   

    root->i_ino = 1; 
    root->i_mode = S_IFDIR | 0755; /* Directory with 755 permissions */
    root->i_atime = root->i_mtime = root->i_ctime = current_time(root);
    root->i_op = &cryptext4_dir_inode_ops;
    root->i_fop = &cryptext4_dir_ops;

    sb->s_root = d_make_root(root);
    if (!sb->s_root) {
        pr_err("cryptext4: failed to create root dentry\n");
        iput(root);
        kfree(sbi->raw_sb);
        kfree(sbi);
        return -ENOMEM;
    }
    pr_info("cryptext4: mounted successfully, blocks=%u, blocksize=%lu\n",
            le32_to_cpu(sbi->raw_sb->s_blocks_count),
            (unsigned long)sb->s_blocksize);

    return 0;
}

/* Mount function (legacy mount_bdev for kernel 5.15) */
static struct dentry *cryptext4_mount(struct file_system_type *fs_type,
                                      int flags, const char *dev_name,
                                      void *data)
{
    return mount_bdev(fs_type, flags, dev_name, data, cryptext4_fill_super);
}

/* Filesystem type registration */
static struct file_system_type cryptext4_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "cryptext4",
    .mount      = cryptext4_mount,       /* 使用旧版 mount API */
    .kill_sb    = cryptext4_kill_sb,
    .fs_flags   = FS_REQUIRES_DEV,
};

/* Module init */
static int __init cryptext4_init(void)
{
    int ret;

    ret = register_filesystem(&cryptext4_fs_type);
    if (ret) {
        pr_err("cryptext4: failed to register filesystem (error %d)\n", ret);
        return ret;
    }

    pr_info("cryptext4: Stage 1 module loaded successfully\n");
    return 0;
}

/* Module exit */
static void __exit cryptext4_exit(void)
{
    unregister_filesystem(&cryptext4_fs_type);
    pr_info("cryptext4: Stage 1 module unloaded\n");
}

module_init(cryptext4_init);
module_exit(cryptext4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carl.Hu + Grok");
MODULE_DESCRIPTION("Cryptext4 - Simple encrypted ext4-like filesystem (Stage 1 skeleton)");
MODULE_VERSION("0.1-stage1");