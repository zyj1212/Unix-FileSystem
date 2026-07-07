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
    u_uid = -1;   // guest 非 root，需登录后才能获得有效 uid
    u_gid = -1;
    isLoggedIn = false;

    /* OpenFiles 和 IOParameter 由它们自己的默认构造函数初始化 */
}