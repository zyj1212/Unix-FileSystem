# Unix FileSystem 系统架构文档

> 同济大学操作系统课程设计 — 用户级 EXT2 文件系统模拟器  
> 基于《要求.pdf》规范实现

---

## 1. 系统概述

### 1.1 项目定位

Unix FileSystem 是一个**用户级文件系统模拟器**，使用 C++ 实现。它通过一个二进制文件 (`disk.img`) 模拟磁盘存储，在内存中构建完整的 EXT2 文件系统逻辑，对外提供交互式 Shell 供用户操作。

### 1.2 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                      Shell 层                            │
│  命令解析 · 用户交互 · login/useradd/chmod/chown/cat …  │
├─────────────────────────────────────────────────────────┤
│                       VFS 层                             │
│  虚拟文件系统 · open/close/read/write · dir/ls/cd       │
│  权限检查（owner/group/other rwx）                      │
├─────────────────────────────────────────────────────────┤
│                      Cache 层                            │
│  InodeCache · DirectoryCache · SuperBlockCache          │
│  BufferCache · BlockGroupDescCache                      │
├─────────────────────────────────────────────────────────┤
│                      Ext2 层                             │
│  format/mount · balloc/bfree · ialloc/ifree             │
│  locateInode · getDiskInodeByNum · updateDiskInode      │
├─────────────────────────────────────────────────────────┤
│                    DiskDriver 层                          │
│  mmap 映射 disk.img · 直接内存读写                       │
└─────────────────────────────────────────────────────────┘
```

### 1.3 源文件目录结构

```
Unix-FileSystem/
├── main.cpp                  # 程序入口
├── Makefile                  # 构建脚本
├── include/                  # 头文件
│   ├── define.h              # 系统常量定义
│   ├── Inode.h               # 内存 Inode 类
│   ├── DiskInode.h           # 磁盘 Inode 结构
│   ├── User.h                # 用户结构
│   ├── Shell.h               # 命令解释器
│   ├── VFS.h                 # 虚拟文件系统接口
│   ├── Ext2.h                # EXT2 文件系统逻辑
│   ├── SuperBlock.h          # 超级块结构
│   ├── SuperBlockCache.h     # 超级块缓存
│   ├── BlockGroupDesc.h      # 组描述符结构 (新增)
│   ├── BlockGroupDescCache.h # 组描述符缓存 (新增)
│   ├── InodePool.h           # Inode 池（磁盘 Inode 区）
│   ├── Bitmap.h              # 位图工具类
│   ├── BufferCache.h         # 缓冲缓存
│   ├── InodeCache.h          # Inode 缓存
│   ├── DirectoryCache.h      # 目录缓存
│   ├── DirectoryEntry.h      # 目录项结构
│   ├── File.h                # 文件描述符
│   ├── OpenFileTable.h       # 打开文件表
│   ├── BufferLruList.h       # LRU 链表
│   ├── DiskDriver.h          # 磁盘驱动
│   ├── DiskBlock.h           # 磁盘块类型定义
│   ├── Buf.h                 # 缓存块
│   ├── Path.h                # 路径解析
│   ├── Kernel.h              # 内核单例
│   ├── VirtualProcess.h      # 虚拟进程
│   ├── Logcat.h              # 日志工具
│   └── TimeHelper.h          # 时间工具
├── Ext2/                     # EXT2 层实现
│   ├── Ext2.cpp
│   ├── SuperBlock.cpp
│   ├── InodePool.cpp
│   ├── DiskInode.cpp
│   └── Path.cpp
├── VFS/                      # VFS + Cache 层实现
│   ├── VFS.cpp
│   ├── Inode.cpp
│   ├── File.cpp
│   ├── InodeCache.cpp
│   ├── DirectoryCache.cpp
│   ├── DirectoryEntry.cpp
│   ├── SuperBlockCache.cpp
│   ├── BlockGroupDescCache.cpp  (新增)
│   └── OpenFileTable.cpp
├── Shell/                    # Shell 实现
│   └── Shell.cpp
├── VirtualProcess/           # 虚拟进程实现
│   ├── Kernel.cpp
│   ├── VirtualProcess.cpp
│   └── User.cpp
├── BufferCache/              # 缓冲缓存实现
│   ├── BufferCache.cpp
│   ├── Buf.cpp
│   └── BufferLruList.cpp
├── DiskDriver/               # 磁盘驱动实现
│   └── DiskDriver.cpp
├── Utils/                    # 工具类实现
│   ├── Bitmap.cpp
│   ├── Logcat.cpp
│   └── TimeHelper.cpp
├── TEST_CASES.md             # 测试用例文档
└── ARCHITECTURE.md           # 本文档
```

---

## 2. 磁盘布局（EXT2 模拟）

### 2.1 磁盘模拟

| 参数 | 值 | 定义位置 |
|------|-----|----------|
| 磁盘大小 | 64 MiB | `DISK_SIZE` |
| 磁盘块大小 | 4096 字节 | `DISK_BLOCK_SIZE` |
| 磁盘块总数 | 16384 | `DISK_BLOCK_NUM` |
| 模拟文件 | `./disk.img` | `DISK_IMG_FILEPATH` |

通过 `mmap()` 将 `disk.img` 映射到进程地址空间，直接使用内存指针读写磁盘数据。

### 2.2 磁盘布局图

```
┌─────────────────────────────────────────────────┐
│ Block 0         │ SuperBlock                    │  ← 超级块（含 block bitmap）
│                 │ (4096 字节，含 padding)        │
├─────────────────────────────────────────────────┤
│ Block 1         │ BlockGroupDesc                │  ← 组描述符表（新增）
│                 │ (4096 字节，含 padding)        │
├─────────────────────────────────────────────────┤
│ Block 2 ~ 4     │ InodePool                     │  ← Inode 位图 + Inode 表
│                 │ Bitmap + Padding + DiskInode[] │     (93 个 inode × 88 字节)
├─────────────────────────────────────────────────┤
│ Block 5         │ Root Directory (/)            │  ← 根目录数据
├─────────────────────────────────────────────────┤
│ Block 6         │ /bin Directory                │
├─────────────────────────────────────────────────┤
│ Block 7         │ /etc Directory                │
├─────────────────────────────────────────────────┤
│ Block 8         │ /home Directory               │
├─────────────────────────────────────────────────┤
│ Block 9         │ /dev Directory                │
├─────────────────────────────────────────────────┤
│ Block 10 ~ 16383│ Free Data Blocks              │  ← 用户文件数据
└─────────────────────────────────────────────────┘
```

### 2.3 各组件说明

#### SuperBlock（超级块）
- 位置：Block 0
- 大小：恰好 4096 字节（手工 `char padding[1504]` 填充）
- 核心字段：
  - `SuperBlockBlockNum` — 超级块自身占用的块数（=1）
  - `free_block_bum` / `total_block_num` — 空闲 / 总磁盘块计数
  - `free_inode_num` / `total_inode_num` — 空闲 / 总 inode 计数
  - `s_inode[]` — **空闲 inode 栈**（LIFO），用于 inode 分配
  - `disk_block_bitmap` — **块位图**（Bitmap 类），管理磁盘块的分配与回收

#### BlockGroupDesc（组描述符表）
- 位置：Block 1
- 大小：恰好 4096 字节（`char padding[4096-24]` 填充）
- 字段：
  - `bg_block_bitmap` — 块位图所在块号（=0）
  - `bg_inode_bitmap` — Inode 位图所在块号（=2）
  - `bg_inode_table` — Inode 表起始块号（=3）
  - `bg_free_blocks_count` — 组内空闲块计数
  - `bg_free_inodes_count` — 组内空闲 inode 计数
  - `bg_used_dirs_count` — 组内已用目录计数（初始 5）
- 缓存层：`BlockGroupDescCache` 类在 VFS 层维护内存副本

#### InodePool（Inode 池）
- 位置：Block 2 ~ 4（3 个磁盘块）
- 内部结构：

| 偏移 | 字段 | 大小 |
|------|------|------|
| 0 | `Bitmap inodePoolBitmap` | ~2048 字节 |
| ~2048 | `char padding[2056]` | 2056 字节 |
| ~4104 | `DiskInode inodeBlock[93]` | 93 × 88 = 8184 字节 |

- 总大小：2048 + 2056 + 8184 = 12288 = 3 × 4096

#### Block Bitmap（块位图）
- 内嵌于 SuperBlock，管理 16384 个数据块的分配状态
- 每个 bit 对应一个磁盘块：1=已占用，0=空闲

#### Inode Bitmap（Inode 位图）
- 内嵌于 InodePool，管理 93 个 inode 的分配状态

---

## 3. Inode 结构（PDF 核心要求）

### 3.1 磁盘 Inode（DiskInode）

大小：**88 字节**（`DISKINODE_SIZE`）

| 字段 | 类型 | 大小 | 说明 |
|------|------|------|------|
| `d_mode` | `unsigned int` | 4 | 文件类型 + rwx 权限位 |
| `d_nlink` | `int` | 4 | 硬链接计数 |
| `d_uid` | `short` | 2 | 文件所有者 ID |
| `d_gid` | `short` | 2 | 文件所属组 ID |
| `d_size` | `int` | 4 | 文件大小（字节） |
| `d_addr[15]` | `int[15]` | 60 | **混合索引表**（12+1+1+1） |
| `d_atime` | `int` | 4 | 最近访问时间 |
| `d_mtime` | `int` | 4 | 最近修改时间 |
| `d_ctime` | `int` | 4 | 创建/变更时间（新增） |

### 3.2 内存 Inode（Inode）

在 DiskInode 基础上增加了运行时字段：`i_flag`（状态标志）、`i_count`（引用计数）、`i_dev`、`i_number`（inode 号）、`i_lastr`（预读块号）、`i_ctime`。

### 3.3 i_mode 位布局

```
Bit:  15        14        13        12        8  7     0
     ┌─────────┬─────────┬─────────┬─────────┬──────────┐
     │ IALLOC  │ IFDIR   │ IFCHR   │ (free)  │ rwxrwxrwx│
     │ 0x8000  │ 0x4000  │ 0x2000  │ 0x1000  │  权限位   │
     └─────────┴─────────┴─────────┴─────────┴──────────┘
     │←────────── IFMT = 0xF000 ──────────→│
