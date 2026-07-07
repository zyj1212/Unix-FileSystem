# Unix FileSystem 测试用例

> 基于《要求.pdf》规范，覆盖所有命令及功能点的综合测试。

---

## 测试环境

```bash
./user_fs
mount
format
```

**前置条件**：每次测试前执行 `mount` → `format` 获得干净的初始文件系统。

---

## 1. 文件系统基础操作

### 1.1 mount — 挂载文件系统
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `mount` | 挂载成功，进入就绪状态 |
| 2 | `ls` | 显示根目录内容：bin, etc, dev, home |

### 1.2 format — 格式化文件系统
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `format` | 格式化成功，创建 SuperBlock、GDT、InodePool、系统目录 |
| 2 | `dir` | 显示根目录下 6 个条目（., .., bin, etc, dev, home），保护码为 drwxr-xr-x |
| 3 | `ls` | 同 dir |

### 1.3 unmount — 卸载文件系统
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `unmount` | 卸载成功 |
| 2 | `ls` | 提示文件系统未挂载 |

---

## 2. 目录操作（PDF 要求：dir、create、delete）

### 2.1 dir — 列目录（验证保护码、物理地址、文件长度）
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `dir` | 显示 4 列：文件名、物理地址、保护码、文件长度 |
| 2 | `dir /etc` | 显示 /etc 目录内容 |
| 3 | `dir /bin` | 显示 /bin 目录（含 . 和 ..） |

**验证点**：
- 文件名列宽 28 字符（对齐 `MAX_FILENAME_LEN`）
- 物理地址显示有效块号或 `-`
- 保护码为 Unix 标准 rwx 格式（如 `drwxr-xr-x`、`-rw-r--r--`）
- 文件长度为字节数

### 2.2 mkdir — 创建目录
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `mkdir testdir` | 创建成功 |
| 2 | `dir` | 显示 testdir，保护码 `drwxr-xr-x` |
| 3 | `mkdir testdir` | 报错：目录已存在 |

### 2.3 rmdir — 删除空目录
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `mkdir emptydir` | 创建成功 |
| 2 | `rmdir emptydir` | 删除成功 |
| 3 | `dir` | emptydir 已消失 |

### 2.4 rmdir — 删除非空目录
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `mkdir parent` | 创建成功 |
| 2 | `cd parent` | 切换成功 |
| 3 | `touch child.txt` | 在 parent 内创建文件 |
| 4 | `cd ..` | 返回上级 |
| 5 | `rmdir parent` | 成功（递归删除） |
| 6 | `dir` | parent 已消失 |

### 2.5 cd — 切换目录
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `cd /etc` | 切换到 /etc |
| 2 | `dir` | 显示 /etc 内容 |
| 3 | `cd ..` | 返回根目录 |
| 4 | `cd /nonexist` | 报错：路径不存在 |

### 2.6 ls — 列表显示
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `ls` | 显示当前目录所有文件和子目录 |

---

## 3. 文件操作（PDF 要求：create、delete、open、close、read、write）

### 3.1 touch — 创建普通文件
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch a.txt` | 创建成功 |
| 2 | `dir` | 显示 a.txt，保护码 `-rw-r--r--`（0644），文件长度 0 |
| 3 | `touch a.txt` | 报错：文件已存在 |

### 3.2 rm — 删除普通文件
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch b.txt` | 创建成功 |
| 2 | `rm b.txt` | 删除成功 |
| 3 | `dir` | b.txt 已消失 |
| 4 | `rm nonexist.txt` | 报错：文件不存在 |

### 3.3 store — 导入文件（写入）
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch data.txt` | 创建空文件 |
| 2 | `store "Hello World" data.txt` | 写入成功 |
| 3 | `dir` | data.txt 文件长度 = 11 |

### 3.4 cat — 读取文件内容
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch poem.txt` | 创建文件 |
| 2 | `store "床前明月光" poem.txt` | 写入内容 |
| 3 | `cat poem.txt` | 输出 "床前明月光" |

### 3.5 withdraw — 导出文件
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch secret.txt` | 创建文件 |
| 2 | `store "password123" secret.txt` | 写入内容 |
| 3 | `withdraw secret.txt` | 导出到宿主机，文件内容 "password123" |

### 3.6 大文件读写（验证混合索引）
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch big.txt` | 创建文件 |
| 2 | `store "AAAA...(>6个块)" big.txt` | 触发间接索引分配 |
| 3 | `cat big.txt` | 完整读出写入内容 |
| 4 | `dir` | 验证文件长度正确 |

---

## 4. 用户管理（PDF 要求：login 用户登录）

### 4.1 whoami — 未登录状态
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `whoami` | 显示 "guest（未登录）" |

### 4.2 login — 登录系统
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 登录成功，提示符变为 `root@user_fs:/$` |
| 2 | `whoami` | 显示 "root (uid=0)" |

