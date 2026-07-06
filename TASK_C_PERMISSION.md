# 🧑‍💻 同学C任务：文件权限管理 + chmod/chown

**分支：** `feature-C-permission`

## 任务目标
实现文件的 rwx 权限保护系统，支持 `chmod`、`chown` 命令，并在 open/read/write 操作中加入权限检查。

---

## 需要修改的文件

### 1. `include/Inode.h` — 添加权限位常量

在 Inode 类的 `public:` 中添加：
```cpp
// ====== 文件权限位（rwx） ======
// 所有者权限
static const unsigned int S_IRUSR = 0x0100;  // 所有者读   (0400)
static const unsigned int S_IWUSR = 0x0080;  // 所有者写   (0200)
static const unsigned int S_IXUSR = 0x0040;  // 所有者执行 (0100)

// 组用户权限
static const unsigned int S_IRGRP = 0x0020;  // 组用户读   (0040)
static const unsigned int S_IWGRP = 0x0010;  // 组用户写   (0020)
static const unsigned int S_IXGRP = 0x0008;  // 组用户执行 (0010)

// 其他用户权限
static const unsigned int S_IROTH = 0x0004;  // 其他用户读 (0004)
static const unsigned int S_IWOTH = 0x0002;  // 其他用户写 (0002)
static const unsigned int S_IXOTH = 0x0001;  // 其他用户执行 (0001)

// 常用权限组合（八进制表示法）
static const unsigned int S_IRWXU = 0x01C0;  // rwx --- --- (0700)
static const unsigned int S_IRWXG = 0x0038;  // --- rwx --- (0070)
static const unsigned int S_IRWXO = 0x0007;  // --- --- rwx (0007)

// 默认文件权限 0644 = rw-r--r--
static const unsigned int DEFAULT_FILE_MODE = 0x0184;  // S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH
// 默认目录权限 0755 = rwxr-xr-x
static const unsigned int DEFAULT_DIR_MODE  = 0x01ED;  // S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH
```

### 2. `VFS/VFS.cpp` — 修改创建文件/目录时设置默认权限

#### 2.1 `createFile()` 函数（约第118-120行）

原来：
```cpp
p_inode->i_mode = 0;
```
改为：
```cpp
p_inode->i_mode = Inode::DEFAULT_FILE_MODE;  // 默认 0644
```

#### 2.2 `mkDir()` 函数（约第304-305行）

原来：
```cpp
p_inode->i_mode = Inode::IFDIR;
```
改为：
```cpp
p_inode->i_mode = Inode::IFDIR | Inode::DEFAULT_DIR_MODE;  // 目录类型 + 默认 0755
```

### 3. `VFS/VFS.cpp` — 在 open/read/write 中加入权限检查

#### 3.1 在 `open()` 函数中（约第402-433行）

在打开文件后，添加权限检查逻辑。在 `Step4` 之前插入：

```cpp
// ====== 新增：权限检查 ======
User &u = VirtualProcess::Instance()->getUser();

// 只有 root(uid=0) 或文件所有者可以修改文件权限
if (mode & File::FWRITE) {
    // 检查写权限
    bool canWrite = false;
    if (u.u_uid == 0) {
        canWrite = true;  // root 可以写任何文件
    } else if (p_inodeOpenFile->i_uid == u.u_uid) {
        canWrite = (p_inodeOpenFile->i_mode & Inode::S_IWUSR) != 0;
    } else if (p_inodeOpenFile->i_gid == u.u_gid) {
        canWrite = (p_inodeOpenFile->i_mode & Inode::S_IWGRP) != 0;
    } else {
        canWrite = (p_inodeOpenFile->i_mode & Inode::S_IWOTH) != 0;
    }
    if (!canWrite) {
        Logcat::log("权限错误：没有写权限！");
        return ERROR_OPEN_ILLEGAL;
    }
}

if (mode & File::FREAD) {
    // 检查读权限
    bool canRead = false;
    if (u.u_uid == 0) {
        canRead = true;  // root 可以读任何文件
    } else if (p_inodeOpenFile->i_uid == u.u_uid) {
        canRead = (p_inodeOpenFile->i_mode & Inode::S_IRUSR) != 0;
    } else if (p_inodeOpenFile->i_gid == u.u_gid) {
        canRead = (p_inodeOpenFile->i_mode & Inode::S_IRGRP) != 0;
    } else {
        canRead = (p_inodeOpenFile->i_mode & Inode::S_IROTH) != 0;
    }
    if (!canRead) {
        Logcat::log("权限错误：没有读权限！");
        return ERROR_OPEN_ILLEGAL;
    }
}
// ====== 权限检查结束 ======
```