```

**文件类型**（通过 `i_mode & IFMT` 判断）：

| 常量 | 值 | 含义 |
|------|-----|------|
| （无位） | 0x0000 | 普通文件 |
| `IFCHR` | 0x2000 | 字符设备 |
| `IFDIR` | 0x4000 | 目录文件 |
| `IFBLK` | 0x6000 | 块设备 |

**权限位**（9 bits，从高到低为 owner/group/other × rwx）：

| 常量 | 值 | 含义 |
|------|-----|------|
| `S_IRUSR` | 0x0100 | 所有者读 |
| `S_IWUSR` | 0x0080 | 所有者写 |
| `S_IXUSR` | 0x0040 | 所有者执行 |
| `S_IRGRP` | 0x0020 | 组用户读 |
| `S_IWGRP` | 0x0010 | 组用户写 |
| `S_IXGRP` | 0x0008 | 组用户执行 |
| `S_IROTH` | 0x0004 | 其他用户读 |
| `S_IWOTH` | 0x0002 | 其他用户写 |
| `S_IXOTH` | 0x0001 | 其他用户执行 |

**默认权限**：文件 `0644`（rw-r--r--）、目录 `0755`（rwxr-xr-x）。

### 3.4 混合索引表（12 直接 + 1 单间接 + 1 双间接 + 1 三间接）

```
i_addr[0]   ──→ 直接数据块 #0
i_addr[1]   ──→ 直接数据块 #1
  ...
