# Unix FileSystem 系统架构文档

> 同济大学操作系统课程设计 — 用户级 EXT2 文件系统模拟器  
> 基于《要求.pdf》规范实现，采用 C++11 编写

---

## 目录

1. [整体架构](#1-整体架构)
2. [磁盘布局](#2-磁盘布局)
3. [核心数据结构](#3-核心数据结构)
4. [缓存系统](#4-缓存系统)
5. [Inode 与混合索引表](#5-inode-与混合索引表)
6. [磁盘读写与数据流](#6-磁盘读写与数据流)
7. [命令系统](#7-命令系统)
8. [用户与权限子系统](#8-用户与权限子系统)
9. [PDF 要求对照](#9-pdf-要求对照)

---

## 1. 整体架构

### 1.1 分层设计

```
┌───────────────────────────────────────────────┐
│  Shell 层  (Shell/Shell.cpp)                   │
│  命令解析 · 用户交互 · 25 个命令的 dispatch     │
├───────────────────────────────────────────────┤
│  VFS 层  (VFS/VFS.cpp)                        │
│  虚拟文件系统 · open/close/read/write/dir/ls   │
│  权限检查 (owner/group/other rwx)              │
├───────────────────────────────────────────────┤
│  Cache 层                                      │
│  InodeCache · SuperBlockCache · BufferCache    │
│  DirectoryCache · BlockGroupDescCache          │
├───────────────────────────────────────────────┤
│  Ext2 层  (Ext2/Ext2.cpp)                      │
│  format/mount · balloc/bfree · ialloc/ifree    │
│  locateInode · Bmap · DiskInode 读写           │
├───────────────────────────────────────────────┤
│  DiskDriver 层  (DiskDriver/DiskDriver.cpp)    │
│  mmap 映射 disk.img · 直接内存读写              │
└───────────────────────────────────────────────┘
```

### 1.2 源文件组织

```
Unix-FileSystem/
├── main.cpp                          # 程序入口，启动 Shell 循环
├── Makefile                          # 构建脚本（每个 .cpp 独立编译）
├── include/                          # 头文件（30 个 .h）
│   ├── define.h                      # 系统常量 — DISK_BLOCK_SIZE, MAX_INODE_NUM 等
│   ├── Inode.h                       # 内存 Inode — 权限位/类型/索引指针常量
│   ├── DiskInode.h                   # 磁盘 Inode — d_addr[15], d_ctime
│   ├── User.h                        # 用户结构 — username, u_uid, isLoggedIn
│   ├── Shell.h                       # Shell — 25 个命令方法声明
│   ├── VFS.h                         # VFS 接口 — open/close/read/write/dir/ls
│   ├── Ext2.h                        # EXT2 逻辑 — format/locateInode/getDiskInodeByNum
│   ├── SuperBlock.h                  # 超级块 — bitmap + inode 栈
│   ├── BlockGroupDesc.h              # 组描述符 (新增)
│   ├── BufferCache.h                 # 缓冲缓存 — Bread/Bdwrite/GetBlk
│   ├── InodeCache.h                  # Inode 缓存 — getInodeByID/addInodeCache
│   ├── SuperBlockCache.h             # 超级块缓存
│   ├── BlockGroupDescCache.h         # 组描述符缓存 (新增)
│   ├── DiskDriver.h                  # 磁盘驱动 — mmap 接口
│   └── ...                           # Path, Bitmap, Kernel, VirtualProcess 等
├── Ext2/                             # EXT2 层实现
│   ├── Ext2.cpp                      # format, registerFs, locateInode, updateDiskInode...
│   ├── SuperBlock.cpp                # balloc, bfree, ialloc, ifree
│   ├── InodePool.cpp                 # Inode 池 — ialloc, ifree, iupdate
│   ├── DiskInode.cpp                 # 3 个构造函数
│   └── Path.cpp                      # 路径解析
├── VFS/                              # VFS + Cache 实现
│   ├── VFS.cpp                       # createFile, deleteFile, open, read, write, dir, ls...
│   ├── Inode.cpp                     # Bmap() — 混合索引映射
│   ├── File.cpp                      # File/OpenFiles/IOParameter
│   ├── InodeCache.cpp                # Inode 缓存管理
│   ├── SuperBlockCache.cpp           # 超级块缓存 + flushBack
│   ├── BlockGroupDescCache.cpp       # GDT 缓存 (新增)
│   └── ...
├── Shell/
│   └── Shell.cpp                     # 全部 25 个命令的实现
├── BufferCache/                      # 缓冲缓存
│   ├── BufferCache.cpp               # Bread, Bwrite, Bdwrite, GetBlk, Brelse
│   └── ...
├── DiskDriver/
│   └── DiskDriver.cpp                # mmap/munmap, getBlk, readBlk, writeBlk
├── VirtualProcess/
│   ├── Kernel.cpp                    # 内核单例
│   ├── User.cpp                      # User 构造函数 (guest uid=-1)
│   └── VirtualProcess.cpp            # 虚拟进程
└── Utils/
    ├── Bitmap.cpp                    # 位图工具
    ├── Logcat.cpp                    # 日志
    └── TimeHelper.cpp                # 时间戳
```

---

## 2. 磁盘布局

### 2.1 基本参数

定义于 [include/define.h](include/define.h)：

```cpp
#define DISK_BLOCK_SIZE 4096                         // 每块 4096 字节
#define DISK_SIZE (64 * 1024 * 1024)                 // 总容量 64 MiB
#define DISK_BLOCK_NUM (DISK_SIZE / DISK_BLOCK_SIZE) // 共 16384 块
#define DISK_IMG_FILEPATH "./disk.img"               // 模拟文件
#define DISKINODE_SIZE 88                            // 磁盘 Inode 大小 (含 ctime + d_addr[15])
#define MAX_INODE_NUM (2 * DISK_BLOCK_SIZE / DISKINODE_SIZE) // 93 个 Inode
#define BUFFER_CACHE_NUM 20                          // 缓冲区数量
#define INODE_CACHE_SIZE 128                         // Inode 缓存容量
```

### 2.2 磁盘布局图

由 [Ext2/Ext2.cpp:12-167](Ext2/Ext2.cpp#L12-L167) 的 `format()` 函数创建：

```
Block   0  ┌──────────────────────┐
           │  SuperBlock          │ ← bitmap + inode 栈 + 手动 padding
Block   1  ├──────────────────────┤
           │  BlockGroupDesc      │ ← 组描述符表 (新增)
Block 2~4  ├──────────────────────┤
           │  InodePool           │ ← Inode Bitmap + 93 个 DiskInode
Block   5  ├──────────────────────┤
           │  Root Directory (/)  │ ← . .. bin etc dev home
Block   6  ├──────────────────────┤
           │  /bin Directory      │ ← . ..
Block   7  ├──────────────────────┤
           │  /etc Directory      │ ← . ..
Block   8  ├──────────────────────┤
           │  /home Directory     │ ← . ..
Block   9  ├──────────────────────┤
           │  /dev Directory      │ ← . ..
Block 10+  ├──────────────────────┤
           │  Free Data Blocks    │ ← 用户文件数据
           └──────────────────────┘
```

format() 核心流程（[Ext2/Ext2.cpp:12-95](Ext2/Ext2.cpp#L12-L95)）：

```cpp
void Ext2::format()
{
    p_bufferCache->initialize();
    DiskBlock *diskMemAddr = Kernel::instance()->getDiskDriver().getDiskMemAddr();
    memset(diskMemAddr, 0, DISK_SIZE);               // 清零整个磁盘

    // ① 构造 SuperBlock
    SuperBlock tempSuperBlock;
    tempSuperBlock.bsetOccupy(0);  // superblock
    tempSuperBlock.bsetOccupy(1);  // GDT (新增)
    tempSuperBlock.bsetOccupy(2);  // InodePool
    tempSuperBlock.bsetOccupy(3);  // InodePool
    tempSuperBlock.bsetOccupy(4);  // InodePool
    tempSuperBlock.bsetOccupy(5);  // root dir
    tempSuperBlock.bsetOccupy(6);  // bin
    tempSuperBlock.bsetOccupy(7);  // etc
    tempSuperBlock.bsetOccupy(8);  // home
    tempSuperBlock.bsetOccupy(9);  // dev
    memcpy(diskMemAddr, &tempSuperBlock, sizeof(SuperBlock));  // → block 0

    // ①-2 写入 GDT (BlockGroupDesc) 到 block 1
    BlockGroupDesc tempGDT;
    tempGDT.bg_block_bitmap = 0;
    tempGDT.bg_inode_bitmap = 2;
    tempGDT.bg_inode_table = 3;
    tempGDT.bg_free_blocks_count = DISK_BLOCK_NUM - 10;
    tempGDT.bg_free_inodes_count = (MAX_INODE_NUM - 1) - 5;
    tempGDT.bg_used_dirs_count = 5;
    memcpy(diskMemAddr + 1, &tempGDT, DISK_BLOCK_SIZE); // → block 1

    // ② 创建 5 个目录 Inode (root=1, bin=2, etc=3, dev=4, home=5)
    InodePool tempInodePool;
    int tempAddr[15] = {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    DiskInode tempDiskInode = DiskInode(Inode::IFDIR | Inode::DEFAULT_DIR_MODE, ...);
    // 写入 root(1), 然后修改 d_addr[0] 写 bin(2), etc(3), dev(4), home(5)
    memcpy(diskMemAddr + 2, &tempInodePool, 3 * DISK_BLOCK_SIZE); // → block 2,3,4

    // ③ 写入目录项数据到 block 5~9 (root 含 . .. bin etc dev home)
    DirectoryEntry *p_directoryEntry = (DirectoryEntry *)diskMemAddr;
    // ... 逐个填充各目录的 . 和 .. 条目 ...
}
```

---

## 3. 核心数据结构

### 3.1 SuperBlock（超级块）

**文件**: [include/SuperBlock.h:10-36](include/SuperBlock.h#L10-L36)

```cpp
class SuperBlock
{
public:
    SuperBlock();
    size_t SuperBlockBlockNum = 1;      // superblock 占 1 个块
    int free_inode_num;                 // 空闲 inode 数（同时是栈指针）
    int free_block_bum;                 // 空闲块数
    int total_block_num;                // 总块数 (=16384)
    int total_inode_num;                // 总 inode 数 (=92)
    InodeId s_inode[MAX_INODE_NUM-1];   // 空闲 inode 栈 (LIFO)
    Bitmap disk_block_bitmap;           // 块位图
    char padding[DISK_BLOCK_SIZE - 2448]; // 手工对齐到 4096 字节

    BlkNum balloc();           // 从位图分配空闲块
    void bfree(BlkNum blkNum); // 释放块回位图
    void bsetOccupy(BlkNum);   // 标记块为已占用
    InodeId ialloc();          // 从栈弹出空闲 inode 号
    void ifree(InodeId);       // 将 inode 号压回栈
};
```

**块位图分配算法** ([Ext2/SuperBlock.cpp](Ext2/SuperBlock.cpp))：不同于经典 Unix 的空闲块链表，本项目使用 `Bitmap` 类管理空闲块，每个 bit 对应一个磁盘块。空闲 inode 使用 **LIFO 栈** 分配，初始化时 `s_inode[]` 按降序填入所有可用 inode 号。

### 3.2 BlockGroupDesc（组描述符表）

**文件**: [include/BlockGroupDesc.h:11-22](include/BlockGroupDesc.h#L11-L22)

```cpp
struct BlockGroupDesc
{
    int bg_block_bitmap;       // 块位图所在块号 (=0, 在 SuperBlock 内)
    int bg_inode_bitmap;       // Inode 位图所在块号 (=2, 在 InodePool 内)
    int bg_inode_table;        // Inode 表起始块号 (=3)
    int bg_free_blocks_count;  // 组内空闲块计数
    int bg_free_inodes_count;  // 组内空闲 inode 计数
    int bg_used_dirs_count;    // 组内已用目录数

    char padding[4096 - 24];   // 手工填充至 4096 字节 (24=6 ints × 4 bytes)
};
```

本项目是**单块组** EXT2 文件系统。GDT 占据一个完整的 4096 字节磁盘块（block 1）。balloc/bfree/ialloc/ifree 操作会同步更新 GDT 缓存中的对应计数器。

### 3.3 DiskInode（磁盘 Inode）

**文件**: [include/DiskInode.h:11-34](include/DiskInode.h#L11-L34)，大小 = 88 字节

```cpp
class DiskInode
{
public:
    unsigned int d_mode;    // 文件类型 + rwx 权限位
    int d_nlink;            // 硬链接计数
    short d_uid;            // 文件所有者 ID
    short d_gid;            // 文件所属组 ID
    int d_size;             // 文件大小 (字节)
    int d_addr[15];         // 混合索引表:
                            //   [0..11]=12 直接, [12]=1 单间接,
                            //   [13]=1 双间接, [14]=1 三间接 (新增)
    int d_atime;            // 最近访问时间
    int d_mtime;            // 最近修改时间
    int d_ctime;            // 创建/变更时间 (新增)
};
```

### 3.4 Inode（内存 Inode）

**文件**: [include/Inode.h:16-103](include/Inode.h#L16-L103)

在 DiskInode 基础上增加运行时字段：
```cpp
class Inode {
    unsigned int i_mode;
    int i_nlink; short i_uid; short i_gid; int i_size;
    int i_addr[15];                // 混合索引表 (12+1+1+1)
    unsigned int i_flag;           // ILOCK|IUPD|IACC|IMOUNT|IWANT|ITEXT
    int i_count;                   // 引用计数
    short i_dev; int i_number;     // 设备号 + inode 号
    int i_lastr;                   // 上次读取的逻辑块号 (预读)
    int i_ctime;                   // 创建时间 (新增)
};
```

### 3.5 i_mode 位布局 + 权限常量

**文件**: [include/Inode.h:50-98](include/Inode.h#L50-L98)

```
Bit:   15       14       13       12        8 7      0
      ┌────────┬────────┬────────┬─────────┬──────────┐
      │ IALLOC │ IFDIR  │ IFCHR  │ (free)  │ rwxrwxrwx│
      │ 0x8000 │ 0x4000 │ 0x2000 │ 0x1000  │  权限位   │
      └────────┴────────┴────────┴─────────┴──────────┘
      │←─────────── IFMT = 0xF000 ─────────→│
```

文件类型常量：
```cpp
static const unsigned int IFMT  = 0xF000;  // 文件类型掩码 (标准 Unix 4-bit)
static const unsigned int IFDIR = 0x4000;  // 目录
static const unsigned int IFCHR = 0x2000;  // 字符设备
static const unsigned int IFBLK = 0x6000;  // 块设备
static const unsigned int IALLOC = 0x8000; // 已分配
```

权限位常量（bits 8-0）：
```cpp
// 所有者 (owner)
static const unsigned int S_IRUSR = 0x0100;  // r (0400)
static const unsigned int S_IWUSR = 0x0080;  // w (0200)
static const unsigned int S_IXUSR = 0x0040;  // x (0100)
// 组用户 (group)
static const unsigned int S_IRGRP = 0x0020;  // r (0040)
static const unsigned int S_IWGRP = 0x0010;  // w (0020)
static const unsigned int S_IXGRP = 0x0008;  // x (0010)
// 其他用户 (other)
static const unsigned int S_IROTH = 0x0004;  // r (0004)
static const unsigned int S_IWOTH = 0x0002;  // w (0002)
static const unsigned int S_IXOTH = 0x0001;  // x (0001)
// 组合掩码
static const unsigned int S_IRWXU = 0x01C0;  // 0700
static const unsigned int S_IRWXG = 0x0038;  // 0070
static const unsigned int S_IRWXO = 0x0007;  // 0007
// 默认权限
static const unsigned int DEFAULT_FILE_MODE = 0x01A4;  // 0644 = rw-r--r--
static const unsigned int DEFAULT_DIR_MODE  = 0x01ED;  // 0755 = rwxr-xr-x
```

> **注意**：为避免与 POSIX `<sys/stat.h>` 中的同名宏冲突，[Inode.h:61-74](include/Inode.h#L61-L74) 使用 `#ifdef` / `#undef` 预处理块取消了 POSIX 宏，然后重新定义为类的 `static const` 成员。

---

## 4. 缓存系统

### 4.1 缓存层次概览

| 缓存类 | 文件 | 容量 | 功能 |
|--------|------|------|------|
| `BufferCache` | [include/BufferCache.h](include/BufferCache.h) | 20 个 Buf | 磁盘块数据缓存 |
| `SuperBlockCache` | [include/SuperBlockCache.h](include/SuperBlockCache.h) | 1 份 | 超级块内存副本 |
| `BlockGroupDescCache` | [include/BlockGroupDescCache.h](include/BlockGroupDescCache.h) | 1 份 | GDT 内存副本 (新增) |
| `InodeCache` | [include/InodeCache.h](include/InodeCache.h) | 128 个 | 内存 Inode 缓存 |
| `DirectoryCache` | [include/DirectoryCache.h](include/DirectoryCache.h) | 128 个 | 目录项缓存 |

### 4.2 BufferCache — 块缓冲

**文件**: [include/BufferCache.h:13-40](include/BufferCache.h#L13-L40)

经典 Unix 缓冲设计，使用 `m_Buf[]` 元数据数组 + `Buffer[]` 数据池：

```cpp
class BufferCache
{
private:
    Buf bFreeList;                       // 空闲缓冲区链表头
    Buf m_Buf[BUFFER_CACHE_NUM];         // 20 个缓冲区控制块
    DiskBlock Buffer[BUFFER_CACHE_NUM];  // 20 个 4096 字节数据区
    DiskDriver *diskDriver;

public:
    Buf *Bread(int blkno);    // 读盘块到缓存 (若已在缓存则直接返回)
    void Bwrite(Buf *bp);     // 同步写盘
    void Bdwrite(Buf *bp);    // 延迟写 (标记脏，由 LRU/Bflush 统一写出)
    void Bflush();            // 冲刷全部延迟写缓冲到磁盘
    Buf *GetBlk(int blkno);   // 为新块分配缓冲区 (不读盘)
    void Brelse(Buf *bp);     // 释放缓冲区引用
};
```

- **Bread(blkno)**：[BufferCache/BufferCache.cpp](BufferCache/BufferCache.cpp) 查找缓存，命中直接返回；未命中则通过 DiskDriver 读盘
- **Bdwrite(pBuf)**：标记 `B_DELWRI` 标志，延迟写；在 unmount 或 Bflush 时才实际写入磁盘
- **GetBlk(blkno)**：用于新分配的块——不需要从磁盘读取旧数据

### 4.3 SuperBlockCache

**文件**: [include/SuperBlockCache.h:6-30](include/SuperBlockCache.h#L6-L30)

```cpp
class SuperBlockCache
{
public:
    bool dirty = false;
    size_t SuperBlockBlockNum = 1;
    int free_inode_num, free_block_bum, total_block_num, total_inode_num;
    InodeId s_inode[MAX_INODE_NUM - 1];  // 空闲 inode 栈
    Bitmap disk_block_bitmap;            // 块位图副本

    BlkNum balloc();    // 从位图分配块 → 同步更新 GDT 计数器
    void bfree(BlkNum); // 释放块 → 同步更新 GDT
    InodeId ialloc();   // 从栈弹出 inode → 同步更新 GDT
    void ifree(InodeId);// 压回栈 → 同步更新 GDT
    void flushBack();   // 将脏 SuperBlock 写回磁盘 block 0
};
```

**flushBack() 流程** ([VFS/SuperBlockCache.cpp](VFS/SuperBlockCache.cpp))：创建栈上临时 `SuperBlock`，将缓存字段复制过去，通过 `GetBlk(0)` 获取 buffer，`memcpy` 写入，`Bdwrite` 标记延迟写。

### 4.4 BlockGroupDescCache

**文件**: [include/BlockGroupDescCache.h:11-27](include/BlockGroupDescCache.h#L11-L27)

```cpp
class BlockGroupDescCache
{
public:
    BlockGroupDesc desc;   // GDT 副本
    bool dirty;

    void mount(int blkno);  // 从指定块号加载 GDT 到内存
    void flushBack();       // 写回磁盘 block 1
};
```

**实现** ([VFS/BlockGroupDescCache.cpp](VFS/BlockGroupDescCache.cpp))：
```cpp
void BlockGroupDescCache::mount(int blkno) {
    Buf *pBuf = Kernel::instance()->getBufferCache().Bread(blkno);
    memcpy(&desc, pBuf->b_addr, sizeof(BlockGroupDesc));
    Kernel::instance()->getBufferCache().Brelse(pBuf);
}

void BlockGroupDescCache::flushBack() {
    Buf *pBuf = Kernel::instance()->getBufferCache().GetBlk(1);
    memcpy(pBuf->b_addr, &desc, sizeof(BlockGroupDesc));
    Kernel::instance()->getBufferCache().Bdwrite(pBuf);
}
```

### 4.5 InodeCache

**文件**: [include/InodeCache.h:19-36](include/InodeCache.h#L19-L36)

采用 **静态数组 + Bitmap** 方案：
```cpp
class InodeCache
{
private:
    Inode inodeCacheArea[INODE_CACHE_SIZE];   // 128 个 Inode
    Bitmap inodeCacheBitmap;                  // 管理槽位占用

public:
    Inode *getInodeByID(int inodeID);         // 按 ID 获取缓存 Inode
    int addInodeCache(DiskInode inode, InodeId inodeId);  // 加入缓存
    void clearCache();                        // 清空全部缓存
    int flushAllCacheDirtyInode();            // 将脏 Inode 全部刷回磁盘
};
```

`getInodeByID()` 实现：先在 `inodeCacheArea[]` 中搜索；未命中则通过 `Ext2::getDiskInodeByNum()` 从磁盘读取 `DiskInode`，调用 `addInodeCache()` 加载入缓存。

### 4.6 DirectoryCache

**文件**: [include/DirectoryCache.h](include/DirectoryCache.h)

```cpp
class DirectoryCache {
public:
    InodeId findInodeIdByPath(Path path);  // 按路径查找 Inode 号
};
```

---

## 5. Inode 与混合索引表

### 5.1 混合索引表常量

定义于 [include/Inode.h:45-54](include/Inode.h#L45-L54)：

```cpp
static const int ADDRESS_PER_INDEX_BLOCK = DISK_BLOCK_SIZE / sizeof(int);  // = 1024
static const int DIRECT_PTR_COUNT = 12;
static const int SINGLE_INDIRECT_IDX = 12;   // i_addr[12] → 单间接
static const int DOUBLE_INDIRECT_IDX = 13;   // i_addr[13] → 双间接
static const int TRIPLE_INDIRECT_IDX = 14;   // i_addr[14] → 三间接 (新增)

static const int SMALL_FILE_BLOCK  = 12;                                       // 直接块
static const int LARGE_FILE_BLOCK  = 12 + 1024;                                // + 单间接
static const int HUGE_FILE_BLOCK   = 12 + 1024 + 1024×1024;                    // + 双间接
static const int MAX_FILE_BLOCK    = 12 + 1024 + 1024² + 1024³;                // + 三间接
```

### 5.2 索引结构示意

```
i_addr[0]   ──→ 直接数据块 #0      (4096 bytes)
i_addr[1]   ──→ 直接数据块 #1
  ...
i_addr[11]  ──→ 直接数据块 #11     ← 共 12 个直接指针

i_addr[12]  ──→ 单间接索引块 ──→ [1024 个数据块指针]
                                   每个指向 4096 字节数据块

i_addr[13]  ──→ 双间接索引块 ──→ [1024 个单间接指针]
                                   每个 ──→ [1024 个数据块指针]

i_addr[14]  ──→ 三间接索引块 ──→ [1024 个双间接指针]
                                   每个 ──→ [1024 个单间接指针]
                                   每个 ──→ [1024 个数据块指针]
```

### 5.3 Bmap() 核心算法

**文件**: [VFS/Inode.cpp:36-197](VFS/Inode.cpp#L36-L197)

混合索引映射函数，输入逻辑块号 `lbn`，返回物理块号 `phyBlkno`，自动按需分配新块：

```cpp
int Inode::Bmap(int lbn)
{
    int phyBlkno; int *iTable; int index;
    Buf *pFirstBuf, *pSecondBuf;

    if (lbn >= Inode::MAX_FILE_BLOCK) return ERROR_LBN_OVERFLOW;

    // ===== 直接索引 (lbn < 12) =====
    if (lbn < Inode::SMALL_FILE_BLOCK) {
        phyBlkno = this->i_addr[lbn];
        if (phyBlkno == 0) {
            phyBlkno = Kernel::instance()->getSuperBlockCache().balloc(); // 按需分配
            this->i_addr[lbn] = phyBlkno;
            this->i_flag |= Inode::IUPD;
        }
        return phyBlkno;
    }

    // ===== 确定间接索引层级 =====
    if (lbn < Inode::LARGE_FILE_BLOCK)       index = SINGLE_INDIRECT_IDX;   // i_addr[12]
    else if (lbn < Inode::HUGE_FILE_BLOCK)   index = DOUBLE_INDIRECT_IDX;   // i_addr[13]
    else                                     index = TRIPLE_INDIRECT_IDX;   // i_addr[14]

    // 读取/分配第一级索引块
    phyBlkno = this->i_addr[index];
    // ... 若为 0 则 balloc() 分配 ...

    // 第二层: 双间接或三间接 → 解析外层索引
    if (index >= DOUBLE_INDIRECT_IDX) { /* 计算 outerIndex, 读取/分配 */ }

    // 第三层: 仅三间接 → 解析中层索引
    if (index == TRIPLE_INDIRECT_IDX) { /* 计算 midIndex, 读取/分配 */ }

    // 最终层: 一级索引表 → 数据块
    // 若数据块为空则 balloc() 分配
}
```

**四层遍历示例**（三间接寻址）：
1. `i_addr[14]` → 三级索引块
2. `三级索引[outerIndex]` → 二级索引块
3. `二级索引[midIndex]` → 一级索引块
4. `一级索引[innerIndex]` → 最终数据块

### 5.4 文件容量上限

| 索引级别 | 逻辑块数 | 最大文件大小 |
|----------|---------|-------------|
| 直接 (×12) | 12 | 48 KB |
| + 单间接 (×1024) | 1,036 | ~4 MB |
| + 双间接 (×1024²) | ~1,049,612 | ~4 GB |
| + 三间接 (×1024³) | ~1.07×10⁹ | ~4 TB |

---

## 6. 磁盘读写与数据流

### 6.1 DiskDriver — mmap 磁盘模拟

**文件**: [DiskDriver/DiskDriver.cpp:22-96](DiskDriver/DiskDriver.cpp#L22-L96)

```cpp
int DiskDriver::mount()
{
    DiskFd = open(DISK_IMG_FILEPATH, O_RDWR | O_CREAT, DEF_MODE);
    // 新文件：截断到 DISK_SIZE (64 MiB)
    if (lseek(DiskFd, 0, SEEK_END) < DISK_SIZE) {
        ftruncate(DiskFd, DISK_SIZE);
        retVal = 1;  // 新文件 (需 format)
    }
    // mmap 映射整个磁盘到进程地址空间
    DiskMemAddr = (DiskBlock *)mmap(nullptr, DISK_SIZE,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, DiskFd, 0);
    return retVal;
}

void DiskDriver::unmount()
{
    munmap(DiskMemAddr, DISK_SIZE);
    close(DiskFd);
}
```

`getBlk(blockNum)` 直接返回 `DiskMemAddr + blockNum`（指针算术），`readBlk`/`writeBlk` 使用 `memcpy`。

### 6.2 读文件数据流

```
cat a.txt
  → Shell::cat()                        [Shell/Shell.cpp:331]
    → VFS::open("a.txt", FREAD)        [VFS/VFS.cpp:538]
      → Ext2::locateInode(Path)         [Ext2/Ext2.cpp] → 逐级目录搜索
      → InodeCache::getInodeByID()      [VFS/InodeCache.cpp]
      → 权限检查 (owner/group/other)     [VFS/VFS.cpp:562-601]
      → FAlloc() → fd
    → VFS::read(fd, buf, len)           [VFS/VFS.cpp:636]
      → Inode::Bmap(lbn) → phyBlkno    [VFS/Inode.cpp:36]
      → BufferCache::Bread(phyBlkno)    [BufferCache/BufferCache.cpp]
    → VFS::close(fd)                    [VFS/VFS.cpp:614]
```

### 6.3 写文件数据流

```
store "hello" a.txt
  → Shell::store()                      [Shell/Shell.cpp:478]
    → VFS::open("a.txt", FWRITE)
      → 权限检查 (owner/group/other S_IW*)
    → VFS::write(fd, "hello", 5)        [VFS/VFS.cpp:700]
      → Inode::Bmap(lbn)               → 分配 + 映射
      → BufferCache::GetBlk/Bdwrite     → 延迟写
      → i_size 更新, IUPD 标志置位
    → VFS::close(fd)
```

---

## 7. 命令系统

### 7.1 命令枚举与分派

**文件**: [include/define.h:54-110](include/define.h#L54-L110) — INSTRUCT 枚举 (25 种命令)

**dispatch** 位于 [Shell/Shell.cpp:142-217](Shell/Shell.cpp#L142-L217)：

```cpp
void Shell::parseCmd()
{
    INSTRUCT inst = getInstType();

    // 精确匹配失败 → 前缀自动补全
    if (inst == ERROR_INST) {
        for (int i = 1; i < INST_NUM; i++)
            if (strncmp(instructStr[i], instStr, strlen(instStr)) == 0) { ... }
        if (match_count == 1) printf("(auto-complete: %s)\n", instructStr[match_index]);
    }

    switch (inst) {
    case MOUNT:   mount();    break;
    case FORMAT:  format();   break;
    case CD:      cd();       break;
    case LS:      ls();       break;
    case DIR:     dir();      break;   // PDF 要求的四列目录
    case TOUCH:   touch();    break;
    case RM:      rm();       break;
    case RMDIR:   rmdir();    break;
    case MKDIR:   mkdir();    break;
    case CAT:     cat();      break;
    case STORE:   store();    break;   // 写入文件
    case WITHDRAW:withdraw(); break;   // 导出文件
    case LOGIN:   login();    break;
    case USERADD: useradd();  break;
    case WHOAMI:  whoami();   break;
    case LOGOUT:  logout();   break;
    case CHMOD:   chmod();    break;
    case CHOWN:   chown();    break;
    case HISTORY: history();  break;
    // ... HELP, EXIT, VERSION, CLEAR, UNMOUNT
    }
}
```

### 7.2 dir — PDF 要求的四列格式

**文件**: [VFS/VFS.cpp:469-522](VFS/VFS.cpp#L469-L522)

```cpp
printf("%-28s  %-10s  %-10s  %s\n", "文件名", "物理地址", "保护码", "文件长度");
printf("---------------------------------------------------------------------\n");

for (int i = 0; i < DISK_BLOCK_SIZE / sizeof(DirectoryEntry); i++) {
    // 1) 文件名 — 28 字符宽
    printf("%-28s  ", p_directoryEntry->m_name);

    // 2) 物理地址 — i_addr[0] (首数据块号, 避免 Bmap 误分配)
    int firstBlk = p_fileInode->i_addr[0];
    printf("%-10s  ", firstBlk <= 0 ? "-" : itoa(firstBlk));

    // 3) 保护码 — 10 字符 Unix 格式 (如 drwxr-xr-x)
    char modeStr[11];
    modeStr[0] = (i_mode & IFDIR) ? 'd' : (i_mode & IFCHR) ? 'c' : (i_mode & IFBLK) ? 'b' : '-';
    modeStr[1] = (i_mode & S_IRUSR) ? 'r' : '-';
    // ... 逐位填充 9 个 rwx ...
    modeStr[10] = '\0';
    printf("%-10s  ", modeStr);

    // 4) 文件长度
    printf("%d\n", p_fileInode->i_size);
}
```

### 7.3 chmod / chown — 权限与所有权管理

**文件**: [Shell/Shell.cpp:727-866](Shell/Shell.cpp#L727-L866)

**chmod 流程**：
1. 解析八进制字符串（如 "755" → 0755，逐位 ×8 累加）
2. 将 POSIX 八进制位映射到自定义 Inode 权限位
3. 定位文件 inode → **检查所有权**（仅 owner 或 root 可执行）
4. 保留文件类型位 (`i_mode & IFMT`)，替换权限位
5. 标记 `IUPD` 脏标志

**chown 流程**：
1. **检查 root 权限**（`uid == 0` + `isLoggedIn`）
2. 打开 `/etc/passwd` 查找目标用户的 uid/gid
3. 定位文件 inode → 直接修改 `i_uid` / `i_gid`
4. 标记 `IUPD` 脏标志

### 7.4 login / useradd / logout — 用户管理

**文件**: [Shell/Shell.cpp:567-905](Shell/Shell.cpp#L567-L905)

- **login**：读取 `/etc/passwd`，逐字符解析 `username:password:uid:gid`，匹配后填充 `User` 结构
- **useradd**：仅 root 可执行；读取整个 `/etc/passwd` → 检查重复 → 找到最大 uid → 新用户 `uid = maxUid + 1` → 写回
- **logout**：`memset` 清零 username/password，`uid/gid = -1`（恢复 guest 状态），`isLoggedIn = false`

### 7.5 读取文件 (cat)

**文件**: [Shell/Shell.cpp:331-358](Shell/Shell.cpp#L331-L358)

```cpp
void Shell::cat() {
    FileFd fd = bounded_VFS->open(path, File::FREAD);  // 触发权限检查
    char buf[4096];
    while (!bounded_VFS->eof(fd)) {
        int readSize = bounded_VFS->read(fd, (u_int8_t*)buf + totalRead, 4096);
        totalRead += readSize;
    }
    bounded_VFS->close(fd);
    printf("%s", buf);
}
```

---

## 8. 用户与权限子系统

### 8.1 User 结构

**文件**: [include/User.h:8-41](include/User.h#L8-L41)

```cpp
class User {
public:
    Inode *u_cdir;              // 当前目录 Inode 指针
    Inode *u_pdir;              // 父目录 Inode 指针
    DirectoryEntry u_dent;      // 当前目录项
    InodeId curDirInodeId;      // 当前目录 Inode 号

    char username[32];          // 登录用户名
    char password[32];          // 登录密码 (明文存储)
    short u_uid;                // 有效用户 ID  (guest=-1, root=0)
    short u_gid;                // 有效组 ID
    bool isLoggedIn;            // 是否已登录

    OpenFiles u_ofiles;         // 进程打开文件表
    IOParameter u_IOParam;      // I/O 参数 (偏移量、目标缓冲区、剩余字节)
};
```

### 8.2 用户初始化

**文件**: [VirtualProcess/User.cpp:4-18](VirtualProcess/User.cpp#L4-L18)

```cpp
User::User() {
    u_cdir = nullptr; u_pdir = nullptr;
    curDirInodeId = 0;
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    u_uid = -1;   // guest 非 root (修复 Guest=Root 漏洞)
    u_gid = -1;
    isLoggedIn = false;
}
```

### 8.3 权限检查 — owner/group/other + root 绕过

**文件**: [VFS/VFS.cpp:562-601](VFS/VFS.cpp#L562-L601)

对每次 `open()` 调用分别检查 FREAD 和 FWRITE：

```
if uid == 0           → 直接放行 (root 特权)
elif i_uid == u_uid   → 检查 owner 的 r/w 权限位
elif i_gid == u_gid   → 检查 group 的 r/w 权限位
else                  → 检查 other 的 r/w 权限位
→ 无权限则返回 ERROR_OPEN_ILLEGAL
```

**chmod**：仅文件所有者或 root（+ 必须已登录）  
**chown**：仅 root（+ 必须已登录）  
**useradd**：仅 root（+ 必须已登录）

### 8.4 passwd 文件格式

```
root:root:0:0
alice:pass123:1001:100
bob:bob456:1002:100
```

`/etc/passwd` 在 format 时由 `createPasswdFile()` ([VFS/VFS.cpp:105-143](VFS/VFS.cpp#L105-L143)) 自动创建，初始含一行 `root:root:0:0`。

---

## 9. PDF 要求对照

| PDF 要求 | 实现位置 | 状态 |
|----------|----------|------|
| **EXT2 结构：Superblock** | [include/SuperBlock.h](include/SuperBlock.h) | ✅ |
| **EXT2 结构：Group Descriptor Table** | [include/BlockGroupDesc.h](include/BlockGroupDesc.h) | ✅ (新增) |
| **EXT2 结构：Block Bitmap** | Bitmap 内嵌于 SuperBlock | ✅ |
| **EXT2 结构：Inode Bitmap** | Bitmap 内嵌于 InodePool | ✅ |
| **EXT2 结构：Inode Table** | InodePool::inodeBlock[93] — 块 2-4 | ✅ |
| **EXT2 结构：Data Blocks** | 块 5+，由 Bmap 映射 | ✅ |
| **Inode：rwx 权限** | [include/Inode.h:77-98](include/Inode.h#L77-L98) — 9 个位常量 | ✅ |
| **Inode：owner/group** | DiskInode::d_uid / d_gid | ✅ |
| **Inode：文件容量** | DiskInode::d_size | ✅ |
| **Inode：ctime (创建时间)** | DiskInode::d_ctime, Inode::i_ctime | ✅ (新增) |
| **Inode：atime / mtime** | DiskInode::d_atime / d_mtime | ✅ |
| **Inode：12 直接指针** | i_addr[0..11] — [include/Inode.h:35](include/Inode.h#L35) | ✅ (原 6→12) |
| **Inode：1 间接 + 1 双间接 + 1 三间接** | i_addr[12/13/14] + [VFS/Inode.cpp:36](VFS/Inode.cpp#L36) Bmap 四层遍历 | ✅ (新增三间接) |
| **dir：文件名+物理地址+保护码+文件长度** | [VFS/VFS.cpp:469-522](VFS/VFS.cpp#L469-L522) — 四列格式 | ✅ |
| **create / delete** | VFS::createFile / deleteFile / mkDir / deleteDir | ✅ |
| **open / close** | VFS::open (含权限检查) / close | ✅ |
| **read / write** | VFS::read / write (含 Bmap 寻址) | ✅ |
| **login 用户登录** | Shell::login + /etc/passwd | ✅ |
| **安全性：读写保护** | [VFS/VFS.cpp:562-601](VFS/VFS.cpp#L562-L601) owner/group/other DAC | ✅ |
| **chmod** | [Shell/Shell.cpp:727](Shell/Shell.cpp#L727) — 含所有权检查 | ✅ |
| **chown** | [Shell/Shell.cpp:794](Shell/Shell.cpp#L794) — 含 root 检查 | ✅ |
| **useradd (auto uid)** | [Shell/Shell.cpp:636](Shell/Shell.cpp#L636) — maxUid+1 递增 | ✅ |
| **guest ≠ root** | [VirtualProcess/User.cpp:13](VirtualProcess/User.cpp#L13) — uid=-1 | ✅ |
