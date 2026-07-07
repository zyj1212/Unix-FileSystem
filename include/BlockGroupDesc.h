#ifndef BLOCK_GROUP_DESC_H
#define BLOCK_GROUP_DESC_H

#include "define.h"

/**
 * 组描述符结构体
 * 描述文件系统中唯一的块组（单组 EXT2）
 * 占用一个完整的磁盘块（4096 字节）
 */
struct BlockGroupDesc
{
    int bg_block_bitmap;       /* 块位图所在的盘块号 (block 0, SuperBlock 内) */
    int bg_inode_bitmap;       /* inode 位图所在的盘块号 (block 2, InodePool 内) */
    int bg_inode_table;        /* inode 表起始盘块号 (block 3) */
    int bg_free_blocks_count;  /* 组内空闲块数 */
    int bg_free_inodes_count;  /* 组内空闲 inode 数 */
    int bg_used_dirs_count;    /* 组内已用目录数 */

    /* 手工填充到 4096 字节 = 一个磁盘块 */
    char padding[4096 - 24];
};

#endif
