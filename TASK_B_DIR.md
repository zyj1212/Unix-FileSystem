# 🧑‍💻 同学B任务：dir命令完善 + 命令历史

**分支：** `feature-B-dir`

## 任务目标
实现 `dir` 命令（列目录时显示：文件名、物理地址、保护码、文件长度），实现 `history` 命令历史功能。

---

## 需要修改的文件

### 1. `include/define.h` — 添加新指令枚举

在 `INSTRUCT` 枚举中添加：
```cpp
DIR,
HISTORY
```

在 `INST_NUM` 从 `16` 改为 `18`

在 `instructStr[]` 数组中添加：
```cpp
"dir",
"history"
```

### 2. `include/Shell.h` — 添加新命令方法和成员

在 Shell 类中添加：
```cpp
// 新命令
void dir();
void history();

// 命令历史记录
static const int HISTORY_MAX = 50;
char cmd_history[HISTORY_MAX][MAX_CMD_LEN];
int history_count;
```

并在构造函数中初始化 `history_count = 0`。

### 3. `Shell/Shell.cpp` — 实现新功能

#### 3.1 在 `parseCmd()` 的 switch 中添加：
```cpp
case DIR:
    dir();
    break;
case HISTORY:
    history();
    break;
```

#### 3.2 在 `readUserInput()` 中记录命令历史

在获取到用户输入后（第25行 `std::cin.getline` 之后），添加：
```cpp
// 记录命令历史
if (strlen(tty_buffer) > 0) {
    if (history_count < HISTORY_MAX) {
        strcpy(cmd_history[history_count], tty_buffer);
        history_count++;
    } else {
        // 循环覆盖最旧的记录
        for (int i = 1; i < HISTORY_MAX; i++) {
            strcpy(cmd_history[i-1], cmd_history[i]);
        }
        strcpy(cmd_history[HISTORY_MAX-1], tty_buffer);
    }
}
```

#### 3.3 实现 `history()` 命令：

```cpp
void Shell::history()
{
    int start = (history_count > 20) ? (history_count - 20) : 0;
    for (int i = start; i < history_count; i++) {
        printf("%4d  %s\n", i + 1, cmd_history[i]);
    }
}
```

#### 3.4 实现 `dir()` 命令——核心任务 🎯

`dir` 命令需要显示：**文件名、物理地址（块号）、保护码（rwx）、文件长度**

```cpp
void Shell::dir()
{
    InodeId targetDirInodeId;
    
    // 获取目标目录的 Inode ID
    if (getParamAmount() < 2) {
        // 不带参数，使用当前目录
        targetDirInodeId = VirtualProcess::Instance()->getUser().curDirInodeId;
    } else {
        // 带参数，解析路径
        Path targetPath(getParam(1));
        Ext2 *ext2 = Kernel::instance()->getExt2();
        targetDirInodeId = ext2->locateDir(targetPath);
        if (targetDirInodeId < 0) {
            Logcat::log("目录不存在！");
            return;
        }
    }
    
    // 打印表头
    printf("%-28s  %-10s  %-10s  %s\n", "文件名", "物理地址", "保护码", "文件长度");
    printf("%-28s  %-10s  %-10s  %s\n", "------", "--------", "------", "--------");
    
    // 获取目录的 Inode
    Inode *dirInode = Kernel::instance()->getInodeCache().getInodeByID(targetDirInodeId);
    if (!dirInode) {
        Logcat::log("无法获取目录信息！");
        return;
    }
    
    // 计算目录大小占多少块
    int blockCount = (dirInode->i_size + DISK_BLOCK_SIZE - 1) / DISK_BLOCK_SIZE;
    
    // 遍历所有目录项
    for (int blk = 0; blk < blockCount; blk++) {
        // 获取物理块号
        int phyBlkno = dirInode->Bmap(blk);
        if (phyBlkno < 0) continue;
        
        // 读取磁盘块
        Buf *pBuf = Kernel::instance()->getBufferCache().Bread(phyBlkno);
        DirectoryEntry *entries = (DirectoryEntry *)pBuf->b_addr;
        int entryCount = DISK_BLOCK_SIZE / sizeof(DirectoryEntry);
        
        for (int i = 0; i < entryCount; i++) {
            // 目录项第一个字节为0表示空项
            if (entries[i].d_ino == 0) continue;
            
            char fileName[32];
            strncpy(fileName, entries[i].d_name, 28);
            fileName[28] = '\0';
            
            // 跳过 . 和 ..
            if (strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0) continue;
            
            // 获取该文件的 Inode
            Inode *fileInode = Kernel::instance()->getInodeCache().getInodeByID(entries[i].d_ino);
            if (!fileInode) continue;
            
            // ===== 获取物理地址（第一个数据块号）=====
            int firstBlock = fileInode->Bmap(0);
            char blockAddr[16];
            if (firstBlock >= 0) {
                snprintf(blockAddr, sizeof(blockAddr), "block:%d", firstBlock);
            } else {
                snprintf(blockAddr, sizeof(blockAddr), "无");
            }
            
            // ===== 获取保护码（rwx格式）=====
            char permStr[12];
            buildPermissionString(fileInode->i_mode, permStr);
            
            // ===== 打印一行 =====
            printf("%-28s  %-10s  %-10s  %d\n", 
                   fileName, blockAddr, permStr, fileInode->i_size);
        }
        
        Kernel::instance()->getBufferCache().Brelse(pBuf);
    }
}
```

