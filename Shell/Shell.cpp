#include "../include/Shell.h"
#include "../include/VirtualProcess.h"
#include "../include/Kernel.h"

void Shell::help()
{

    system("cat help");
}

int Shell::readUserInput()
{
    Logcat::log("建议先输入help指令，查看使用说明");
    history_count = 0;
    while (true)
    {
        //Step0:
        //显示命令提示符（支持用户登录提示）

        User &curU = VirtualProcess::Instance()->getUser();
        if (curU.isLoggedIn) {
            printf("%s@user_fs:/$ ", curU.username);
        } else {
            printf("guest$ ");
        }

        //Step1:获取用户输入放到缓冲区

        std::cin.getline(tty_buffer, MAX_CMD_LEN, '\n');

        // 如果输入为空，跳过
        if (strlen(tty_buffer) == 0)
        {
            continue;
        }

        // 记录历史命令
        if (history_count < HISTORY_MAX)
        {
            strcpy(history_buf[history_count], tty_buffer);
            history_count++;
        }
        else
        {
            // 循环覆盖最旧的记录
            for (int i = 1; i < HISTORY_MAX; i++)
            {
                strcpy(history_buf[i - 1], history_buf[i]);
            }
            strcpy(history_buf[HISTORY_MAX - 1], tty_buffer);
        }

        //Step2:先将tab转换为space
        for (char *checker = strrchr(tty_buffer, '\t'); checker != NULL; checker = strrchr(checker, '\t'))
        {
            *checker = ' ';
        }

        //Step3:以空格、tab为界，分解命令参数，存到Shell::split_cmd中
        char *dupl_tty_buffer = strdup(tty_buffer);
        /**
         * NOTE strdup创建的字符串是在堆上的，需要自己delete释放
         *@comment:这里拷贝一份tty_buffer的副本，因为后面用strtok函数的时候，会改变参数的字符串
         *当然也不是非要调用strtok，但是方便啊
         * 
         */

        //splitCmd先清空一下
        memset(split_cmd, 0x0, sizeof(split_cmd));
        int cmd_param_seq = 0;
        for (char *p = strtok(dupl_tty_buffer, " "); p != nullptr; p = strtok(NULL, " "), cmd_param_seq++)
        {
            strcpy(split_cmd[cmd_param_seq], p);
        }
        param_num = cmd_param_seq;
#ifdef IS_DEBUG
        for (int i = 0; i < param_num; i++)
        {
            std::cout << "看一下刚输入的参数：" << split_cmd[i] << ' ';
        }
        std::cout << std::endl;
#endif
        //TODO

        //Step4:解析执行指令
        parseCmd();
        delete dupl_tty_buffer;
        fflush(stdin);
    }
}

void Shell::parseCmd()
{
    // 先尝试精确匹配
    INSTRUCT inst = getInstType();
    
    if (inst == ERROR_INST)
    {
        // 精确匹配失败，尝试前缀补全匹配
        char *instStr = getInstStr();
        int match_count = 0;
        int match_index = -1;
        
        for (int i = 1; i < INST_NUM; i++)
        {
            if (strncmp(instructStr[i], instStr, strlen(instStr)) == 0)
            {
                match_count++;
                match_index = i;
            }
        }
        
        if (match_count == 1)
        {
            // 唯一匹配，自动补全并执行
            printf("（自动补全: %s）\n", instructStr[match_index]);
            inst = INSTRUCT(match_index - 1);
        }
        else if (match_count > 1)
        {
            // 多个匹配，列出所有可能的命令
            printf("（未匹配命令 \"%s\"，您是否想输入：", instStr);
            for (int i = 1; i < INST_NUM; i++)
            {
                if (strncmp(instructStr[i], instStr, strlen(instStr)) == 0)
                {
                    printf("%s ", instructStr[i]);
                }
            }
            printf("）\n");
            return;
        }
        else
        {
            Logcat::log("CMD NOT SUPPORTED!\n");
            return;
        }
    }

    switch (inst)
    {
    case MOUNT:
        mount(); //OK
        break;
    case UNMOUNT:
        unmount(); //OK
        break;
    case FORMAT:
        format(); //OK
        break;
    case CD:
        cd(); //OK
        break;
    case LS:
        ls(); //OK
        break;
    case RM:
        rm(); //OK
        break;
    case RMDIR:
        rmdir(); //OK
        break;
    case MKDIR:
        mkdir(); //OK
        break;
    case TOUCH:
        touch(); //OK
        break;
    case CLEAR:
        clear(); //OK
        break;
    case HELP:
        help(); //OK
        break;
    case EXIT:
        mexit(); //OK
        break;
    case VERSION:
        version(); //OK
        break;
    case STORE:
        store(); //OK
        break;
    case WITHDRAW:
        withdraw(); //OKKK
        break;
    case LOGIN:
        login();
        break;
    case USERADD:
        useradd();
        break;
    case WHOAMI:
        whoami();
        break;
    case CHMOD:
        chmod();
        break;
    case CHOWN:
        chown();
        break;
    case DIR:
        dir();
        break;
    case HISTORY:
        history();
        break;
    case LOGOUT:
        logout();
        break;
    default:
        Logcat::log("CMD NOT SUPPORTED!\n");
        break;
    }
}