i_addr[11]  ──→ 直接数据块 #11
i_addr[12]  ──→ 单间接索引块 → [1024 个数据块指针]
i_addr[13]  ──→ 双间接索引块 → [1024 个单间接指针] → 各 [1024 个数据块指针]
i_addr[14]  ──→ 三间接索引块 → [1024 个双间接指针] → ... → 数据块
```

**文件容量上限**：

| 索引级别 | 可寻址逻辑块数 | 最大文件大小 |
|----------|---------------|-------------|
| 直接 (12) | 12 | 48 KB |
| + 单间接 (×1024) | 1,036 | ~4 MB |
| + 双间接 (×1024²) | 1,049,612 | ~4 GB |
| + 三间接 (×1024³) | ~1.07×10⁹ | ~4 TB |

**核心算法**：`Inode::Bmap(int lbn)` — 逻辑块号 → 物理块号映射，自动为不存在的块分配磁盘空间。

---

## 4. 用户与权限子系统

### 4.1 User 结构体

```cpp
class User {
    Inode *u_cdir;               // 当前目录 Inode 指针
    Inode *u_pdir;               // 父目录 Inode 指针
    DirectoryEntry u_dent;       // 当前目录项
    InodeId curDirInodeId;       // 当前目录 Inode 号
    char username[32];           // 登录用户名
    char password[32];           // 登录用户密码
    short u_uid;                 // 有效用户 ID（guest=-1, root=0）
    short u_gid;                 // 有效组 ID
    bool isLoggedIn;             // 是否已登录
    OpenFiles u_ofiles;          // 进程打开文件表
    IOParameter u_IOParam;       // I/O 参数（偏移量等）
};
```

### 4.2 认证流程

```
┌──────────┐    login      ┌──────────────┐    读取      ┌───────────┐
│  guest   │ ───────────→  │ Shell::login │ ──────────→ │/etc/passwd│
│ uid=-1   │               │ 验证密码      │ ←────────── │(明文存储)  │
└──────────┘               └──────┬───────┘             └───────────┘
                                  │ 验证通过
                                  ▼
                          ┌──────────────┐
                          │  已登录用户   │
                          │ uid=从passwd │
                          └──────────────┘
