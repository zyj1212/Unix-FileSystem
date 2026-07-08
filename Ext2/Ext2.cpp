#include "../include/Ext2.h"
#include "../include/Kernel.h"
#include "../include/VirtualProcess.h"
#include "../include/TimeHelper.h"
#include "../include/BlockGroupDesc.h"
#include "../include/BlockGroupDescCache.h"

/**
 * 这个函数貌似是最格格不入的。
 * 为了简便，做硬写入。不经过缓存层，直接借助mmap对img进行写入完成初始化。
 */
void Ext2::format()
{
    p_bufferCache->initialize();
    //0# superblock
    //1# GDT (组描述符表)
    //2,3,4# inodePool
    // 5~DISK_BLOCK_NUM-1# 放数据
    DiskBlock *diskMemAddr = Kernel::instance()->getDiskDriver().getDiskMemAddr();
    memset(diskMemAddr, 0, DISK_SIZE);

    //①构造一个superBlock结构，写入磁盘中
    SuperBlock tempSuperBlock;
    tempSuperBlock.total_block_num = DISK_BLOCK_NUM;
    tempSuperBlock.free_block_bum = DISK_BLOCK_NUM;
    // tempSuperBlock.total_inode_num = MAX_INODE_NUM;
    // tempSuperBlock.free_inode_num = MAX_INODE_NUM;
    tempSuperBlock.bsetOccupy(0); //0#盘块被superblock占据
    tempSuperBlock.bsetOccupy(1); //1#盘块被GDT占据
    tempSuperBlock.bsetOccupy(2);
    tempSuperBlock.bsetOccupy(3);
    tempSuperBlock.bsetOccupy(4); //2~4#盘块被inodePool占据(即磁盘Inode区)
    tempSuperBlock.bsetOccupy(5); //5#盘块放根目录文件
    tempSuperBlock.bsetOccupy(6); //6#盘块放bin目录文件
    tempSuperBlock.bsetOccupy(7); //7#盘块放etc目录文件
    tempSuperBlock.bsetOccupy(8); //8#盘块放home目录文件
    tempSuperBlock.bsetOccupy(9); //9#盘块放dev目录文件
    //tempSuperBlock.free_block_bum -= 9;
    tempSuperBlock.free_inode_num -= 5;

    // ①-1 写入 SuperBlock 到 block 0
    memcpy(diskMemAddr, &tempSuperBlock, sizeof(SuperBlock));

    // ①-2 写入 GDT 到 block 1
    BlockGroupDesc tempGDT;
    memset(&tempGDT, 0, sizeof(BlockGroupDesc));
    tempGDT.bg_block_bitmap = 0;                                         // 块位图在 SuperBlock 内 (block 0)
    tempGDT.bg_inode_bitmap = 2;                                         // Inode 位图在 InodePool 内 (block 2)
    tempGDT.bg_inode_table = 3;                                          // Inode 表从 block 3 开始
    tempGDT.bg_free_blocks_count = DISK_BLOCK_NUM - 10;                  // 已占用 10 块
    tempGDT.bg_free_inodes_count = (MAX_INODE_NUM - 1) - 5;              // 已用 5 个 inode (根+4目录)
    tempGDT.bg_used_dirs_count = 5;                                      // 5 个目录
    memcpy(diskMemAddr + 1, &tempGDT, DISK_BLOCK_SIZE);
    // 送一份到 VFS 的 GDT 缓存
    Kernel::instance()->getBlockGroupDescCache().desc = tempGDT;
    Kernel::instance()->getBlockGroupDescCache().dirty = false;
    //还要送一份到VFS中
    Kernel::instance()->getSuperBlockCache().dirty = false;
    Kernel::instance()->getSuperBlockCache().SuperBlockBlockNum = tempSuperBlock.SuperBlockBlockNum;
    Kernel::instance()->getSuperBlockCache().free_inode_num = tempSuperBlock.free_inode_num; //空闲inode
    Kernel::instance()->getSuperBlockCache().free_block_bum = tempSuperBlock.free_block_bum;
    //空闲盘块数
    Kernel::instance()->getSuperBlockCache().total_block_num = tempSuperBlock.total_block_num;     //总盘块数
    Kernel::instance()->getSuperBlockCache().total_inode_num = tempSuperBlock.total_inode_num;     //总inode数
    Kernel::instance()->getSuperBlockCache().disk_block_bitmap = tempSuperBlock.disk_block_bitmap; //用bitmap管理空闲盘块
    memcpy(Kernel::instance()->getSuperBlockCache().s_inode, tempSuperBlock.s_inode, sizeof(tempSuperBlock.s_inode));

    //②构造DiskInode,修改InodePool,将InodePool写入磁盘img (block 2,3,4)
    InodePool tempInodePool;
    int tempAddr[15] = {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    DiskInode tempDiskInode = DiskInode(Inode::IFDIR | Inode::DEFAULT_DIR_MODE, 1, 0, 0, 6 * sizeof(DirectoryEntry), tempAddr, TimeHelper::getCurTime(), TimeHelper::getCurTime(), TimeHelper::getCurTime());
    tempInodePool.iupdate(1, tempDiskInode);
    //1#inode，是根目录
    tempDiskInode.d_addr[0] = 6;
    tempDiskInode.d_size = sizeof(DirectoryEntry) * 2;
    tempInodePool.iupdate(2, tempDiskInode);
    //2#inode，是bin
    tempDiskInode.d_addr[0] = 7;
    tempDiskInode.d_size = sizeof(DirectoryEntry) * 2;
    tempInodePool.iupdate(3, tempDiskInode);
    //3#inode，是etc
    tempDiskInode.d_addr[0] = 8;
    tempDiskInode.d_size = sizeof(DirectoryEntry) * 2;
    tempInodePool.iupdate(4, tempDiskInode);
    //4#inode，是home
    tempDiskInode.d_addr[0] = 9;
    tempDiskInode.d_size = sizeof(DirectoryEntry) * 2;
    tempInodePool.iupdate(5, tempDiskInode);
    //5#inode，是dev
    memcpy(diskMemAddr + 2, &tempInodePool, 3 * DISK_BLOCK_SIZE);

    //③数据区写入目录文件 (block 5 = 根目录, block 6 = bin, block 7 = etc, block 8 = dev, block 9 = home)
    DirectoryEntry *p_directoryEntry = (DirectoryEntry *)(diskMemAddr + 5);
    DirectoryEntry tempDirctoryEntry;
    // block 5: 根目录文件 (. .. bin etc dev home)
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "bin");
    tempDirctoryEntry.m_ino = 2;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "etc");
    tempDirctoryEntry.m_ino = 3;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "dev");
    tempDirctoryEntry.m_ino = 4;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "home");
    tempDirctoryEntry.m_ino = 5;
    *p_directoryEntry = tempDirctoryEntry;

    // block 6: bin目录 (. ..)
    p_directoryEntry = (DirectoryEntry *)(diskMemAddr + 6);
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 2;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    // block 7: etc目录 (. ..)
    p_directoryEntry = (DirectoryEntry *)(diskMemAddr + 7);
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 3;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    // block 8: dev目录 (. ..)
    p_directoryEntry = (DirectoryEntry *)(diskMemAddr + 8);
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 4;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    // block 9: home目录 (. ..)
    p_directoryEntry = (DirectoryEntry *)(diskMemAddr + 9);
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 5;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    // 格式化完成后：丢弃所有旧缓存（不刷回！否则旧数据会覆盖新磁盘）
    // 只需重新初始化缓存层，不执行 Bflush
    // 注意：不调用 diskDriver.mount()，因为 diskMemAddr 已经在第一次 mount 时映射好了
    //        如果再次 mount 会导致二次 mmap，覆盖刚刚写入的新数据
    p_bufferCache->initialize();
    //如果格式化成功，将ext2_status置ready
    ext2_status = Ext2_READY;
}

