#ifndef BLOCK_GROUP_DESC_CACHE_H
#define BLOCK_GROUP_DESC_CACHE_H

#include "BlockGroupDesc.h"
#include "define.h"

/**
 * VFS 层的组描述符缓存
 * 模式与 SuperBlockCache 一致
 */
class BlockGroupDescCache
{
public:
    BlockGroupDesc desc;  /* 缓存的组描述符 */
    bool dirty;           /* 脏标志：是否需要写回磁盘 */

    BlockGroupDescCache();
    ~BlockGroupDescCache();

    /* 从指定盘块号加载 GDT 到缓存 */
    void mount(int blkno);

    /* 将缓存的 GDT 写回磁盘 */
    void flushBack();
};

#endif
