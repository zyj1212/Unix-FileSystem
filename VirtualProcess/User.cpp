#include "../include/User.h"
#include <cstring>

User::User()
{
    u_cdir = nullptr;
    u_pdir = nullptr;
    curDirInodeId = 0;

    // 用户标识初始化
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    u_uid = 0;
    u_gid = 0;
    isLoggedIn = false;

    /* OpenFiles 和 IOParameter 由它们自己的默认构造函数初始化 */
}