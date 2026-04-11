/*
 * cryptext4.c - Stage 2: Simple kernel filesystem skeleton
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
#include <linux/splice.h>
#include <linux/atomic.h>
#include "disk_format.h"

/* ====================== Stage 2: 内存文件模拟结构 ====================== */
struct cryptext4_file_data {
    struct list_head list;
    char name[256];
    uint32_t ino;
    char *data;          // 文件内容（内存分配）
    size_t size;         // 当前文件大小
    umode_t mode;
};

static LIST_HEAD(cryptext4_files);   // 全局内存文件链表（根目录下所有文件）


/* global variables */
static atomic_t cryptext4_inode_count = ATOMIC_INIT(2);

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


/* ====================== 前向声明 ====================== */
static int cryptext4_create(struct user_namespace *mnt_userns,
                            struct inode *dir, struct dentry *dentry,
                            umode_t mode, bool excl);

static int cryptext4_mkdir(struct user_namespace *mnt_userns,
                           struct inode *dir, struct dentry *dentry,
                           umode_t mode);

static void print_block_hex(const char *buf, size_t size);

/* ====================== Directory Inode & File Operations (提前声明) ====================== */
static const struct inode_operations cryptext4_dir_inode_ops;
static const struct file_operations cryptext4_dir_ops;



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
    // kill_block_super(sb);  // 默认释放块设备 superblock
}

/* Stage 2: superblock operations */
static const struct super_operations cryptext4_sops = {
    // .alloc_inode   = cryptext4_alloc_inode,
    .destroy_inode = cryptext4_destroy_inode,
    .drop_inode    = generic_delete_inode,
    .statfs        = cryptext4_statfs,
    .put_super     = cryptext4_put_super,
};

/* ========== directory iterates (ls) ============== */
static int cryptext4_iterate(struct file *filp, struct dir_context *ctx)
{
    pr_info("cryptext4: iterate called, pos = %lld\n", ctx->pos);

    if (ctx->pos == 0) {
        dir_emit_dots(filp, ctx);
        ctx->pos = 2;
        return 0;
    }

    /* Stage 2：目前我们不做复杂目录项管理，只显示 . 和 .. */
    /* 后面 Stage 3 会实现真正的目录项遍历 */

    return 0;
}

/* ========== lookup (research file) ============== */
static struct dentry *cryptext4_lookup(struct inode *dir, 
        struct dentry *dentry, unsigned int flags)
{
    /* Placeholder for lookup implementation */
    pr_info("cryptext4: lookup called for name '%s'\n", dentry->d_name.name);
    if(dentry->d_name.name && dentry->d_name.len > 0) {
        pr_info("cryptext4: lookup - name '%s' (len=%u)\n", dentry->d_name.name, dentry->d_name.len);
    } else {
        pr_info("cryptext4: lookup - empty name\n");
    }

    return NULL; /* Not found */
}

