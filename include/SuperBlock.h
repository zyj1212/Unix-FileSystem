#ifndef SUPER_BLOCK_H
#define SUPER_BLOCK_H
#include "define.h"
#include "Bitmap.h"
/**
 * SuperBlock在安装磁盘的时候读入内存，磁盘使用过程改动内存超级块。
 * 最晚在磁盘卸载的时候，需要把脏超级块刷回磁盘。（这个时候磁盘缓存也会被刷回）
 */

/**
 * 我的超级块采用的是bitmap管理空闲盘块
 * 一个SuperBlock应该占据有一块盘块，大小为DISK_BLOCK_SIZE
 */
class SuperBlock
{
public:
  SuperBlock();
  size_t SuperBlockBlockNum = 1; //暂时考虑superblock占1个磁盘block
  int free_inode_num;            //空闲inode
  int free_block_bum;            //空闲盘块数
  int total_block_num;           //总盘块数
  int total_inode_num;           //总inode数
  InodeId s_inode[MAX_INODE_NUM-1];   //空闲inode栈，用于管理inode的分配和回收
  Bitmap disk_block_bitmap;      //用bitmap管理空闲盘块
  // padding 确保 SuperBlock 恰好占满一个磁盘块
  // sizeof(other fields) ≈ 8(size_t)+16(4 ints)+368(s_inode[92])+2056(Bitmap) = 2448
  char padding[DISK_BLOCK_SIZE - 2448];

  BlkNum balloc();
  void bfree(BlkNum blkNum);
  void bsetOccupy(BlkNum blkNum);
  InodeId ialloc();
  void ifree(InodeId inodeId);
};

#endif// File maintained for Unix FileSystem course project - experiment 2 update 
