#ifndef FILEUTIL_H
#define FILEUTIL_H

// ============================================================
// 文件操作工具层（Linux 版，替代 Qt 的 QDir/QFile）
// 对应移植自: TcpServer/mytcpsocket.cpp 中的文件操作逻辑
// 统一 UTF-8 路径处理（与 Qt5 toStdString 输出一致）
// ============================================================
#include <string>
#include <vector>
#include "protocol.h"   // FileInfo 结构

namespace fileutil
{
    // 创建目录（如不存在）
    bool createDir(const std::string &path);

    // 判断路径是否存在 / 是否是目录
    bool exists(const std::string &path);
    bool isDir(const std::string &path);

    // 列出目录内容，填充 FileInfo（文件名 + 类型：0目录 / 1文件）
    // 跳过 "." 与 ".."；路径不存在时返回空
    std::vector<FileInfo> listDir(const std::string &path);

    // 递归删除目录（对应 QDir::removeRecursively）
    bool deleteDirRecursive(const std::string &path);

    // 删除单个文件
    bool deleteFile(const std::string &path);

    // 重命名文件/目录（对应 QFile::rename / QDir::rename）
    bool renameFile(const std::string &oldPath, const std::string &newPath);

    // 复制单个文件
    bool copyFile(const std::string &src, const std::string &dst);

    // 递归复制目录（对应修复版 copyDir：srcTmp=srcPath+name, dstTmp=dstPath+name）
    bool copyDirRecursive(const std::string &srcPath, const std::string &dstPath);

    // 获取文件大小（-1 表示失败）
    long long getFileSize(const std::string &path);

    // 路径拼接：dir + "/" + name
    std::string join(const std::string &dir, const std::string &name);
}

#endif // FILEUTIL_H