/**
 * @comment:实际上是做字符串到枚举类型的转化，为了switch case
 */
INSTRUCT Shell::getInstType()
{
    char *instStr = getInstStr();
#ifdef IS_DEBUG
    Logcat::log(TAG, "命令行命令字为:");
    Logcat::log(TAG, instStr);

#endif
    //为什么从1开始
    for (int i = 1; i < INST_NUM; i++)
    {
        //这里要加感叹号，注意strcmp在相等时返回的是0
        if (!strcmp(instructStr[i], instStr))
        {

#ifdef IS_DEBUG
            //std::cout<<INSTRUCT(i)<<std::endl;
#endif
            return INSTRUCT(i - 1);
        }
    }
    return ERROR_INST;
}

/**
 * @comment:命令缓冲区→命令参数字符数组→第一个参数得到命令字符串
 * 此函数的功能就是读出第一个字符串，亦即InstStr
 */
char *Shell::getInstStr()
{
    return split_cmd[0];
    //很简单，数组首个就是命令关键字
}

/**
 * @comment:这个是getInstStr更通用的情况
 */
char *Shell::getParam(int i)
{
    return split_cmd[i];
}

/**
 * 获得参数的个数
 */
int Shell::getParamAmount()
{
    for (int i = 0; i < MAX_PARAM_NUM; i++)
    {
        if (!strcmp(split_cmd[i], ""))
        {
            return i;
        }
    }
    return MAX_PARAM_NUM;
}

void Shell::mount()
{
    Logcat::devlog(TAG, "MOUNT EXEC");
    /**
     * 装载磁盘的最上层命令调用函数：
     * 硬盘装载的步骤：
     * ①内存inodeCache初始化
     * ②DiskDriver打开虚拟磁盘img，mmap，进入就绪状态
     * ③装载SuperBlock到VFS的SuperBlock缓存
     * 
     *  */
    bounded_VFS->mount();
}

void Shell::unmount()
{
    bounded_VFS->unmount();
    Logcat::devlog(TAG, "unmount EXEC");
}

/**
 * 对装载的磁盘镜像做格式化
 */
void Shell::format()
{

    if (bounded_VFS->isMounted())
    {
        bounded_VFS->format();
        Logcat::devlog(TAG, "format EXEC");
    }
    else
    {
        Logcat::log(TAG, "ERROR,DISK NOT MOUNTED!");
    }
}
void Shell::mkdir()
{
    if (getParamAmount() == 2)
    {
        if (bounded_VFS->mkDir(getParam(1)) < 0)
        {
            Logcat::log("ERROR!存在同名目录，创建失败！");
        }
    }
    else
    {
        Logcat::log("ERROR！MKDIR参数个数错误！");
    }
    Logcat::devlog(TAG, "mkdir EXEC");
}
void Shell::cat()
{
    Logcat::devlog(TAG, "cat EXEC");
    if (getParamAmount() != 2) {
        Logcat::log("用法: cat 文件名");
        return;
    }

    const char *fileName = getParam(1);
    Path path(fileName);
    FileFd fd = bounded_VFS->open(path, File::FREAD);
    if (fd < 0) {
        Logcat::log("文件打开失败！");
        return;
    }

    char buf[4096];
    int totalRead = 0;
    while (!bounded_VFS->eof(fd)) {
        int readSize = bounded_VFS->read(fd, (u_int8_t*)buf + totalRead, sizeof(buf) - totalRead - 1);
        if (readSize <= 0) break;
        totalRead += readSize;
    }
    buf[totalRead] = '\0';
    printf("%s", buf);

    bounded_VFS->close(fd);
}
void Shell::touch()
{
    if (getParamAmount() != 2)
    {
        Logcat::log("ERROR!参数个数错误！");
        return;
    }
    else
    {
        if (0 > bounded_VFS->createFile(getParam(1)))
        {
            Logcat::log("ERROR!存在同名文件，创建失败！");
        }
    }

    Logcat::devlog(TAG, "touch EXEC");
}

