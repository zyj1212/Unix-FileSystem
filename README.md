# Unix-FileSystem — 操作系统课程设计（实验二）

基于开源 Unix 文件系统项目扩展开发的用户级文件系统模拟器。

---

## 项目简介

本项目以一个大文件作为虚拟磁盘，模拟 Ext2 文件系统的核心机制，包括：
- 虚拟磁盘管理（mount/format/unmount）
- 文件与目录的创建、删除、读写
- 多级缓存加速（BufferCache、InodeCache、SuperBlockCache）
- 混合索引表支持小/大/巨型文件

**实验二扩展**：三人协作，在原有基础上新增用户登录系统、文件权限控制、目录详细信息查看与命令增强。

---

## 实验二 功能扩展

| 同学 | 负责模块 | 新增命令 |
|------|---------|---------|
| **同学A** | 多用户与登录验证 | `login`、`useradd`、`whoami` |
| **同学B** | 文件信息查看与命令增强 | `dir`、`history`、命令前缀自动补全 |
| **同学C** | 文件权限控制 | `chmod`、`chown`、读写权限校验拦截 |

---

## 支持的全部命令

### 基础命令（实验一）

| 命令 | 功能 | 用法示例 |
|------|------|---------|
| `mount` | 挂载虚拟磁盘 | `mount` |
| `unmount` | 卸载虚拟磁盘 | `unmount` |
| `format` | 格式化磁盘（自动创建 `/etc/passwd`） | `format` |
| `mkdir` | 创建目录 | `mkdir dirname` |
| `touch` | 创建空文件 | `touch filename` |
| `rm` | 删除文件 | `rm filename` |
| `rmdir` | 递归删除目录 | `rmdir dirname` |
| `ls` | 列出目录内容 | `ls [path]` |
| `cd` | 切换当前目录 | `cd path` |
| `store` | 外部文件存入虚拟磁盘 | `store /home/a.jpg inner.jpg` |
| `withdraw` | 从虚拟磁盘提取文件 | `withdraw inner.jpg /home/b.jpg` |
| `clear` | 清屏 | `clear` |
| `help` | 显示帮助 | `help` |
| `version` | 显示版本信息 | `version` |
| `exit` | 退出程序（自动卸载） | `exit` |

### 扩展命令（实验二）

| 命令 | 功能 | 用法示例 | 权限要求 |
|------|------|---------|---------|
| `login` | 用户登录验证 | `login root root` | 无 |
| `useradd` | 创建新用户 | `useradd alice 123456` | 仅 root |
| `whoami` | 显示当前用户 | `whoami` | 无 |
| `chmod` | 修改文件权限 | `chmod 755 test.txt` | 无 |
| `chown` | 修改文件所有者 | `chown alice file.txt` | 仅 root |
| `dir` | 显示目录详细信息 | `dir [path]` | 无 |
| `history` | 查看历史命令 | `history` | 无 |

---

## 快速开始

### 编译

```bash
make
```

### 运行

```bash
./user_fs
```

### 典型操作流程

```bash
mount           # 挂载虚拟磁盘（首次会自动创建 disk.img）
format          # 格式化磁盘，自动生成 /etc/passwd（含 root:root:0:0）
login root root # root 登录
whoami          # 显示当前用户：root (uid=0)
mkdir test
cd test
touch hello.txt
dir             # 查看详细信息（文件名、物理地址、保护码、文件长度）
history         # 查看输入过的命令历史
chmod 644 hello.txt
exit            # 退出并自动卸载
```

---

## 系统架构

### 整体层次

```
Shell（命令解析层）
   ↓
VFS（虚拟文件系统层）—— InodeCache / DirectoryCache / SuperBlockCache
   ↓
Ext2（文件系统实现层）
   ↓
BufferCache（磁盘缓存层）—— LRU 管理
   ↓
DiskDriver（磁盘驱动层）—— mmap 操作虚拟磁盘文件
```

### 关键模块

| 模块 | 路径 | 说明 |
|------|------|------|
| Shell | `Shell/Shell.cpp` | 命令解析、用户交互 |
| VFS | `VFS/VFS.cpp` | 文件系统统一接口 |
| Ext2 | `Ext2/Ext2.cpp` | Ext2 格式实现 |
| BufferCache | `BufferCache/` | 磁盘块 LRU 缓存 |
| DiskDriver | `DiskDriver/DiskDriver.cpp` | 虚拟磁盘底层读写 |

---

## 实验二 技术要点

### 1. 多用户登录系统（同学A）

- 格式化时自动创建 `/etc/passwd`，写入默认 root 用户 `root:root:0:0`
- `login` 命令读取 `/etc/passwd` 逐行匹配用户名和密码
- `useradd` 仅允许 root 执行，向 `/etc/passwd` 追加新用户记录
- 登录后命令提示符显示当前用户名（如 `root@user_fs:/$`）

### 2. 文件信息查看与命令增强（同学B）

- `dir`：遍历目录文件的数据块，读取每个目录项对应的 inode，输出**文件名、首物理块号、保护码（rwx）、文件长度**
- `history`：Shell 内部维护 `history_buf[HISTORY_MAX]` 环形缓冲区，记录用户输入
- **命令前缀自动补全**：输入部分前缀（如 `hi`），若唯一匹配则自动补全为 `history`；若多个匹配则提示候选命令

### 3. 文件权限控制（同学C）

- `chmod`：解析八进制权限值（如 `755`），转换为 `S_IRUSR/S_IWUSR/...` 位，写入 inode 的 `i_mode`
- `chown`：仅 root 可执行，修改 inode 的 `i_uid` 和 `i_gid`
- **权限校验拦截**：在 `VFS::open()` 中，根据当前用户的 UID/GID 与文件的 `i_mode` 权限位进行匹配校验，无权限则拒绝打开并返回错误

---

## 工程结构

```
.
├── main.cpp                  # 程序入口
├── makefile                  # 编译脚本
├── help                      # 帮助文本
├── version                   # 版本信息
├── include/                  # 头文件
│   ├── define.h              # 全局定义、命令枚举
│   ├── Shell.h               # Shell 类
│   ├── VFS.h                 # VFS 类
│   ├── Inode.h               # Inode + 权限位定义
│   ├── User.h                # 用户类（含登录状态）
│   └── ...
├── Shell/Shell.cpp           # 命令解析与执行
├── VFS/VFS.cpp               # 虚拟文件系统
├── Ext2/                     # Ext2 实现
├── BufferCache/              # 磁盘缓存
├── DiskDriver/               # 磁盘驱动
├── VirtualProcess/           # 进程/用户管理
└── Utils/                    # 工具类
```

---

## 开发环境

- **操作系统**：Linux / Ubuntu（推荐）
- **编译器**：g++
- **构建工具**：make
- **C++ 标准**：C++11

---

## 在线仓库

[https://github.com/zyj1212/Unix-FileSystem](https://github.com/zyj1212/Unix-FileSystem)

---

## 参考资料

1. [IBM - Linux 文件系统](https://www.ibm.com/developerworks/cn/linux/l-linux-filesystem/index.html)
2. [The Linux Kernel](http://www.tldp.org/LDP/tlk/tlk.html)
3. [Introduction about VFS](https://segmentfault.com/a/1190000008476809)