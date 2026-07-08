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

    SuperBlock *p_superBlock = (SuperBlock *)diskMemAddr;
    *p_superBlock = tempSuperBlock; //没有动态申请，不用管深浅拷贝
    p_superBlock++;
    diskMemAddr = (DiskBlock *)p_superBlock;

    // ①-2 写入 GDT 到 block 1
    BlockGroupDesc tempGDT;
    memset(&tempGDT, 0, sizeof(BlockGroupDesc));
    tempGDT.bg_block_bitmap = 0;                                         // 块位图在 SuperBlock 内 (block 0)
    tempGDT.bg_inode_bitmap = 2;                                         // Inode 位图在 InodePool 内 (block 2)
    tempGDT.bg_inode_table = 3;                                          // Inode 表从 block 3 开始
    tempGDT.bg_free_blocks_count = DISK_BLOCK_NUM - 10;                  // 已占用 10 块
    tempGDT.bg_free_inodes_count = (MAX_INODE_NUM - 1) - 5;              // 已用 5 个 inode (根+4目录)
    tempGDT.bg_used_dirs_count = 5;                                      // 5 个目录
    BlockGroupDesc *p_gdt = (BlockGroupDesc *)diskMemAddr;
    *p_gdt = tempGDT;
    p_gdt++;
    diskMemAddr = (DiskBlock *)p_gdt;
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

    //②构造DiskInode,修改InodePool,将InodePool写入磁盘img
    InodePool tempInodePool;
    int tempAddr[15] = {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    DiskInode tempDiskInode = DiskInode(Inode::IFDIR, 1, 0, 0, 6 * sizeof(DirectoryEntry), tempAddr, TimeHelper::getCurTime(), TimeHelper::getCurTime(), TimeHelper::getCurTime());
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
    InodePool *p_InodePool = (InodePool *)diskMemAddr;
    *p_InodePool = tempInodePool;
    p_InodePool++;
    diskMemAddr = (DiskBlock *)p_InodePool;

    //③数据区写入目录文件
    DirectoryEntry *p_directoryEntry = (DirectoryEntry *)diskMemAddr;
    DirectoryEntry tempDirctoryEntry;
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

    diskMemAddr++; //移动到下一个盘块，写bin目录
    p_directoryEntry = (DirectoryEntry *)diskMemAddr;
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 2;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    diskMemAddr++; //移动到下一个盘块，写etc目录
    p_directoryEntry = (DirectoryEntry *)diskMemAddr;
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 3;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    diskMemAddr++; //移动到下一个盘块，写dev目录
    p_directoryEntry = (DirectoryEntry *)diskMemAddr;
    strcpy(tempDirctoryEntry.m_name, ".");
    tempDirctoryEntry.m_ino = 4;
    *p_directoryEntry = tempDirctoryEntry;
    p_directoryEntry++;
    strcpy(tempDirctoryEntry.m_name, "..");
    tempDirctoryEntry.m_ino = 1;
    *p_directoryEntry = tempDirctoryEntry;

    diskMemAddr++; //移动到下一个盘块，写home目录
    p_directoryEntry = (DirectoryEntry *)diskMemAddr;
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
    memcpy(&superBlock, pBuf->b_addr, DISK_BLOCK_SIZE);
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
//从disk中读取出一个制定inodeID的DiskInode.可能涉及io了（不确定在不在缓存中）
DiskInode Ext2::getDiskInodeByNum(int inodeID)
{
    //inode分布在两个盘块上，首先根据inodeID计算在哪个盘块上
    int blkno = 3 + inodeID / (DISK_BLOCK_SIZE / DISKINODE_SIZE);
    Buf *pBuf;
    pBuf = p_bufferCache->Bread(blkno);
    DiskInode *p_diskInode = (DiskInode *)pBuf->b_addr;
    DiskInode tempDiskInode;
    tempDiskInode = *(p_diskInode + inodeID % (DISK_BLOCK_SIZE / DISKINODE_SIZE));
    p_bufferCache->Brelse(pBuf);
    return tempDiskInode; //外部可能会调用DiskInode的拷贝构造函数
}

/**
 * 功能：根据路径，做线性目录搜索
 * VFS在inodeDirectoryCache失效的时候，会调用本函数，在磁盘上根据路径确定inode号。
 * 
 */
InodeId Ext2::locateInode(Path &path)
{

    InodeId dirInodeId = locateDir(path); //先确定其父目录的inode号
    if (path.level == 0)
    {
        return ROOT_INODE_ID;
    }
    else
    {
        return getInodeIdInDir(dirInodeId, path.getInodeName());
    }
}

/**
 * VFS在inodeDirectoryCache失效的时候，会调用本函数，在磁盘上根据路径确定
 * 一个路径截至最后一层之前的目录inode号
 */
InodeId Ext2::locateDir(Path &path)
{

    InodeId dirInode;   //目录文件的inode号
    if (path.from_root) //如果是绝对路径,从根inode开始搜索
    {
        dirInode = ROOT_INODE_ID;
    }
    else //如果是相对路径，从当前inode号开始搜索
    {
        dirInode = VirtualProcess::Instance()->getUser().curDirInodeId;
    }

    for (int i = 0; i < path.level - 1; i++)
    {
        dirInode = getInodeIdInDir(dirInode, path.path[i]);
        if (dirInode < 0)
        {
            return ERROR_PATH_NFOUND; //没有找到
        }
    }

    return dirInode;
}

/**
 * 线性目录搜索的步骤：根据文件名在目录文件（已知inode号）中查找inode号。
 * 可以知道目录搜索的起点一定是已知的inode号，要么是cur要么是ROOT
 */
InodeId Ext2::getInodeIdInDir(InodeId dirInodeId, FileName fileName)
{
    //Step1:先根据目录inode号dirInodeId获得目录inode对象
    Inode *p_dirInode = Kernel::instance()->getInodeCache().getInodeByID(dirInodeId);
    //TODO 错误处理
    //Step2：读取该inode指示的数据块
    int blkno = p_dirInode->Bmap(0); //Bmap查物理块号
    Buf *pBuf;
    pBuf = Kernel::instance()->getBufferCache().Bread(blkno);
    DirectoryEntry *p_directoryEntry = (DirectoryEntry *)pBuf->b_addr;
    //Step3：访问这个目录文件中的entry，搜索（同时缓存到dentryCache中）
    //TODO 缓存到dentryCache中
    for (int i = 0; i < DISK_BLOCK_SIZE / sizeof(DirectoryEntry); i++)
    {
        if ((p_directoryEntry->m_ino != 0) && (!strcmp(p_directoryEntry->m_name, fileName)))
        {
            return p_directoryEntry->m_ino;
        } //ino==0表示该文件被删除
        p_directoryEntry++;
    }

    Kernel::instance()->getBufferCache().Brelse(pBuf);
    return -1;
}

int Ext2::bmap(int inodeNum, int logicBlockNum)
{

    return OK;
} //文件中的地址映射。查混合索引表，确定物理块号。
  //逻辑块号bn=u_offset/512

Ext2_Status Ext2::getExt2Status()
{
    return ext2_status;
}
// File maintained for Unix FileSystem course project - experiment 2 update 