int Ext2::registerFs()
{
    /**
 * mount的前一步在vfs.cpp中完成
 * 
 * 
 * 
 */
    int mountRes = this->p_bufferCache->mount(); //②DiskDriver打开虚拟磁盘img，mmap，进入就绪状态
    if (mountRes == -1)
    {
        ext2_status = Ext2_UNINITIALIZED;
    }
    else if (mountRes == 0) //有现成的(认为已经格式化)  //NOTE 这里可以提升
    {
        ext2_status = Ext2_READY;
        //NOTE 显然，如果是一个未格式话的磁盘，下面的操作没有意义。
        //④装载SuperBlock到VFS的SuperBlock缓存(这一步需要经过缓存层)
        SuperBlock tempSuperBlock; //从磁盘上读的先放到这个对象里（用于解析），然后再挪到vfs superblock
        loadSuperBlock(tempSuperBlock);
        //搬到vfs中的superBlockCache
        SuperBlockCache &kernelSBC = Kernel::instance()->getSuperBlockCache();
        kernelSBC.dirty = false;
        kernelSBC.disk_block_bitmap = tempSuperBlock.disk_block_bitmap;
        kernelSBC.free_block_bum = tempSuperBlock.free_block_bum;
        kernelSBC.free_inode_num = tempSuperBlock.free_inode_num;
        kernelSBC.total_block_num = tempSuperBlock.total_block_num;
        kernelSBC.total_inode_num = tempSuperBlock.total_inode_num;
        kernelSBC.SuperBlockBlockNum = tempSuperBlock.SuperBlockBlockNum;
        memcpy(kernelSBC.s_inode, tempSuperBlock.s_inode, sizeof(tempSuperBlock.s_inode));

        // 加载 GDT 到 VFS 缓存
        Kernel::instance()->getBlockGroupDescCache().mount(1);
    }
    else if (mountRes == 1)
    { // 新生成的img，还有待格式化

        //NO DO THIS//③ext模块中的指针赋值，指向img文件内存映射的地址。
        ext2_status = Ext2_NOFORM;
    }

    return OK;
}

