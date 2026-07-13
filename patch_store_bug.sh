#!/bin/bash
# 应用store/read/write修复补丁
# 在VMware虚拟机中执行: bash patch_store_bug.sh

cd ~/Unix-FileSystem

# 备份原文件
cp VFS/VFS.cpp VFS/VFS.cpp.bak

# 修复1: VFS::read中长度计算+1越界
# length > i_size - f_offset + 1 --> length > i_size - f_offset
sed -i 's/length > p_inode->i_size - p_file->f_offset + 1)/length > p_inode->i_size - p_file->f_offset)/' VFS/VFS.cpp

# 修复2: VFS::read中块内剩余+1越界 (两个位置)
sed -i 's/DISK_BLOCK_SIZE - offsetInBlock + 1/DISK_BLOCK_SIZE - offsetInBlock/g' VFS/VFS.cpp

# 修复3: VFS::read中content指针丢失（复制后应前进）
sed -i 's/readByteCount = length;\n            \/\/修改offset/readByteCount = length;\n            content += length - readByteCount;\n            \/\/修改offset/' VFS/VFS.cpp

# 修复4: VFS::write入口添加防御性检查
if ! grep -q "ERROR!write: 无效参数" VFS/VFS.cpp; then
    # 在"User &u = VirtualProcess::Instance()->getUser();"前插入
    sed -i '/int VFS::write/,/User &u =/{/User &u =/i\    if (fd < 0 || content == NULL || length < 0)\n    {\n        Logcat::log("ERROR!write: 无效参数");\n        return -1;\n    }\n\n    int writeByteCount = 0;\n\n/;}' VFS/VFS.cpp
fi

# 重新编译
rm -f disk.img
make clean && make

echo "修复完成！运行 ./user_fs 测试"