> **注意：** 这段代码要加在 `Step3` 之后，`Step4` 之前。检查时要用到用户信息（同学A做的登录系统），如果同学A还没完成，可以先假设 `u.u_uid` 来自 `VirtualProcess::Instance()->Getuid()` 。

### 4. `include/define.h` — 添加新指令枚举

在 `INSTRUCT` 枚举中添加：
```cpp
CHMOD,
CHOWN
```

在 `INST_NUM` 从 `16` 改为 `18`

在 `instructStr[]` 数组中添加：
```cpp
"chmod",
"chown"
```

### 5. `include/Shell.h` — 添加新命令方法

```cpp
void chmod();
void chown();
```

### 6. `Shell/Shell.cpp` — 实现 chmod 和 chown

#### 6.1 在 `parseCmd()` 的 switch 中添加：
```cpp
case CHMOD:
    chmod();
    break;
case CHOWN:
    chown();
    break;
```

#### 6.2 实现 `chmod()` 命令：

```cpp
void Shell::chmod()
{
    if (getParamAmount() != 3) {
        Logcat::log("用法: chmod 权限值 文件名");
        Logcat::log("示例: chmod 755 test.txt");
        return;
    }
    
    // 解析权限值（八进制字符串转数字）
    const char *permStr = getParam(1);
    const char *fileName = getParam(2);
    
    int permValue = 0;
    // 手动解析八进制字符串，如 "755" → 0755
    for (const char *p = permStr; *p != '\0'; p++) {
        permValue = permValue * 8 + (*p - '0');
    }
    
    // 将数字权限转换为 i_mode 中的权限位
    unsigned int newMode = 0;
    
    // 所有者权限
    if (permValue & 0400) newMode |= Inode::S_IRUSR;
    if (permValue & 0200) newMode |= Inode::S_IWUSR;
    if (permValue & 0100) newMode |= Inode::S_IXUSR;
    
    // 组用户权限
    if (permValue & 0040) newMode |= Inode::S_IRGRP;
    if (permValue & 0020) newMode |= Inode::S_IWGRP;
    if (permValue & 0010) newMode |= Inode::S_IXGRP;
    
    // 其他用户权限
    if (permValue & 0004) newMode |= Inode::S_IROTH;
    if (permValue & 0002) newMode |= Inode::S_IWOTH;
    if (permValue & 0001) newMode |= Inode::S_IXOTH;
    
    // 查找文件
    Path path(fileName);
    InodeId targetInodeId = bounded_VFS->getExt2()->locateInode(path);
    if (targetInodeId < 0) {
        Logcat::log("文件不存在！");
        return;
    }
    
    Inode *p_inode = Kernel::instance()->getInodeCache().getInodeByID(targetInodeId);
    
    // 保存文件类型标志
    unsigned int fileType = p_inode->i_mode & Inode::IFMT;
    
    // 设置新的权限
    p_inode->i_mode = fileType | newMode;
    p_inode->i_flag |= Inode::IUPD;
    
    Logcat::log("权限修改成功！");
}
```

#### 6.3 实现 `chown()` 命令：