/**
 * 删除文件
 */
void Shell::rm()
{
    if (getParamAmount() != 2)
    {
        Logcat::log("ERROR!参数个数错误！");
        return;
    }
    else
    {
        if (0 > bounded_VFS->deleteFile(getParam(1)))
        {
            Logcat::log("删除文件失败！");
        }
    }

    Logcat::devlog(TAG, "rm EXEC");
}

/**
 * 删除目录以及其下的所有文件
 */
void Shell::rmdir()
{
    if (getParamAmount() != 2)
    {
        Logcat::log("ERROR!参数个数错误！");
        return;
    }
    else
    {
        if (0 > bounded_VFS->deleteDir(getParam(1)))
        {
            Logcat::log("删除，目录失败！");
        }
    }

    Logcat::devlog(TAG, "rmdir EXEC");
}

void Shell::version()
{
    system("cat version");
    Logcat::devlog(TAG, "version EXEC");
}
void Shell::man()
{
    Logcat::log(TAG, "欢迎求助那个男人");

    Logcat::devlog(TAG, "man EXEC");
}
void Shell::mexit()
{
    if (bounded_VFS->isMounted())
    {
        bounded_VFS->unmount();
    }
    Logcat::devlog(TAG, "exit EXEC");
    Logcat::log("程序结束！");
    exit(OK);
}

/**
 * 用户指令：更改当前目录
 */
void Shell::cd()
{

    //cd必须带参数
    if (getParamAmount() != 2)
    {
        Logcat::log("Error!cd命令参数个数错误！");
    }
    else
    {
        bounded_VFS->cd(getParam(1));
    }
}

/**
 * ls函数可以带参数，也可以不带（curDir）
 */
void Shell::ls()
{
    if (!strcmp(getParam(1), ""))
    {
        //不带参数的ls，以curDir为默认参数
        bounded_VFS->ls(VirtualProcess::Instance()->getUser().curDirInodeId);
    }
    else
    {
        bounded_VFS->ls(getParam(1)); //getParam(1)获得的是ls后面跟的目录名（可能是相对的也可能是绝对的）
    }
}

/**
 * 将外部文件考入虚拟磁盘.带两个命令参数
 * Usage: store [src path] [des filename]
 */
void Shell::store()
{
    if (getParamAmount() == 3)
    {
        InodeId desInodeId;
        //STORE的步骤
        //Step1：创建文件（如果有同名的返回失败）
        desInodeId = bounded_VFS->createFile(getParam(2));
        if (desInodeId < 0)
        {
            Logcat::log("ERROR!目标文件名已存在！");
            return;
        }
        //Step2：打开文件
        Path desPath(getParam(2));
        FileFd fd_des = bounded_VFS->open(desPath, File::FWRITE);
        //Step3：写入文件
        FILE *fd_src = fopen(getParam(1), "rb");
        if (fd_src == NULL)
        {
            Logcat::log("源文件打开失败！");
            return;
        }
        DiskBlock tempBuf;
        int file_size = 0;
        while (!feof(fd_src))
        {
            //int blkCount = 0;
            int readsize = fread(&tempBuf, 1, DISK_BLOCK_SIZE, fd_src);
            file_size += readsize;
            bounded_VFS->write(fd_des, (u_int8_t *)&tempBuf, readsize);
        }
        Inode *p_desInode = Kernel::instance()->getInodeCache().getInodeByID(desInodeId);
        p_desInode->i_size = file_size; //TODO这一块不太好，封装性差了点

        //Step4：关闭文件
        fclose(fd_src);
        bounded_VFS->close(fd_des);
    }
    else
    {
        Logcat::log("ERROR!store命令参数个数错误");
    }
}

