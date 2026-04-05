# Cryptext4

> A minimal ext4-like encrypted filesystem (Linux kernel module)

---

## 📖 Overview

Cryptext4 is a custom Linux filesystem designed to explore how modern filesystems (like ext4) work internally, with a focus on:

* Block management
* Inode design
* Extent-based storage
* Journaling
* Built-in encryption (AES-XTS, planned)

This project is implemented as a Linux kernel module together with a custom `mkfs` tool.

---

## ✨ Features

* [x] Custom superblock format
* [x] Mountable filesystem skeleton
* [x] Basic VFS integration (`ls` support)
* [ ] On-disk inode table
* [ ] Block & inode bitmap
* [ ] File creation (`touch`)
* [ ] Extent-based storage
* [ ] Delayed allocation
* [ ] Journaling (JBD-like)
* [ ] AES-XTS encryption

---

## 🧠 Architecture

```
User Space
   ↓
   VFS
   ↓
Cryptext4 (Kernel Module)
   ├── Superblock
   ├── Inode Layer
   ├── Block Allocator
   └── (Future) Encryption Layer
   ↓
Block Device
```

---

## 💾 Disk Layout (v2)

```
Block 0:
  [0 ~ 1023]    Reserved
  [1024 ~ ...]  Superblock

Block 1: inode bitmap (reserved)
Block 2: block bitmap (reserved)
Block 3~6: inode table (reserved)
Block 7+: data blocks
```

---

## 🚀 Quick Start

### 1. Build mkfs

```bash
gcc -o mkfs.cryptext4 mkfs.c
```

### 2. Create test image

```bash
dd if=/dev/zero of=fs.img bs=1M count=64
```

### 3. Format filesystem

```bash
sudo ./mkfs.cryptext4 fs.img
```

### 4. Build kernel module

```bash
make
```

### 5. Load module

```bash
sudo insmod cryptext4.ko
```

### 6. Mount filesystem

```bash
mkdir -p /mnt/cryptext4
sudo mount -t cryptext4 -o loop fs.img /mnt/cryptext4
```

### 7. Test

```bash
ls -la /mnt/cryptext4
```

---

## 📂 Project Structure

```
.
├── cryptext4.c       # Kernel filesystem implementation
├── disk_format.h     # On-disk data structures
├── mkfs.c            # Filesystem formatter
├── Makefile
```

---

## 🛣️ Roadmap

### stage 2

* Superblock
* Mount support

### Stage 2

* Root inode
* Basic directory (`ls`)

### Stage 3

* Inode table
* File creation

### Stage 4

* Extents

### Stage 5

* Journaling

### Stage 6

* Encryption (AES-XTS)

---

## 🔐 Future Work

* Per-file encryption keys
* Transparent encryption/decryption
* Performance optimization (mballoc-like allocator)
* Better VFS integration

---

## 🎯 Motivation

This project is built to deeply understand Linux filesystem internals, including:

* VFS layer
* Block device interaction
* Filesystem layout design
* Encryption integration

---

## ⚠️ Disclaimer

This project is for educational and research purposes only.
**Not production-ready.**

---

## 👨‍💻 Author

Carl Hu