```

- `/etc/passwd` 格式：`username:password:uid:gid`
- format 时自动创建 `root:root:0:0` 和 `/etc/passwd` 文件
- `useradd` 自动递增 uid（从 1001 开始，依次递增）
- 未登录 guest 的 uid=-1，**不是 root**，无法绕过权限检查

### 4.3 权限检查模型

**VFS::open() 中实施**（[VFS.cpp:555-593](VFS/VFS.cpp#L555-L593)）：

```
if (uid == 0)          → 直接放行（root 特权）
elif (i_uid == u_uid)  → 检查 owner 权限位
elif (i_gid == u_gid)  → 检查 group 权限位
else                   → 检查 other 权限位
→ 无权限则返回 ERROR_OPEN_ILLEGAL
```

**chmod**：仅文件所有者或 root 可执行（+ 必须已登录）  
**chown**：仅 root 可执行（+ 必须已登录）

---

## 5. 命令系统

### 5.1 命令列表

| 命令 | 功能 | 对应 PDF 要求 |
|------|------|--------------|
| `mount` | 挂载文件系统 | — |
| `format` | 格式化磁盘 | — |
| `unmount` | 卸载文件系统 | — |
| `dir` | 列目录（文件名+物理地址+保护码+文件长度） | ✅ dir 命令格式 |
| `ls` | 简要列表 | — |
| `cd` | 切换目录 | — |
| `mkdir` | 创建目录 | ✅ create |
| `rmdir` | 删除目录（递归） | ✅ delete |
| `touch` | 创建普通文件 | ✅ create |
| `rm` | 删除普通文件 | ✅ delete |
| `cat` | 读取并打印文件内容 | ✅ read |
| `store` | 写入内容到文件 | ✅ write |
| `withdraw` | 导出文件到宿主机 | — |
| `open` / `close` | VFS 内部文件打开/关闭 | ✅ open/close |
| `login` | 用户登录 | ✅ login |
| `logout` | 退出登录 | ✅ 用户交互 |
| `useradd` | 创建新用户（root 权限） | ✅ 用户管理 |
| `whoami` | 显示当前用户 | ✅ 用户交互 |
| `chmod` | 修改文件权限 | ✅ 读写保护 |
| `chown` | 修改文件所有者 | ✅ 读写保护 |
| `history` | 显示命令历史 | — |
| `help` | 帮助信息 | — |
| `version` | 版本信息 | — |
| `clear` | 清屏 | — |
| `exit` | 退出程序 | — |

### 5.2 命令解析流程

```
用户输入 → readUserInput()
  → 显示提示符 (guest$ 或 username@user_fs:/$)
  → 记录历史
  → split_cmd() 分割参数
  → parseCmd() 匹配命令（支持前缀自动补全）
      ├── INSTRUCT 枚举分发
      └── 调用对应的 Shell 方法
```

### 5.3 dir 命令输出格式

严格按照 PDF 要求实现（[VFS.cpp:467-513](VFS/VFS.cpp#L467-L513)）：

```
文件名                        物理地址    保护码      文件长度
.                            5          drwxr-xr-x  192
..                           5          drwxr-xr-x  192
bin                          6          drwxr-xr-x  64
etc                          7          drwxr-xr-x  64
a.txt                        10         -rw-r--r--  0
```

四列分别为：文件名（28 字符宽）、物理地址（第一数据块号）、保护码（10 字符 Unix 格式）、文件长度（字节）。

---

## 6. 缓存系统

### 6.1 缓存层次

| 缓存类 | 缓存内容 | 容量 |
|--------|----------|------|
| `BufferCache` | 磁盘块数据 | 20 个 Buf |
| `InodeCache` | 内存 Inode | 128 个 |
| `DirectoryCache` | 目录项 | 128 个 |
| `SuperBlockCache` | 超级块副本 | 1 份 |
| `BlockGroupDescCache` | 组描述符副本 | 1 份 |

### 6.2 BufferCache 写策略

- **Bread(blkno)**：读磁盘块，若缓存未命中则从磁盘加载
- **Bdwrite(pBuf)**：延迟写 — 标记缓存为脏，由 LRU 淘汰时写回
- **Brelse(pBuf)**：释放缓存引用
- **GetBlk(blkno)**：获取缓存块（用于新分配）

### 6.3 SuperBlockCache 同步

`balloc()/bfree()/ialloc()/ifree()` 每次调用同步更新：
1. SuperBlockCache 自身计数器
2. BlockGroupDescCache 对应的 `bg_free_*` 计数器
3. 标记脏标志，后续 `flushBack()` 写回磁盘

---

## 7. 关键算法

### 7.1 Bmap() — 逻辑块号到物理块号映射

```
输入: lbn (逻辑块号)
输出: phyBlkno (物理盘块号)