int Ext2::unregisterFs()
{
    p_bufferCache->unmount();
    ext2_status = Ext2_UNINITIALIZED;
    return OK;
}

void Ext2::loadSuperBlock(SuperBlock &superBlock)
{
    //User &u = VirtualProcess::Instance()->getUser();
    Buf *pBuf;
    pBuf = p_bufferCache->Bread(0);
    memcpy(&superBlock, pBuf->b_addr, sizeof(SuperBlock));
    p_bufferCache->Brelse(pBuf);
}

int Ext2::setBufferCache(BufferCache *p_bufferCache)
{
    this->p_bufferCache = p_bufferCache;
    return OK;
}
int Ext2::allocNewInode()
{
    return OK;
}
//写回脏inode回磁盘（可能还是在缓存中，但是这里不管，缓存层是透明的）
void Ext2::updateDiskInode(int inodeID, DiskInode diskInode)
{
    //要先读后写!
    int blkno = 3 + inodeID / (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    Buf *pBuf;
    pBuf = p_bufferCache->Bread(blkno);
    DiskInode *p_diskInode = (DiskInode *)pBuf->b_addr;
    p_diskInode = p_diskInode + inodeID % (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    //定位到需要写diskInode的位置
    *p_diskInode = diskInode;     //更新DiskInode
    p_bufferCache->Bdwrite(pBuf); //bdwrite中会做brelse的。
    //p_bufferCache->Brelse(pBuf);
}
void Ext2::readDiskInode(int inodeID, DiskInode &diskInode)
{
    int blkno;
    blkno = 3 + inodeID / (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    Buf *pBuf;
    pBuf = p_bufferCache->Bread(blkno);
    DiskInode *p_diskInode = (DiskInode *)pBuf->b_addr;
    p_diskInode = p_diskInode + inodeID % (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    diskInode = *p_diskInode;
    p_bufferCache->Brelse(pBuf);
}

int Ext2::iAssign(Inode **ppInode)
{
    Inode *pInode = NULL;
    DiskInode diskInode;

    // 使用 SuperBlockCache 的 inode 栈分配，获取 free inode 号
    InodeId inodeNumber = Kernel::instance()->getSuperBlockCache().ialloc();
    if (inodeNumber <= 0)
    {
        return ERROR_OUTOF_INODE;
    }

    // 初始化 DiskInode
    memset(&diskInode, 0, sizeof(diskInode));
    for (int i = 0; i < 15; i++)
    {
        diskInode.d_addr[i] = 0;
    }
    diskInode.d_mode = 0;
    diskInode.d_nlink = 1;
    diskInode.d_uid = VirtualProcess::Instance()->Getuid();
    diskInode.d_gid = VirtualProcess::Instance()->Getgid();
    diskInode.d_size = 0;
    diskInode.d_atime = TimeHelper::getCurTime();
    diskInode.d_mtime = TimeHelper::getCurTime();
    diskInode.d_ctime = TimeHelper::getCurTime();

    // 刷入磁盘 inode
    updateDiskInode(inodeNumber, diskInode);

    // 载入内存 Inode 缓存
    if (Kernel::instance()->getInodeCache().addInodeCache(diskInode, inodeNumber) < 0)
    {
        return ERROR_OUTOF_INODE;
    }
    pInode = Kernel::instance()->getInodeCache().getInodeByID(inodeNumber);
    if (pInode == NULL)
    {
        return ERROR_OUTOF_INODE;
    }

    pInode->i_mode = 0;
    pInode->i_nlink = 1;
    pInode->i_uid = VirtualProcess::Instance()->Getuid();
    pInode->i_gid = VirtualProcess::Instance()->Getgid();
    pInode->i_size = 0;
    pInode->i_ctime = TimeHelper::getCurTime();

    *ppInode = pInode;
    return OK;
}
/**
 * 释放一个inode，交给superBlockCache管理
 */
void Ext2::iFree(int inodeId)
{
    Kernel::instance()->getSuperBlockCache().ifree(inodeId);
}

Ext2_Status Ext2::getExt2Status()
{
    return ext2_status;
}

// 从磁盘读取指定 inode 号的 DiskInode
DiskInode Ext2::getDiskInodeByNum(int inodeID)
{
    int blkno = 3 + inodeID / (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    Buf *pBuf;
    pBuf = p_bufferCache->Bread(blkno);
    DiskInode *p_diskInode = (DiskInode *)pBuf->b_addr;
    DiskInode tempDiskInode;
    tempDiskInode = *(p_diskInode + inodeID % (DISK_BLOCK_SIZE / DISKINODE_SIZE));
    p_bufferCache->Brelse(pBuf);
    return tempDiskInode;
}

// 根据路径查找 inode 号
InodeId Ext2::locateInode(Path &path)
{
    InodeId dirInodeId = locateDir(path);
    if (path.level == 0)
    {
        return ROOT_INODE_ID;
    }
    else
    {
        return getInodeIdInDir(dirInodeId, path.getInodeName());
    }
}

// 根据路径查找父目录的 inode 号
InodeId Ext2::locateDir(Path &path)
{
    InodeId dirInode;
    if (path.from_root)
    {
        dirInode = ROOT_INODE_ID;
    }
    else
    {
        dirInode = VirtualProcess::Instance()->getUser().curDirInodeId;
    }

    for (int i = 0; i < path.level - 1; i++)
    {
        dirInode = getInodeIdInDir(dirInode, path.path[i]);
        if (dirInode < 0)
        {
            return ERROR_PATH_NFOUND;
        }
    }
    return dirInode;
}

// 在指定目录中按文件名线性搜索 inode 号
InodeId Ext2::getInodeIdInDir(InodeId dirInodeId, FileName fileName)
{
    Inode *p_dirInode = Kernel::instance()->getInodeCache().getInodeByID(dirInodeId);
    int blkno = p_dirInode->Bmap(0);
    Buf *pBuf;
    pBuf = Kernel::instance()->getBufferCache().Bread(blkno);
    DirectoryEntry *p_directoryEntry = (DirectoryEntry *)pBuf->b_addr;

    for (int i = 0; i < DISK_BLOCK_SIZE / sizeof(DirectoryEntry); i++)
    {
        if ((p_directoryEntry->m_ino != 0) && (!strcmp(p_directoryEntry->m_name, fileName)))
        {
            return p_directoryEntry->m_ino;
        }
        p_directoryEntry++;
    }
    Kernel::instance()->getBufferCache().Brelse(pBuf);
    return -1;
}

int Ext2::bmap(int inodeNum, int logicBlockNum)
{
    return OK;
}