/* ====================== Create File (Stage 3 - Inode Bitmap) ====================== */
static int cryptext4_create(struct user_namespace *mnt_userns,
                            struct inode *dir, struct dentry *dentry,
                            umode_t mode, bool excl)
{
    struct inode *inode;
    uint32_t ino = atomic_inc_return(&cryptext4_inode_count);
    // struct cryptext4_file_data *file_data;
    struct cryptext4_sb_info *sbi = dir->i_sb->s_fs_info;
    int bit;

    pr_info("cryptext4: [Stage 3] create file '%s' - starting inode allocation\n",  dentry->d_name.name);
    pr_info("cryptext4: create file '%s' (ino=%u)\n", dentry->d_name.name, ino);

    if(!sbi || !sbi->inode_bitmap) {
        pr_err("cryptext4: create file '%s' - error: superblock info or inode bitmap not initialized\n", dentry->d_name.name);
        return -EIO;
    }

    /* 1. inode bitmap 中分配一个空闲 inode 号 */
    bit = find_first_zero_bit(sbi->inode_bitmap, CRYPTEXT4_INODES_PER_GROUP);
    if(bit >= CRYPTEXT4_INODES_PER_GROUP) {
        pr_err("cryptext4: create file '%s' - error: no free inodes available\n", dentry->d_name.name);
        return -ENOSPC;
    }

    ino = bit + 1; /* Inode numbers start from 1 */
    set_bit(bit, sbi->inode_bitmap); /* Mark inode as allocated */

    /* s. update superblock 中的 free_inodes counts */
    sbi->raw_sb->s_free_inodes = cpu_to_le32(le32_to_cpu(sbi->raw_sb->s_free_inodes) - 1);
    pr_info("cryptext4: create file '%s' - allocated inode %u (bit %d)\n", dentry->d_name.name, ino, bit);

    /* 4. create vfs inode */
    inode = new_inode(dir->i_sb);
    if (!inode) {
        clear_bit(bit, sbi->inode_bitmap); /* Rollback inode allocation */
        pr_err("cryptext4: create file '%s' - error: failed to allocate VFS inode\n", dentry->d_name.name);
        return -ENOMEM;
    }

    inode->i_ino = ino;
    inode->i_mode = mode | S_IFREG;
    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_size = 0;

    /* 使用内核简单文件操作（Stage 3 暂时） */
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;

    d_instantiate(dentry, inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);

    pr_info("cryptext4: file '%s' created successfully (ino=%u)\n", 
            dentry->d_name.name, ino);

    return 0;

    // /* 在内存中创建文件数据结构 */
    // file_data = kzalloc(sizeof(*file_data), GFP_KERNEL);
    // if (!file_data)
    //     return -ENOMEM;

    // strncpy(file_data->name, dentry->d_name.name, sizeof(file_data->name)-1);
    // file_data->ino = ino;
    // file_data->mode = mode | S_IFREG;
    // file_data->data = NULL;
    // file_data->size = 0;

    // list_add_tail(&file_data->list, &cryptext4_files);

    // /* 创建 VFS inode */
    // inode = new_inode(dir->i_sb);
    // if (!inode) {
    //     kfree(file_data);
    //     return -ENOMEM;
    // }

    // inode->i_ino = ino;
    // inode->i_mode = file_data->mode;
    // inode->i_uid = current_fsuid();
    // inode->i_gid = current_fsgid();
    // inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    // inode->i_size = 0;

    // /* ==================== 重要修改部分 ==================== */
    // /* 5.15 内核兼容方案：先使用 simple_dir_inode_operations */
    // inode->i_op = &simple_dir_inode_operations;
    // inode->i_fop = &simple_dir_operations;
    // /* ==================================================== */

    // d_instantiate(dentry, inode);
    // mark_inode_dirty(inode);
    // mark_inode_dirty(dir);

    // pr_info("cryptext4: file '%s' created in memory\n", dentry->d_name.name);
    // return 0;
}

/* ====================== Create Directory ====================== */
static int cryptext4_mkdir(struct user_namespace *mnt_userns,
                           struct inode *dir, struct dentry *dentry,
                           umode_t mode)
{
    struct inode *inode;
    uint32_t ino = atomic_inc_return(&cryptext4_inode_count);

    pr_info("cryptext4: mkdir '%s' (ino=%u)\n", dentry->d_name.name, ino);

    inode = new_inode(dir->i_sb);
    if (!inode)
        return -ENOMEM;

    inode->i_ino = ino;
    inode->i_mode = mode | S_IFDIR;
    inode->i_uid.val = current_fsuid().val;
    inode->i_gid.val = current_fsgid().val;
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    inode->i_op = &cryptext4_dir_inode_ops;
    inode->i_fop = &cryptext4_dir_ops;

    d_instantiate(dentry, inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);

    return 0;
}

/* ====================== 现在定义目录操作集 ====================== */
static const struct inode_operations cryptext4_dir_inode_ops = {
    .lookup = cryptext4_lookup,
    .create = cryptext4_create,
    .mkdir  = cryptext4_mkdir,
};

// static const struct file_operations cryptext4_file_ops = {
//     .owner = THIS_MODULE,
//     .read  = cryptext4_file_read,
//     .write = cryptext4_file_write,
//     .llseek = default_llseek,
// };

static const struct file_operations cryptext4_dir_ops = {
    .owner          = THIS_MODULE,
    .iterate_shared = cryptext4_iterate,
};

