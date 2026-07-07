#include "../include/Inode.h"
#include "../include/Kernel.h"
Inode::Inode()
{
    // dirty = false;
    // inode_id = 0;
    // memset(i_addr, 0, 10 * sizeof(int));
}

/**
 * 转换构造函数,
 * 将磁盘inode结构转换为内存inode结构
 * NOTE:需要对i_number进行单独的赋值
 */
Inode::Inode(DiskInode d_inode)
{
    this->i_mode = d_inode.d_mode;
    this->i_nlink = d_inode.d_nlink;
    this->i_uid = d_inode.d_uid;
    this->i_gid = d_inode.d_gid;
    this->i_size = d_inode.d_size;
    memcpy(this->i_addr, d_inode.d_addr, sizeof(d_inode.d_addr));
    this->i_flag = 0;
    this->i_count = 0;
    this->i_dev = 0;
    //this->i_number = ? ;s  注意！DISKINODE是没有INODE号这个属性的，一个DISKINODE的号是固定的，可以根据其位置算出来
    this->i_lastr = -1;
    this->i_ctime = d_inode.d_ctime;
}

/**
 * 根据混合索引表，用逻辑块号，查出物理盘块号.
 * NOTE 功能不止查。
 * TODO bmap暂时不做
 */
int Inode::Bmap(int lbn)
{

    int phyBlkno; /* 转换后的物理盘块号 */
    int *iTable;  /* 用于访问索引盘块中各级间接索引表 */
    int index;
    Buf *pFirstBuf, *pSecondBuf;

    // 超出支持的最大文件块数
    if (lbn >= Inode::MAX_FILE_BLOCK)
    {
        return ERROR_LBN_OVERFLOW;
    }

    // ===== 直接索引: lbn < 12 =====
    if (lbn < Inode::SMALL_FILE_BLOCK)
    {
        phyBlkno = this->i_addr[lbn];
        if (phyBlkno == 0)
        {
            phyBlkno = Kernel::instance()->getSuperBlockCache().balloc();
            if (phyBlkno == -1)
            {
                return ERROR_OUTOF_BLOCK;
            }
            this->i_addr[lbn] = phyBlkno;
            this->i_flag |= Inode::IUPD;
        }
        return phyBlkno;
    }

    // ===== 间接索引: lbn >= 12 =====
    // 确定 i_addr 中的索引位置
    if (lbn < Inode::LARGE_FILE_BLOCK)
    {
        index = Inode::SINGLE_INDIRECT_IDX;  // i_addr[12] — 单间接
    }
    else if (lbn < Inode::HUGE_FILE_BLOCK)
    {
        index = Inode::DOUBLE_INDIRECT_IDX;  // i_addr[13] — 双间接
    }
    else
    {
        index = Inode::TRIPLE_INDIRECT_IDX;  // i_addr[14] — 三间接
    }

    // 读取/分配第一级索引块
    phyBlkno = this->i_addr[index];
    if (0 == phyBlkno)
    {
        this->i_flag |= Inode::IUPD;
        int newBlkNum = Kernel::instance()->getSuperBlockCache().balloc();
        if (newBlkNum < 0)
        {
            return ERROR_OUTOF_BLOCK;
        }
        this->i_addr[index] = newBlkNum;
        pFirstBuf = Kernel::instance()->getBufferCache().GetBlk(newBlkNum);
    }
    else
    {
        pFirstBuf = Kernel::instance()->getBufferCache().Bread(phyBlkno);
    }
    iTable = (int *)pFirstBuf->b_addr;

    // ===== 第二层索引: 双间接或三间接 =====
    if (index >= Inode::DOUBLE_INDIRECT_IDX) // 13 或 14
    {
        int outerIndex;
        if (index == Inode::TRIPLE_INDIRECT_IDX)
        {
            // 三间接: 三级索引块 → 二级索引块
            outerIndex = (lbn - Inode::HUGE_FILE_BLOCK) /
                         (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK);
        }
        else
        {
            // 双间接: 二级索引块 → 一级索引块
            outerIndex = (lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK;
        }

        phyBlkno = iTable[outerIndex];
        if (0 == phyBlkno)
        {
            BlkNum newBlkNum = Kernel::instance()->getSuperBlockCache().balloc();
            if (newBlkNum < 0)
            {
                Kernel::instance()->getBufferCache().Brelse(pFirstBuf);
                return ERROR_OUTOF_BLOCK;
            }
            iTable[outerIndex] = newBlkNum;
            pSecondBuf = Kernel::instance()->getBufferCache().GetBlk(newBlkNum);
            Kernel::instance()->getBufferCache().Bdwrite(pFirstBuf);
        }
        else
        {
            Kernel::instance()->getBufferCache().Brelse(pFirstBuf);
            pSecondBuf = Kernel::instance()->getBufferCache().Bread(phyBlkno);
        }
        pFirstBuf = pSecondBuf;
        iTable = (int *)pFirstBuf->b_addr;
    }

    // ===== 第三层索引: 仅三间接 =====
    if (index == Inode::TRIPLE_INDIRECT_IDX)
    {
        // 二级索引块 → 一级索引块
        int midIndex = ((lbn - Inode::HUGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) %
                       Inode::ADDRESS_PER_INDEX_BLOCK;

        phyBlkno = iTable[midIndex];
        if (0 == phyBlkno)
        {
            BlkNum newBlkNum = Kernel::instance()->getSuperBlockCache().balloc();
            if (newBlkNum < 0)
            {
                Kernel::instance()->getBufferCache().Brelse(pFirstBuf);
                return ERROR_OUTOF_BLOCK;
            }
            iTable[midIndex] = newBlkNum;
            pSecondBuf = Kernel::instance()->getBufferCache().GetBlk(newBlkNum);
            Kernel::instance()->getBufferCache().Bdwrite(pFirstBuf);
        }
        else
        {
            Kernel::instance()->getBufferCache().Brelse(pFirstBuf);
            pSecondBuf = Kernel::instance()->getBufferCache().Bread(phyBlkno);
        }
        pFirstBuf = pSecondBuf;
        iTable = (int *)pFirstBuf->b_addr;
    }

    // ===== 最终层: 一级索引表 → 数据块 =====
    if (lbn < Inode::LARGE_FILE_BLOCK)
    {
        index = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    }
    else if (lbn < Inode::HUGE_FILE_BLOCK)
    {
        index = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    }
    else
    {
        index = (lbn - Inode::HUGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
    }

    int newBlk3;
    if ((phyBlkno = iTable[index]) == 0 &&
        (newBlk3 = Kernel::instance()->getSuperBlockCache().balloc()) >= 0)
    {
        phyBlkno = newBlk3;
        iTable[index] = phyBlkno;
        pSecondBuf = Kernel::instance()->getBufferCache().GetBlk(newBlk3);
        Kernel::instance()->getBufferCache().Bdwrite(pSecondBuf);
        Kernel::instance()->getBufferCache().Bdwrite(pFirstBuf);
    }
    else
    {
        Kernel::instance()->getBufferCache().Brelse(pFirstBuf);
    }
    return phyBlkno;
}
// File maintained for Unix FileSystem course project - experiment 2 update 
