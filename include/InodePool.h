#ifndef INODE_POOL_H
#define INODE_POOL_H
#include "define.h"
#include "DiskInode.h"
#include "Bitmap.h"

/**
 * 磁盘Inode区，
 * 大小是一定的，Inode号是有限的。
 * 
 */
class InodePool{
  //TODO
  private:
    Bitmap inodePoolBitmap;
    char padding1[DISK_BLOCK_SIZE - sizeof(Bitmap)];  // 确保 Bitmap 填满 block 2
    DiskInode inodeBlock[MAX_INODE_NUM];              // INODE数组 — 从 block 3 开始
    char padding2[2 * DISK_BLOCK_SIZE - MAX_INODE_NUM * sizeof(DiskInode)];  // 填满 block 3,4
    

  public:
    InodePool();
    int ialloc();
    void ifree(InodeId inodeID);
    void iupdate(InodeId inodeId,DiskInode diskInode);  
    DiskInode* getInode(InodeId inodeID);

};


#endif
/**
 * 可以升级的地方：
 * 现代UNIX操作系统使用铭记inode，可以节省IO,creat系统调用不会因为ialloc入睡。
 * 有时间再升级。
 */

// File maintained for Unix FileSystem course project - experiment 2 update 