/* Kill superblock - cleanup */
static void cryptext4_kill_sb(struct super_block *sb)
{
    struct cryptext4_sb_info *sbi = sb->s_fs_info;
    pr_info("cryptext4: killing superblock\n");

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

    pr_info("cryptext4: Stage 3 fill_super started\n");

    // 这里有问题，这里导致了读取失败，全部都是0，后续的 magic 也不对了
    // if(!sb_set_blocksize(sb, BLOCK_SIZE)) {
    //     pr_err("cryptext4: unsupported block size %d\n", BLOCK_SIZE);
    //     return -EINVAL;
    // }

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

    pr_info("cryptext4: === Block 0 full dump (0~4095) ===\n");
    print_block_hex((char *)bh->b_data, 4096);     // 先看整个 block 0（可选）

    pr_info("cryptext4: === Superblock at offset 1024 (should have data) ===\n");
    print_block_hex((char *)bh->b_data + 1024, 128);   // 只打印 superblock 部分
    
    sb_data = (char *)bh->b_data + 1024;

    sbi->raw_sb = kmemdup(sb_data, CRYPTEXT4_SB_SIZE, GFP_KERNEL);
    if(sbi->raw_sb != NULL && le32_to_cpu(sbi->raw_sb->s_magic) == CRYPTEXT4_MAGIC) {
        pr_info("cryptext4: valid superblock found on disk\n");
    } else {
        pr_err("cryptext4: invalid superblock (magic mismatch or read error)\n");
        brelse(bh);
        kfree(sbi);
        return -EINVAL;
    }

    if(!sb_set_blocksize(sb, __le32_to_cpu(sbi->raw_sb->s_blocksize))) {
        pr_err("cryptext4: unsupported block size %d\n", __le32_to_cpu(sbi->raw_sb->s_blocksize));
        return -EINVAL;
    }
    brelse(bh);
    bh = NULL;

    pr_info("cryptext4: superblock read successfully: blocks=%u, inodes=%u, blocksize=%u\n",
            le32_to_cpu(sbi->raw_sb->s_blocks_count),
            le32_to_cpu(sbi->raw_sb->s_inodes_count),
            le32_to_cpu(sbi->raw_sb->s_blocksize));
            
    /* Stage 3: 初始化 inode 和 block bitmap */
    sbi->inode_table_start_block = le32_to_cpu(sbi->raw_sb->s_inode_table_start);
    sbi->inode_bitmap = kzalloc(BITS_TO_LONGS(CRYPTEXT4_INODES_PER_GROUP) * sizeof(long), GFP_KERNEL);
    sbi->block_bitmap = kzalloc(BITS_TO_LONGS(CRYPTEXT4_BLOCKS_PER_GROUP) * sizeof(long), GFP_KERNEL);
    
    if(!sbi->inode_bitmap || !sbi->block_bitmap) {
        pr_err("cryptext4: failed to allocate memory for bitmaps\n");
        kfree(sbi->inode_bitmap);
        kfree(sbi->block_bitmap);
        kfree(sbi->raw_sb);
        kfree(sbi);
        return -ENOMEM;
    }

    pr_info("s_inode_bitmap_block = %u, s_block_bitmap_block = %u\n",
            le32_to_cpu(sbi->raw_sb->s_inode_bitmap_block),
            le32_to_cpu(sbi->raw_sb->s_block_bitmap_block));
    
    /* read inode bitmap to memory */
    bh = sb_bread(sb, le32_to_cpu(sbi->raw_sb->s_inode_bitmap_block));
    if (bh) {
        memcpy(sbi->inode_bitmap, bh->b_data, BLOCK_SIZE);
        brelse(bh);
    } else {
        pr_err("cryptext4: failed to read inode bitmap\n");
        kfree(sbi->inode_bitmap);
        kfree(sbi->block_bitmap);
        kfree(sbi->raw_sb);
        kfree(sbi);
        return -EIO;
    }
    
    /* read block bitmap to memory */
    bh = sb_bread(sb, le32_to_cpu(sbi->raw_sb->s_block_bitmap_block));
    if (bh) {
        memcpy(sbi->block_bitmap, bh->b_data, BLOCK_SIZE);
        brelse(bh);
    } else {
        pr_err("cryptext4: failed to read block bitmap\n");
        kfree(sbi->inode_bitmap);
        kfree(sbi->block_bitmap);
        kfree(sbi->raw_sb);
        kfree(sbi);
        return -EIO;
    }

    /* Setup superblock */
    sb->s_magic = le32_to_cpu(sbi->raw_sb->s_magic);
    sb->s_blocksize = le32_to_cpu(sbi->raw_sb->s_blocksize);
    sb->s_blocksize_bits = ilog2(sb->s_blocksize);
    sb->s_maxbytes = 1LL << 40;                     /* 1TB limit for now, adjustable later */
    sb->s_op = &cryptext4_sops;                     /* Will be set in later stages */
    sb->s_flags |= SB_NOATIME;                      /* Disable atime updates for simplicity */

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

    pr_info("cryptext4: stage 2 module loaded successfully\n");
    return 0;
}

/* Module exit */
static void __exit cryptext4_exit(void)
{
    unregister_filesystem(&cryptext4_fs_type);
    pr_info("cryptext4: stage 2 module unloaded\n");
}

module_init(cryptext4_init);
module_exit(cryptext4_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carl.Hu + Grok");
MODULE_DESCRIPTION("Cryptext4 - Simple encrypted ext4-like filesystem (stage 2 skeleton)");
MODULE_VERSION("0.1-stage1");