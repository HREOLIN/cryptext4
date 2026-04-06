# Cryptext4

> A minimal ext4-like encrypted filesystem (Linux kernel module)

---

## 📖 Overview

Cryptext4 是一个用于学习 Linux 文件系统内部原理的自定义内核文件系统模块。

目前我们已进入 **Stage 3**：  
实现**磁盘级持久化结构**，包括 Inode Bitmap、Block Bitmap、Inode Table 和基本的 Block Group 布局。

目标是让 `touch`、`mkdir`、`ls` 等操作真正写入磁盘，而不再仅存在于内存中。

---

## ✨ Current Features (Stage 3)

* [x] Custom superblock format
* [x] Mountable filesystem
* [x] Basic VFS integration (`lookup`, `iterate`)
* [x] In-memory file creation (`touch`)
* [x] **Stage 3 Progress**:
  - Inode Bitmap
  - Block Bitmap
  - Inode Table
  - Basic inode allocation from bitmap
* [ ] Persistent file data (write to data blocks)
* [ ] Extent-based storage
* [ ] Journaling
* [ ] AES-XTS encryption

---

## 🧠 Architecture
~~~
User Space
↓
VFS Layer
↓
Cryptext4 Kernel Module
├── Superblock
├── Inode Bitmap
├── Block Bitmap
├── Inode Table
├── Block Allocator
└── (Future) Encryption Layer
↓
Block Device (/dev/loopX or image file)
~~~

---
## 💾 Disk Layout (Stage 3 - v3)
~~~
Block 0:
[0 ~ 1023]     Reserved (compatibility)
[1024 ~ ...]   Superblock
Block 1: Inode Bitmap
Block 2: Block Bitmap
Block 3~18: Inode Table (≈1024 inodes)
Block 19+: Data Blocks (file contents + directory entries)
~~~

--- 

## 🚀 Quick Start (Stage 3)

### 1. Build mkfs

~~~
gcc -o mkfs.cryptext4 mkfs.c
~~~

--- 
### 2. Create filesystem image

~~~
dd if=/dev/zero of=fs.img bs=1M count=128
~~~

### 3. Format with Stage 3 layout

~~~
sudo ./mkfs.cryptext4 fs.img
~~~

### 4. Build kernel module

~~~
make
~~~

### 5. Load and mount

~~~
sudo insmod cryptext4.ko

mkdir -p /mnt/cryptext4
sudo mount -t cryptext4 -o loop fs.img /mnt/cryptext4
~~~

---

### 6. Test

~~~
cd /mnt/cryptext4
touch test.txt
echo "Hello Stage 3" > hello.txt
mkdir dir1
ls -la
~~~

## 📂 Project Structure

~~~
.
├── cryptext4.c          # Kernel module (Stage 3)
├── disk_format.h        # On-disk data structures
├── mkfs.c               # Filesystem formatter (must be updated for Stage 3)
├── Makefile
└── README.md
~~~

---

## 🛣️ Roadmap

### Stage 1
- Superblock + basic mount

### Stage 2
- Root inode + VFS integration (ls, in-memory touch)

### Stage 3 (Current)

- Inode Bitmap
- Block Bitmap
- Inode Table
- Basic inode allocation
- On-disk metadata foundation

### Stage 4 (Next)

- Block allocation + data block write
- Directory entry persistence
- Simple extent support

### Stage 5
- Journaling (basic)

### Stage 6
- AES-XTS encryption

---

## 🔧 Stage 3: mkfs 修改要点
当前 **mkfs.c** 仅写入 Superblock，需要升级以支持 Stage 3 结构：</br>
**必须修改的内容：**

~~~
1. 扩展 struct cryptext4_super_block（在 disk_format.h 中增加）：
   .s_inodes_per_group
   .s_blocks_per_group
   .s_inode_bitmap_block
   .s_block_bitmap_block
   .s_inode_table_start

2.格式化时初始化：
   .Inode Bitmap（保留 inode 0，标记 inode 1 为已用）
   .Block Bitmap（标记前 19 个 block 为已用）
   .Inode Table（写入根目录 inode）

3.更新 free_inodes / free_blocks 计数

~~~

---

## 🔐 Future Work
- Persistent file content (data blocks)
- Extent-based allocation
- Journaling
- Per-file AES-XTS encryption

## ⚠️ Disclaimer
This project is for **educational and research purposes only.**</br>
Not intended for production use.

## 👨‍💻 Author
Carl Hu and Grok