#### 3.5 添加辅助函数 `buildPermissionString`：

在 Shell 类中添加或作为文件内静态函数：

```cpp
/**
 * 将 i_mode 转换为 rwx 权限字符串
 */
void Shell::buildPermissionString(unsigned int mode, char *outStr)
{
    // 文件类型
    if ((mode & Inode::IFMT) == Inode::IFDIR) {
        outStr[0] = 'd';
    } else if ((mode & Inode::IFMT) == Inode::IFCHR) {
        outStr[0] = 'c';
    } else {
        outStr[0] = '-';
    }
    
    // 所有者权限（目前还没有扩展，先显示模拟权限）
    // 这里先写死，等同学C实现权限位后再修改
    outStr[1] = (mode & 0x0100) ? 'r' : '-';
    outStr[2] = (mode & 0x0080) ? 'w' : '-';
    outStr[3] = (mode & 0x0040) ? 'x' : '-';
    
    // 组用户权限
    outStr[4] = (mode & 0x0020) ? 'r' : '-';
    outStr[5] = (mode & 0x0010) ? 'w' : '-';
    outStr[6] = (mode & 0x0008) ? 'x' : '-';
    
    // 其他用户权限
    outStr[7] = (mode & 0x0004) ? 'r' : '-';
    outStr[8] = (mode & 0x0002) ? 'w' : '-';
    outStr[9] = (mode & 0x0001) ? 'x' : '-';
    
    outStr[10] = '\0';
}
```

注意需要在 `Shell.h` 中声明该方法：
```cpp
void buildPermissionString(unsigned int mode, char *outStr);
```

### 4. `include/Inode.h` — 添加权限位常量（和同学C配合）

```cpp
// 在 Inode 类的 public 中添加权限位常量
static const unsigned int S_IRUSR = 0x0100;  // 所有者读
static const unsigned int S_IWUSR = 0x0080;  // 所有者写
static const unsigned int S_IXUSR = 0x0040;  // 所有者执行
static const unsigned int S_IRGRP = 0x0020;  // 组用户读
static const unsigned int S_IWGRP = 0x0010;  // 组用户写
static const unsigned int S_IXGRP = 0x0008;  // 组用户执行
static const unsigned int S_IROTH = 0x0004;  // 其他用户读
static const unsigned int S_IWOTH = 0x0002;  // 其他用户写
static const unsigned int S_IXOTH = 0x0001;  // 其他用户执行
```

---

## 补充：修改 `VFS/VFS.cpp` 中 `ls()` 函数（可选）

目前 `ls()` 可能只显示文件名。可以保持原来的 `ls` 行为不变，`dir` 命令专门显示详细信息。但也可以增强 `ls` 的输出：

在 `VFS.cpp` 的 `ls()` 中找到遍历目录项的逻辑，可以增加文件类型标识：
```
$ ls
[dir]  myfolder
[file] test.txt
[file] a.txt
```

这样 `ls` 和 `dir` 就有区分度了。

---

## 编译测试

```bash
# 切换到你的分支
git checkout feature-B-dir

# 修改代码后编译
make

# 运行测试
./user_fs
```

测试流程：
```
mount
format
mkdir testdir
touch a.txt
touch b.txt
dir
```

预期输出：
```
文件名                         物理地址      保护码        文件长度
------                         --------      ------        --------
testdir                        block:15      drwxr-xr-x    4096
a.txt                          block:16      -rw-r--r--    0
b.txt                          block:17      -rw-r--r--    0
```

再测试 `history`：
```
mount
format
ls
dir
history
```
预期输出显示最近执行的命令列表。

---

## 和同学的协作

- 你添加的权限位常量要和 **同学C** 保持一致
- `dir` 命令中的权限显示部分是依赖同学C实现的权限位的，如果同学C还没完成，你可以先用模拟值占位
- 建议你和同学C协商好 `i_mode` 中权限位的常量定义，使用完全相同的值