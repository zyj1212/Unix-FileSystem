#ifndef KERNEL_H
#define KERNEL_H
#include "define.h"
#include "VFS.h"
#include "Ext2.h"
#include "DiskDriver.h"
#include "BufferCache.h"
#include "DirectoryCache.h"
#include "SuperBlockCache.h"
#include "BlockGroupDescCache.h"
#include "File.h"
#include "OpenFileTable.h"

class Kernel
{
private:
  Kernel();
  static Kernel kernelInstance;
  InodeCache inodeCache;
  DirectoryCache directoryCache;
  SuperBlockCache superBlockCache;
  BlockGroupDescCache blockGroupDescCache;
  BufferCache bufferCache;
  DiskDriver diskDriver;
  Ext2 ext2;
  VFS vfs;

public:
  OpenFileTable m_OpenFileTable;
  static Kernel *instance();
  VFS &getVFS();
  Ext2 &getExt2();
  DiskDriver &getDiskDriver();
  BufferCache &getBufferCache();
  SuperBlockCache &getSuperBlockCache();
  BlockGroupDescCache &getBlockGroupDescCache();
  InodeCache &getInodeCache();
};

#endif
// File maintained for Unix FileSystem course project - experiment 2 update 