/**
 * 将文件从虚拟磁盘中拷出
 * Usage: withdraw [src filename] [des outer_path]
 */
void Shell::withdraw()
{
    if (getParamAmount() == 3)
    {
        InodeId desInodeId;
        //WITHDRAW的步骤
        //Step1：创建文件（如果有同名的返回失败）
        FILE *fd_des = fopen(getParam(2), "wb");
        if (fd_des == NULL)
        {
            Logcat::log("目的文件创建失败！");
            return;
        }

        //Step2：打开文件
        Path srcPath(getParam(1));
        FileFd fd_src = bounded_VFS->open(srcPath, File::FREAD);
        if (fd_src < 0)
        {
            Logcat::log("源文件打开失败！");
            return;
        }
        //Step3：写入文件
        DiskBlock tempBuf;
        while (!bounded_VFS->eof(fd_src))
        {
            //int blkCount = 0;
            int writesize = bounded_VFS->read(fd_src, (u_int8_t *)&tempBuf, DISK_BLOCK_SIZE);
            fwrite(&tempBuf, 1, writesize, fd_des);
        }
        //Step4：关闭文件
        fclose(fd_des);
        bounded_VFS->close(fd_src);
    }
    else
    {
        Logcat::log("ERROR!store命令参数个数错误");
    }
}

void Shell::login()
{
    if (getParamAmount() != 3) {
        Logcat::log("用法: login 用户名 密码");
        return;
    }

    User &u = VirtualProcess::Instance()->getUser();

    // 如果已经登录，提示先退出
    if (u.isLoggedIn) {
        Logcat::log("当前已登录，请先退出当前用户");
        return;
    }

    const char *inputUser = getParam(1);
    const char *inputPass = getParam(2);

    // 打开 /etc/passwd 文件（绝对路径）
    Path passwdPath("/etc/passwd");
    FileFd fd = bounded_VFS->open(passwdPath, File::FREAD);
    if (fd < 0) {
        Logcat::log("用户文件不存在！请先执行 format");
        return;
    }

    // 逐行读取用户文件
    char line[256];
    bool found = false;

    while (!bounded_VFS->eof(fd)) {
        memset(line, 0, sizeof(line));
        char ch;
        int pos = 0;
        while (!bounded_VFS->eof(fd) && pos < 255) {
            bounded_VFS->read(fd, (u_int8_t*)&ch, 1);
            if (ch == '\n') break;
            line[pos++] = ch;
        }
        line[pos] = '\0';

        if (strlen(line) == 0) continue;

        // 解析：用户名:密码:uid:gid
        char savedUser[32], savedPass[32];
        int savedUid, savedGid;
        if (sscanf(line, "%[^:]:%[^:]:%d:%d",
                   savedUser, savedPass, &savedUid, &savedGid) == 4) {
            if (strcmp(savedUser, inputUser) == 0 &&
                strcmp(savedPass, inputPass) == 0) {
                found = true;
                strcpy(u.username, savedUser);
                strcpy(u.password, savedPass);
                u.u_uid = (short)savedUid;
                u.u_gid = (short)savedGid;
                u.isLoggedIn = true;
                Logcat::log("登录成功！");
                break;
            }
        }
    }

    bounded_VFS->close(fd);

    if (!found) {
        Logcat::log("登录失败：用户名或密码错误！");
    }
}

