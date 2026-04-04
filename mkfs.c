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
    * ===== 磁盘布局（v2）=====
    *
    * block 0:
    *   [0 ~ 1023]    保留
    *   [1024 ~ ...]  superblock
    *
    * block 1: inode bitmap（未来）
    * block 2: block bitmap（未来）
    * block 3~6: inode table（预留）
    * block 7+: data blocks
    */
    uint32_t reserved_blocks = 8;

    /* 初始化超级块 */
    sb.s_magic = cpu_to_le32(CRYPTEXT4_MAGIC);
    sb.s_version = cpu_to_le32(CRYPEXT4_VERSION);
    sb.s_blocksize = cpu_to_le32(BLOCK_SIZE);
    sb.s_blocks_count = cpu_to_le32(total_blocks);
    sb.s_inodes_count = cpu_to_le32(1024);   // 简化

    sb.s_free_blocks = sb.s_blocks_count;
    sb.s_free_inodes = sb.s_inodes_count;

    sb.s_encrypt_algo = cpu_to_le32(1); // AES-XTS

    gen_random(sb.s_salt, sizeof(sb.s_salt)); // 生成随机 salt

    /* ========== write block0 =========== */
    unsigned char block[BLOCK_SIZE] = {0};
    memcpy(block + SUPERBLOCK_OFFSET, &sb, sizeof(sb));

    if(lseek(fd, 0, SEEK_SET) != 0) {
        perror("lseek");
        close(fd);
        return 1;
    }

    if(write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("write block0");
        close(fd);
        return 1;
    }

    memset(block, 0, BLOCK_SIZE); // 清空缓冲区
    for (uint32_t i = 1; i < reserved_blocks; i++) {
        if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write metadata block");
            close(fd);
            return 1;
        }
    }

        /* ===== 输出信息 ===== */
    printf("cryptext4 mkfs v2 success:\n");
    printf("  device:        %s\n", argv[1]);
    printf("  total blocks:  %u\n", total_blocks);
    printf("  free blocks:   %u\n", total_blocks - reserved_blocks);
    printf("  block size:    %d\n", BLOCK_SIZE);
    printf("  superblock at: offset %d\n", SUPERBLOCK_OFFSET);

    close(fd);
    return 0;
}