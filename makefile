CXX = g++
CXXFLAGS = -g -std=c++11 -I include
LDFLAGS = -lpthread

# 所有源文件（含子目录）
SRCS = main.cpp \
       Shell/Shell.cpp \
       VFS/VFS.cpp VFS/File.cpp VFS/DirectoryEntry.cpp VFS/Inode.cpp \
       VFS/InodeCache.cpp VFS/DirectoryCache.cpp VFS/SuperBlockCache.cpp \
       VFS/BlockGroupDescCache.cpp VFS/OpenFileTable.cpp \
       Ext2/Ext2.cpp Ext2/SuperBlock.cpp Ext2/InodePool.cpp Ext2/DiskInode.cpp Ext2/Path.cpp \
       VirtualProcess/Kernel.cpp VirtualProcess/User.cpp VirtualProcess/VirtualProcess.cpp \
       BufferCache/BufferCache.cpp BufferCache/Buf.cpp BufferCache/BufferLruList.cpp \
       DiskDriver/DiskDriver.cpp DiskDriver/DiskBlock.cpp \
       Utils/Bitmap.cpp Utils/Logcat.cpp Utils/TimeHelper.cpp

# 生成根目录下的 .o 文件
OBJS = $(notdir $(SRCS:.cpp=.o))

# 默认目标
user_fs: $(OBJS)
	$(CXX) -g -o $@ $^ $(LDFLAGS)

# 模式规则：从各子目录编译 .cpp 到当前目录的 .o
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Shell.o: Shell/Shell.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

VFS.o: VFS/VFS.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

File.o: VFS/File.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

DirectoryEntry.o: VFS/DirectoryEntry.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Inode.o: VFS/Inode.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

InodeCache.o: VFS/InodeCache.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

DirectoryCache.o: VFS/DirectoryCache.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

SuperBlockCache.o: VFS/SuperBlockCache.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

BlockGroupDescCache.o: VFS/BlockGroupDescCache.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

OpenFileTable.o: VFS/OpenFileTable.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Ext2.o: Ext2/Ext2.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

SuperBlock.o: Ext2/SuperBlock.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

InodePool.o: Ext2/InodePool.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

DiskInode.o: Ext2/DiskInode.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Path.o: Ext2/Path.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Kernel.o: VirtualProcess/Kernel.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

User.o: VirtualProcess/User.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

VirtualProcess.o: VirtualProcess/VirtualProcess.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

BufferCache.o: BufferCache/BufferCache.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Buf.o: BufferCache/Buf.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

BufferLruList.o: BufferCache/BufferLruList.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

DiskDriver.o: DiskDriver/DiskDriver.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

DiskBlock.o: DiskDriver/DiskBlock.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Bitmap.o: Utils/Bitmap.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

Logcat.o: Utils/Logcat.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

TimeHelper.o: Utils/TimeHelper.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: user_fs

clean:
	rm -f *.o user_fs include/*.gch

.PHONY: all clean