```cpp
void Shell::chown()
{
    if (getParamAmount() != 3) {
        Logcat::log("用法: chown 用户名 文件名");
        return;
    }
    
    const char *userName = getParam(1);
    const char *fileName = getParam(2);
    
    // 只有 root 可以修改文件所有者
    User &currentUser = VirtualProcess::Instance()->getUser();
    if (currentUser.u_uid != 0) {
        Logcat::log("错误：只有 root 用户才能修改文件所有者！");
        return;
    }
    
    // 查找 /etc/passwd 获取用户的 uid
    Path passwdPath("/etc/passwd");
    FileFd fd = bounded_VFS->open(passwdPath, File::FREAD);
    if (fd < 0) {
        Logcat::log("用户文件读取失败！");
        return;
    }
    
    short targetUid = -1;
    short targetGid = -1;
    char line[256];
    char ch;
    int pos = 0;
    
    while (!bounded_VFS->eof(fd)) {
        memset(line, 0, sizeof(line));
        pos = 0;
        while (!bounded_VFS->eof(fd) && pos < sizeof(line)-1) {
            bounded_VFS->read(fd, (u_int8_t*)&ch, 1);
            if (ch == '\n') break;
            line[pos++] = ch;
        }
        line[pos] = '\0';
        
        char savedUser[32], savedPass[32];
        int savedUid, savedGid;
        if (sscanf(line, "%[^:]:%[^:]:%d:%d", savedUser, savedPass, &savedUid, &savedGid) == 4) {
            if (strcmp(savedUser, userName) == 0) {
                targetUid = savedUid;
                targetGid = savedGid;
                break;
            }
        }
    }
    bounded_VFS->close(fd);
    
    if (targetUid < 0) {
        Logcat::log("用户不存在！");
        return;
    }
    
    // 查找文件
    Path path(fileName);
    InodeId targetInodeId = bounded_VFS->getExt2()->locateInode(path);
    if (targetInodeId < 0) {
        Logcat::log("文件不存在！");
        return;
    }
    
    Inode *p_inode = Kernel::instance()->getInodeCache().getInodeByID(targetInodeId);
    p_inode->i_uid = targetUid;
    p_inode->i_gid = targetGid;
    p_inode->i_flag |= Inode::IUPD;
    
    Logcat::log("文件所有者修改成功！");
}
```

> **注意：** `chown` 依赖同学A的登录系统（需要从 `/etc/passwd` 读取用户信息）。

### 7. `include/VFS.h` — 暴露 `getExt2()` 方法（chown 需要用到）

```cpp
// 在 VFS 类中添加
Ext2* getExt2() { return p_ext2; }
```

### 8. `Ext2/DiskInode.cpp` — 完善 DiskInode 的构造

确保 DiskInode 到 Inode 的转换正确传递权限位：

```cpp
// 在 DiskInode(Inode inode) 构造函数中
DiskInode::DiskInode(Inode inode)
{
    d_mode = inode.i_mode;  // 确保这行正确传递了权限位
    d_nlink = inode.i_nlink;
    d_uid = inode.i_uid;
    d_gid = inode.i_gid;
    d_size = inode.i_size;
    for (int i = 0; i < 10; i++) {
        d_addr[i] = inode.i_addr[i];
    }
    d_atime = 0;
    d_mtime = 0;
}
```

---

## 编译测试

```bash
# 切换到你的分支
git checkout feature-C-permission

# 修改代码后编译
make

# 运行测试
./user_fs
```

测试流程1 — 权限设置：
```
mount
format
login root root
touch a.txt
ls           # 查看文件
chmod 777 a.txt
dir          # 查看权限是否改变
```

测试流程2 — 权限保护：
```
chmod 600 a.txt    # 只有所有者可以读写
# 退出用其他用户登录测试权限
```

测试流程3 — 修改所有者：
```
chown alice a.txt  # 将 a.txt 的所有者改为 alice
```

---

## 和同学的协作

- **依赖同学A**的登录系统：chown 需要读取 `/etc/passwd`，权限检查需要当前用户的 `u_uid`
- **与同学B共享**权限位常量定义（`Inode.h` 中的 `S_IRUSR` 等），确保两个人的值完全一致
- 如果同学A还没完成，你可以先用 `VirtualProcess::Instance()->Getuid()` 获取用户ID做测试

---

## 测试小技巧

| 八进制 | 对应的权限位 | 含义 |
|--------|------------|------|
| 777 | rwxrwxrwx | 所有人可读可写可执行 |
| 755 | rwxr-xr-x | 所有者全部权限，其他人只读+执行 |
| 644 | rw-r--r-- | 所有者读写，其他人只读 |
| 600 | rw------- | 仅所有者读写 |
| 000 | --------- | 所有人都无权限 |