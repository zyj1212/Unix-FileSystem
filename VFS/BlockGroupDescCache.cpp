#include "../include/BlockGroupDescCache.h"
#include "../include/Kernel.h"
#include "../include/BufferCache.h"
#include <cstring>

BlockGroupDescCache::BlockGroupDescCache()
{
    dirty = false;
    memset(&desc, 0, sizeof(BlockGroupDesc));
}

BlockGroupDescCache::~BlockGroupDescCache()
{
}

void BlockGroupDescCache::mount(int blkno)
{
    Buf *pBuf = Kernel::instance()->getBufferCache().Bread(blkno);
    memcpy(&desc, pBuf->b_addr, sizeof(BlockGroupDesc));
    Kernel::instance()->getBufferCache().Brelse(pBuf);
    dirty = false;
}

void BlockGroupDescCache::flushBack()
{
    Buf *pBuf = Kernel::instance()->getBufferCache().GetBlk(1);
    memcpy(pBuf->b_addr, &desc, sizeof(BlockGroupDesc));
    Kernel::instance()->getBufferCache().Bdwrite(pBuf);
    dirty = false;
}