if lbn >= MAX_FILE_BLOCK → ERROR (超出范围)
if lbn < 12              → 直接索引: i_addr[lbn]
if lbn < 1036            → 单间接: i_addr[12] → 索引块[lbn-12]
if lbn < 1049612         → 双间接: i_addr[13] → 二级索引 → 一级索引 → 数据块
else                     → 三间接: i_addr[14] → 三级索引 → 二级索引 → 一级索引 → 数据块

若索引块/数据块不存在(==0)，自动调用 balloc() 分配
```

### 7.2 locateInode() — 路径到 Inode 号的解析

```
Path("/a/b/c.txt")
  → 从根目录(或当前目录)开始
  → 逐级调用 getInodeIdInDir() 线性搜索目录项
  → 返回目标 inode 号
```

### 7.3 空闲 inode 栈分配

SuperBlock 维护一个 LIFO 栈 `s_inode[]`：
- `ialloc()`：弹出栈顶元素并返回
- `ifree()`：将释放的 inode 号压入栈顶
- 初始化时栈中按降序排列全部可用 inode 号

---

## 8. 数据流示意

### 8.1 读取文件流程

```
Shell::cat("a.txt")
  → VFS::open("a.txt", FREAD)
    → Ext2::locateInode("a.txt") → inodeId
    → InodeCache::getInodeByID(inodeId) → pInode
    → 权限检查 (u_uid vs i_uid/i_gid vs S_IR*)
    → FAlloc() → 分配 File 结构 → 返回 fd
  → VFS::read(fd, buf, len)
    → Bmap(lbn) → phyBlkno
    → BufferCache::Bread(phyBlkno) → 数据
    → 返回数据字节数
  → VFS::close(fd)
```

### 8.2 写入文件流程

```
Shell::store("hello", "a.txt")
  → VFS::open("a.txt", FWRITE)
    → 权限检查 (S_IW*)
  → VFS::write(fd, "hello", 5)
    → Bmap(lbn) → 分配 + 映射
    → BufferCache::GetBlk/Bdwrite
    → i_size 更新, i_flag |= IUPD
  → VFS::close(fd)
```

---

## 9. 与 PDF 要求的对照总结

| PDF 要求 | 实现模块 | 状态 |
|----------|----------|------|
| EXT2 模拟：Superblock | `SuperBlock.h/cpp` | ✅ |
| EXT2 模拟：Group Descriptor Table | `BlockGroupDesc.h/cpp` | ✅（新增） |
| EXT2 模拟：Block Bitmap | `Bitmap`（内嵌于 SuperBlock） | ✅ |
| EXT2 模拟：Inode Bitmap | `Bitmap`（内嵌于 InodePool） | ✅ |
| EXT2 模拟：Inode Table | `InodePool`（DiskInode 数组） | ✅ |
| EXT2 模拟：Data Blocks | Block 10+，通过 Bmap 映射 | ✅ |
| Inode：rwx 权限 | `i_mode` 位 8-0，9 个权限常量 | ✅ |
| Inode：owner/group | `i_uid` / `i_gid` | ✅ |
| Inode：文件容量 | `i_size` | ✅ |
| Inode：ctime | `i_ctime` / `d_ctime` | ✅（新增） |
| Inode：atime | `i_atime` / `d_atime` | ✅ |
| Inode：mtime | `i_mtime` / `d_mtime` | ✅ |
| Inode：12 直接指针 | `i_addr[0..11]` | ✅（原 6 → 12） |
| Inode：1 间接 + 1 双间接 + 1 三间接 | `i_addr[12/13/14]` + 四层 Bmap | ✅（新增三间接） |
| 文件操作：dir | `VFS::dir()` — 四列输出 | ✅ |
| 文件操作：create | `VFS::createFile()` / `mkDir()` | ✅ |
| 文件操作：delete | `VFS::deleteFile()` / `deleteDir()` | ✅ |
| 文件操作：open/close | `VFS::open()` / `VFS::close()` | ✅ |
| 文件操作：read/write | `VFS::read()` / `VFS::write()` | ✅ |
| 用户交互：login | `Shell::login()` + `/etc/passwd` | ✅ |
| 安全性：读写保护 | VFS::open() 权限检查 + chmod/chown | ✅ |