void Shell::useradd()
{
    if (getParamAmount() != 3) {
        Logcat::log("用法: useradd 用户名 密码");
        return;
    }

    const char *newUser = getParam(1);
    const char *newPass = getParam(2);

    User &u = VirtualProcess::Instance()->getUser();
    if (!u.isLoggedIn || u.u_uid != 0) {
        Logcat::log("错误：只有 root 用户才能创建新用户！");
        return;
    }

    // Step1: 读取 /etc/passwd 全部内容到内存
    Path passwdPath("/etc/passwd");
    FileFd fd = bounded_VFS->open(passwdPath, File::FREAD);
    if (fd < 0) {
        Logcat::log("用户文件读取失败！请先执行 format 创建用户文件");
        return;
    }

    char fileContent[4096];
    memset(fileContent, 0, sizeof(fileContent));
    int totalRead = 0;
    char tempBuf[512];
    while (!bounded_VFS->eof(fd)) {
        int readSize = bounded_VFS->read(fd, (u_int8_t*)tempBuf, 512);
        if (readSize > 0 && totalRead + readSize < 4000) {
            memcpy(fileContent + totalRead, tempBuf, readSize);
            totalRead += readSize;
        }
    }
    bounded_VFS->close(fd);
    fileContent[totalRead] = '\0';

    // Step2: 检查用户名是否已存在
    char contentCopy[4096];
    strcpy(contentCopy, fileContent);
    char *line = strtok(contentCopy, "\n");
    while (line != NULL) {
        char existingUser[32];
        if (sscanf(line, "%[^:]", existingUser) == 1) {
            if (strcmp(existingUser, newUser) == 0) {
                Logcat::log("错误：用户名已存在！");
                return;
            }
        }
        line = strtok(NULL, "\n");
    }

    // Step3: 追加新用户行
    char newLine[128];
    // uid 从 1001 开始分配
    snprintf(newLine, sizeof(newLine), "%s:%s:1001:100\n", newUser, newPass);

    // Step4: 覆盖写回
    fd = bounded_VFS->open(passwdPath, File::FWRITE);
    if (fd < 0) {
        Logcat::log("用户文件打开失败！");
        return;
    }

    bounded_VFS->write(fd, (u_int8_t*)fileContent, strlen(fileContent));
    bounded_VFS->write(fd, (u_int8_t*)newLine, strlen(newLine));
    bounded_VFS->close(fd);

    Logcat::log("用户创建成功！");
}

void Shell::whoami()
{
    User &u = VirtualProcess::Instance()->getUser();
    if (u.isLoggedIn) {
        printf("当前用户：%s (uid=%d)\n", u.username, u.u_uid);
    } else {
        Logcat::log("当前用户：guest（未登录）");
    }
}

void Shell::clear()
{
    system("clear");
}

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
        while (!bounded_VFS->eof(fd) && pos < (int)sizeof(line) - 1) {
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

void Shell::dir()
{
    if (!strcmp(getParam(1), ""))
    {
        //不带参数的dir，以curDir为默认参数
        bounded_VFS->dir(VirtualProcess::Instance()->getUser().curDirInodeId);
    }
    else
    {
        bounded_VFS->dir(getParam(1));
    }
}

void Shell::history()
{
    printf("  History:\n");
    for (int i = 0; i < history_count; i++)
    {
        printf("%3d  %s\n", i + 1, history_buf[i]);
    }
}

void Shell::logout()
{
    User &u = VirtualProcess::Instance()->getUser();
    if (!u.isLoggedIn) {
        Logcat::log("当前未登录");
        return;
    }

    memset(u.username, 0, sizeof(u.username));
    memset(u.password, 0, sizeof(u.password));
    u.u_uid = 0;
    u.u_gid = 0;
    u.isLoggedIn = false;

    Logcat::log("已退出登录");
}

Shell::Shell()
{
    TAG = strdup("Shell");
    history_count = 0;
}
Shell::~Shell()
{
    delete TAG;
}

void Shell::setVFS(VFS *vfs)
{
    bounded_VFS = vfs;
}

//隐式调用

// void Shell::creat()
// {
//     Logcat::devlog(TAG, "creat EXEC");
// }

/**
 * 临时的，不应该是一个用户接口
 */
// void Shell::open()
// {
//     Path path(getParam(1));
//     bounded_VFS->open(path, File::FREAD);
//     Logcat::log(TAG, "open EXEC");
// }
/**
 * 临时的，不应该是一个用户接口
 */
// void Shell::close()
// {
//     Logcat::log(TAG, "close EXEC");
// }

/**
 * 临时的，不应该是一个用户接口
 */
// void Shell::read()
// {
//     Logcat::log(TAG, "read EXEC");
// }

/**
 * 临时的，不应该是一个用户接口
 */
// void Shell::write()
// {
//     Logcat::log(TAG, "write EXEC");
// }

/**
 * 临时的，不应该是一个用户接口
 */
// void Shell::lseek()
// {
//     Logcat::log(TAG, "lseek EXEC");
// }