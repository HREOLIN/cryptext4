/*
 * mkfs.c - Description
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
// mkfs.c - 简单 mkfs.cryptext4
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include "disk_format.h"

#define BLOCK_SIZE 4096
#define SUPERBLOCK_OFFSET 1024

static inline uint32_t cpu_to_le32(uint32_t x) { return x; }
static inline uint32_t le32_to_cpu(uint32_t x) { return x; }

static void gen_random(void *buf, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, buf, len) != (ssize_t)len) {
            perror("read urandom");
        }
        close(fd);
    } else {
        /* fallback */
        srand(time(NULL));
        for (size_t i = 0; i < len; i++)
            ((unsigned char *)buf)[i] = rand() % 256;
    }
}

int main(int argc, char *argv[])
{
    int fd;
    struct cryptext4_super_block sb = {0};
    char *device;
    off_t size;
    struct stat st;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device or image file>\n", argv[0]);
        return 1;
    }
    device = argv[1];

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }
    size = st.st_size;

    if(size < (10 * BLOCK_SIZE)) {
        fprintf(stderr, "Device too small (must be at least %d bytes)\n", 10 * BLOCK_SIZE);
        close(fd);
        return 1;
    }

    uint32_t total_blocks = size / BLOCK_SIZE;

    /*
    * ===== 磁盘布局（v3）=====
    *
    * block 0:
    *   [0 ~ 1023]    保留
    *   [1024 ~ ...]  superblock
    *
    * block 1: inode bitmap
    * block 2: block bitmap
    * block 3~6: inode table
    * block 7+: data blocks
    */

    
    /* stage 3 parameters define */
    uint32_t inodes_per_group = CRYPTEXT4_INODES_PER_GROUP;
    uint32_t blocks_per_group = CRYPTEXT4_BLOCKS_PER_GROUP;
    uint32_t inode_bitmap_block = 1;
    uint32_t block_bitmap_block = 2;
    uint32_t inode_table_start = 3;
    uint32_t reserved_blocks = 19; // 0~18 被占用

    /* ==================== init superblock ======================= */
    sb.s_magic              = cpu_to_le32(CRYPTEXT4_MAGIC);
    sb.s_version            = cpu_to_le32(CRYPEXT4_VERSION);
    sb.s_blocksize          = cpu_to_le32(BLOCK_SIZE);
    sb.s_blocks_count       = cpu_to_le32(total_blocks);
    sb.s_inodes_count       = cpu_to_le32(inodes_per_group);
    sb.s_free_blocks        = cpu_to_le32(total_blocks - reserved_blocks);
    sb.s_free_inodes        = cpu_to_le32(inodes_per_group - 1);   // inode 1 为根目录
    sb.s_blocks_count       = cpu_to_le32(total_blocks);

    /* Stage 3 new fields */
    sb.s_inode_per_group    = cpu_to_le32(inodes_per_group);
    sb.s_block_per_group    = cpu_to_le32(blocks_per_group);
    sb.s_inode_bitmap_block = cpu_to_le32(inode_bitmap_block);
    sb.s_block_bitmap_block = cpu_to_le32(block_bitmap_block);
    sb.s_inode_table_start  = cpu_to_le32(inode_table_start);

    // /* 初始化超级块 */
    // sb.s_magic = cpu_to_le32(CRYPTEXT4_MAGIC);
    // sb.s_version = cpu_to_le32(CRYPEXT4_VERSION);
    // sb.s_blocksize = cpu_to_le32(BLOCK_SIZE);
    // sb.s_blocks_count = cpu_to_le32(total_blocks);
    // sb.s_inodes_count = cpu_to_le32(1024);   // 简化

    // sb.s_free_blocks = sb.s_blocks_count;
    // sb.s_free_inodes = sb.s_inodes_count;

    sb.s_encrypt_algo = cpu_to_le32(1); // AES-XTS
    gen_random(sb.s_salt, sizeof(sb.s_salt)); // 生成随机 salt

    /* ================== write superblock ================== */
    unsigned char block[BLOCK_SIZE] = {0};
    memcpy(block + SUPERBLOCK_OFFSET, &sb, sizeof(sb));

    lseek(fd, 0, SEEK_SET);
    if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write superblock");
        close(fd);
        return 1;
    }

    /* ==================== 初始化 Inode Bitmap ==================== */
    memset(block, 0, BLOCK_SIZE);
    block[0] = 0x03;                    // inode 0 和 inode 1 被占用

    lseek(fd, inode_bitmap_block * BLOCK_SIZE, SEEK_SET);
    if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write inode bitmap");
        close(fd);
        return 1;
    }

    /* ==================== 初始化 Block Bitmap ==================== */
    memset(block, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < reserved_blocks; i++) {
        int byte_idx = i / 8;
        int bit_idx  = i % 8;
        block[byte_idx] |= (1u << bit_idx);
    }

    lseek(fd, block_bitmap_block * BLOCK_SIZE, SEEK_SET);
    if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write block bitmap");
        close(fd);
        return 1;
    }

    /* ==================== 清零 Inode Table ==================== */
    memset(block, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < 16; i++) {        // 16 blocks = 1024 inodes
        if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write inode table");
            close(fd);
            return 1;
        }
    }

    /* ==================== 清零剩余保留块 ==================== */
    memset(block, 0, BLOCK_SIZE);
    for (uint32_t i = reserved_blocks; i < 32; i++) {   // 多清一点保险
        if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write reserved blocks");
            close(fd);
            return 1;
        }
    }

    printf("cryptext4 mkfs v3 (Stage 3) success:\n");
    printf("  device:             %s\n", device);
    printf("  total blocks:       %u\n", total_blocks);
    printf("  inodes per group:   %u\n", inodes_per_group);
    printf("  inode bitmap:       block %u\n", inode_bitmap_block);
    printf("  block bitmap:       block %u\n", block_bitmap_block);
    printf("  inode table start:  block %u\n", inode_table_start);
    printf("  reserved blocks:    %u\n", reserved_blocks);

    close(fd);
    return 0;
}