### 4.3 login — 错误密码
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root wrongpass` | 报错：用户名或密码错误 |

### 4.4 useradd — 创建用户（root 权限）
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 以 root 登录 |
| 2 | `useradd alice pass123` | 创建成功 |
| 3 | `logout` | 退出登录 |
| 4 | `login alice pass123` | 以 alice 登录成功 |
| 5 | `whoami` | 显示 "alice (uid=1001)" |

### 4.5 useradd — 自动递增 uid
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 以 root 登录 |
| 2 | `useradd alice pass1` | uid=1001 |
| 3 | `useradd bob pass2` | uid=1002（自动递增） |
| 4 | `useradd charlie pass3` | uid=1003 |
| 5 | `logout` | 退出 |
| 6 | `login bob pass2` | 以 bob 登录成功 |

### 4.6 useradd — 非 root 无权创建
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `useradd testuser pass` | 报错：只有 root 才能创建用户（guest uid=-1，不是 root） |

### 4.7 useradd — 重复用户名
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 以 root 登录 |
| 2 | `useradd alice pass1` | 创建成功 |
| 3 | `useradd alice pass2` | 报错：用户名已存在 |

### 4.8 logout — 退出登录
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 登录成功 |
| 2 | `logout` | 退出成功，提示符恢复为 `guest$` |
| 3 | `whoami` | 显示 "guest（未登录）" |

---

## 5. 权限管理（PDF 要求：读写保护、rwx 权限）

### 5.1 chmod — 修改文件权限
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | 以 root 登录 |
| 2 | `touch test.txt` | 创建文件（默认 0644） |
| 3 | `dir` | 保护码 `-rw-r--r--` |
| 4 | `chmod 777 test.txt` | 修改成功 |
| 5 | `dir` | 保护码 `-rwxrwxrwx` |
| 6 | `chmod 000 test.txt` | 修改为无权限 |
| 7 | `dir` | 保护码 `----------` |
| 8 | `chmod 755 test.txt` | 恢复权限 |
| 9 | `dir` | 保护码 `-rwxr-xr-x` |

### 5.2 chmod — 非所有者不能修改
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | root 登录 |
| 2 | `touch rootfile.txt` | root 创建文件 |
| 3 | `logout` | 退出 |
| 4 | `login alice pass123` | alice 登录 |
| 5 | `chmod 777 rootfile.txt` | 报错：只有文件所有者或 root 才能修改 |

### 5.3 chmod — 未登录不能修改
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `chmod 777 test.txt` | 报错：请先登录（guest 无法通过检查） |

### 5.4 chown — 修改文件所有者（root 权限）
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | root 登录 |
| 2 | `useradd alice pass123` | 创建 alice |
| 3 | `touch shared.txt` | root 创建文件 |
| 4 | `chown alice shared.txt` | 修改成功 |
| 5 | `dir` | 所有者变为 alice |

### 5.5 chown — 非 root 无权修改
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login alice pass123` | alice 登录 |
| 2 | `touch myfile.txt` | alice 创建文件 |
| 3 | `chown root myfile.txt` | 报错：只有 root 才能修改 |

### 5.6 权限保护 — 读保护
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | root 登录 |
| 2 | `useradd alice pass123` | 创建 alice |
| 3 | `touch secret.txt` | root 创建文件（0644） |
| 4 | `store "classified" secret.txt` | 写入内容 |
| 5 | `chmod 600 secret.txt` | 仅所有者可读写 |
| 6 | `logout` | 退出 |
| 7 | `login alice pass123` | alice 登录 |
| 8 | `cat secret.txt` | 报错：没有读权限 |
| 9 | `logout` | 退出 |
| 10 | `login root root` | root 登录 |
| 11 | `cat secret.txt` | 成功读取（root 可绕过权限） |

### 5.7 权限保护 — 写保护
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | root 登录 |
| 2 | `touch readonly.txt` | 创建文件 |
| 3 | `chmod 444 readonly.txt` | 所有人只读 |
| 4 | `store "data" readonly.txt` | 报错：没有写权限 |

### 5.8 root 绕过权限
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `login root root` | root 登录 |
| 2 | `touch anyfile.txt` | 创建文件 |
| 3 | `chmod 000 anyfile.txt` | 无任何权限 |
| 4 | `cat anyfile.txt` | root 仍可读（uid=0 绕过） |
| 5 | `store "root writes" anyfile.txt` | root 仍可写 |

---

## 6. 其他命令

### 6.1 history — 命令历史
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `touch a.txt` | 创建文件 |
| 2 | `ls` | 列出文件 |
| 3 | `dir` | 列目录 |
| 4 | `history` | 显示最近命令记录 |

### 6.2 help — 帮助信息
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `help` | 显示所有可用命令列表 |

### 6.3 version — 版本信息
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `version` | 显示文件系统版本信息 |

### 6.4 clear — 清屏
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `clear` | 清空终端屏幕 |

### 6.5 exit — 退出程序
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `exit` | 退出 user_fs 程序 |

### 6.6 命令自动补全
| 步骤 | 命令 | 预期结果 |
|------|------|----------|
| 1 | `to` + Tab | 自动补全为 `touch` |
| 2 | `mk` + Tab | 补全为 `mkdir` |
| 3 | `hist` + Tab | 补全为 `history` |

---

## 7. 综合场景测试

### 7.1 多用户协作场景
```
mount → format
login root root
useradd alice alice123
useradd bob bob123
logout

login alice alice123
mkdir alice_work
cd alice_work
touch report.txt
store "Alice's report" report.txt
chmod 644 report.txt     # 允许 bob 读取
cd ..
logout

login bob bob123
cat /alice_work/report.txt   # 可读（644 other r）
store "Bob's edit" /alice_work/report.txt  # 报错（非所有者、无写权限）
logout
```

### 7.2 混合索引大文件场景
```
mount → format
login root root
touch giant.txt
store "AAAA...(50KB+)" giant.txt    # 触发单间接索引
dir                                   # 验证文件大小
cat giant.txt | wc -c                # 验证内容完整性
rm giant.txt                          # 验证大文件正确删除（包括间接块）
```

### 7.3 深层目录嵌套场景
```
mount → format
login root root
mkdir a
cd a
mkdir b
cd b
mkdir c
cd c
touch deep.txt
store "deep file" deep.txt
cd /
cat /a/b/c/deep.txt                  # 验证深层路径访问
```
