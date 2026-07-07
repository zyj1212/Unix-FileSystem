# 🧑‍💻 同学A任务：用户登录系统

**分支：** `feature-A-login`

## 任务目标
实现多用户登录系统，支持 `login`、`useradd`、`whoami` 命令。

---

## 需要修改的文件

### 1. `include/define.h` — 添加新指令枚举

在 `INSTRUCT` 枚举中添加：
```cpp
LOGIN,
USERADD,
WHOAMI
```

在 `INST_NUM` 从 `16` 改为 `19`

在 `instructStr[]` 数组中添加：
```cpp
"login",
"useradd",
"whoami"
```

### 2. `include/User.h` — 扩展用户类

将原来**静态写死的用户ID**改为动态的：
```cpp
class User
{
public:
    Inode *u_cdir;
    Inode *u_pdir;
    DirectoryEntry u_dent;
    InodeId curDirInodeId;

    // ====== 新增内容 ======
    char username[32];    // 当前登录用户名
    char password[32];    // 当前登录用户密码
    short u_uid;          // 有效用户ID（原来是静态常量）
    short u_gid;          // 有效组ID
    bool isLoggedIn;      // 是否已登录
    // =====================

    OpenFiles u_ofiles;
    IOParameter u_IOParam;

    // 构造函数初始化
    User();
};
```

### 3. `VirtualProcess/User.cpp` — 实现用户构造函数

```cpp
#include "../include/User.h"
#include <cstring>

User::User()
{
    u_cdir = nullptr;
    u_pdir = nullptr;
    curDirInodeId = 0;
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    u_uid = 0;
    u_gid = 0;
    isLoggedIn = false;
}
```

### 4. `include/Shell.h` — 添加新命令方法

在 Shell 类中添加：
```cpp
void login();
void useradd();
void whoami();
```

### 5. `Shell/Shell.cpp` — 实现新命令 + 命令提示符优化

#### 5.1 在 `parseCmd()` 的 switch 中添加新 case：
```cpp
case LOGIN:
    login();
    break;
case USERADD:
    useradd();
    break;
case WHOAMI:
    whoami();
    break;
```

#### 5.2 修改 `readUserInput()` 中的提示符：

原来只显示 `$`，改为：
```cpp
// 显示命令提示符（带用户名）
User &u = VirtualProcess::Instance()->getUser();
if (u.isLoggedIn) {
    printf("%s@user_fs:%s$ ", u.username, getCurrentDirName());
} else {
    printf("(guest)$ ");
}
```

其中 `getCurrentDirName()` 需要在 Shell 类中新增一个方法，获取当前目录的简化路径。

#### 5.3 实现 `login()` 命令：

```cpp
void Shell::login()
{
    if (getParamAmount() < 3) {
        Logcat::log("用法: login 用户名 密码");
        return;
    }
    
    User &u = VirtualProcess::Instance()->getUser();
    const char *inputUser = getParam(1);
    const char *inputPass = getParam(2);
    
    // 打开用户文件 /etc/passwd
    Path passwdPath("/etc/passwd");
    FileFd fd = bounded_VFS->open(passwdPath, File::FREAD);
    if (fd < 0) {
        Logcat::log("用户文件不存在！请先执行 format");
        return;
    }
    
    // 读取用户文件，逐行匹配
    char line[256];
    int offset = 0;
    bool found = false;
    
    while (!bounded_VFS->eof(fd)) {
        memset(line, 0, sizeof(line));
        // 逐字节读取直到换行
        char ch;
        int pos = 0;
        while (!bounded_VFS->eof(fd) && pos < sizeof(line)-1) {
            bounded_VFS->read(fd, (u_int8_t*)&ch, 1);
            if (ch == '\n') break;
            line[pos++] = ch;
        }
        line[pos] = '\0';
        
        // 解析行：用户名:密码:uid:gid
        char savedUser[32], savedPass[32];
        int savedUid, savedGid;
        if (sscanf(line, "%[^:]:%[^:]:%d:%d", savedUser, savedPass, &savedUid, &savedGid) == 4) {
            if (strcmp(savedUser, inputUser) == 0 && strcmp(savedPass, inputPass) == 0) {
                found = true;
                // 设置当前用户
                strcpy(u.username, savedUser);
                strcpy(u.password, savedPass);
                u.u_uid = savedUid;
                u.u_gid = savedGid;
                u.isLoggedIn = true;
                Logcat::log("登录成功！");
                break;
            }
        }
    }
    
    bounded_VFS->close(fd);
    
    if (!found) {
        Logcat::log("用户名或密码错误！");
    }
}
```

#### 5.4 实现 `useradd()` 命令：

```cpp
void Shell::useradd()
{
    if (getParamAmount() < 3) {
        Logcat::log("用法: useradd 用户名 密码");
        return;
    }
    
    const char *newUser = getParam(1);
    const char *newPass = getParam(2);
    
    // 只有 root 可以创建用户
    User &u = VirtualProcess::Instance()->getUser();
    if (u.u_uid != 0) {
        Logcat::log("错误：只有 root 用户才能创建新用户！");
        return;
    }
    
    // 打开 /etc/passwd 追加
    Path passwdPath("/etc/passwd");
    FileFd fd = bounded_VFS->open(passwdPath, File::FWRITE);
    if (fd < 0) {
        Logcat::log("打开用户文件失败");
        return;
    }
    
    // 移动到文件末尾
    // （这里需要 VFS 支持 lseek，可以用 seek 或直接计算）
    // 追加新用户行
    char line[128];
    static int nextUid = 1001; // 从1001开始分配
    snprintf(line, sizeof(line), "%s:%s:%d:%d\n", newUser, newPass, nextUid++, 100);
    
    bounded_VFS->write(fd, (u_int8_t*)line, strlen(line));
    bounded_VFS->close(fd);
    
    Logcat::log("用户创建成功！");
}
```

> **注意：** `useradd` 追加写入需要 VFS 支持 lseek 到文件尾。如果当前 VFS 没有 lseek，简单做法是在 `open` 时用 `FAPPEND` 模式打开，或者直接读取全部内容、追加再写回。

#### 5.5 实现 `whoami()` 命令：

```cpp
void Shell::whoami()
{
    User &u = VirtualProcess::Instance()->getUser();
    if (u.isLoggedIn) {
        Logcat::log("当前用户：", u.username);
    } else {
        Logcat::log("当前用户：guest（未登录）");
    }
}
```

### 6. `Ext2/Ext2.cpp` — 格式化时创建用户文件

在 `format()` 函数中，创建根目录后，添加创建 `/etc/passwd` 文件的逻辑：

```cpp
// 格式化时创建 /etc 目录和 passwd 文件
mkdir("/etc");
// 创建 passwd 文件并写入默认 root 用户
createFile("/etc/passwd");
// 打开并写入 "root:root:0:0\n"
Path p("/etc/passwd");
FileFd fd = open(p, File::FWRITE);
const char *rootLine = "root:root:0:0\n";
write(fd, (u_int8_t*)rootLine, strlen(rootLine));
close(fd);
```

---

## 编译测试

```bash
# 切换到你的分支
git checkout feature-A-login

# 修改代码后编译
make

# 运行测试
./user_fs
```

测试流程：
```
mount
format
login root root    # 用 root 登录
whoami             # 显示当前用户
useradd alice 123  # 创建新用户
logout             # 退出（可选）
login alice 123    # 用新用户登录
```

---

## 和同学的协作

- 你的用户系统会被 **同学C（权限管理）** 依赖，他需要在 open/read/write 时检查用户权限
- 如果你提前完成了，可以帮同学C做权限检查的联调