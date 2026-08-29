#include "fileutil.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

namespace fileutil
{

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDir(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool createDir(const std::string &path)
{
    if (exists(path)) return isDir(path);   // 已存在且是目录算成功
    // 0755: 属主读写执行，组/其他读执行
    return mkdir(path.c_str(), 0755) == 0;
}

std::vector<FileInfo> listDir(const std::string &path)
{
    std::vector<FileInfo> ret;
    DIR *dp = opendir(path.c_str());
    if (!dp) return ret;

    struct dirent *ent;
    while ((ent = readdir(dp)) != nullptr) {
        // 跳过 "." 与 ".."
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        FileInfo fi;
        memset(&fi, 0, sizeof(fi));
        strncpy(fi.caFileName, ent->d_name, sizeof(fi.caFileName) - 1);

        // 判断类型：拼接完整路径再 stat
        std::string full = join(path, ent->d_name);
        struct stat st;
        if (stat(full.c_str(), &st) == 0)
            fi.iFileType = S_ISDIR(st.st_mode) ? 0 : 1;   // 0 目录 / 1 文件
        else
            fi.iFileType = 1;   // stat 失败按文件处理
        ret.push_back(fi);
    }
    closedir(dp);
    return ret;
}

bool deleteDirRecursive(const std::string &path)
{
    DIR *dp = opendir(path.c_str());
    if (!dp) {
        // 不是目录则尝试按文件删
        if (!exists(path)) return false;
        return unlink(path.c_str()) == 0;
    }

    struct dirent *ent;
    bool ok = true;
    while ((ent = readdir(dp)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        std::string full = join(path, ent->d_name);
        struct stat st;
        if (stat(full.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (!deleteDirRecursive(full)) ok = false;
            } else {
                if (unlink(full.c_str()) != 0) ok = false;
            }
        }
    }
    closedir(dp);
    if (rmdir(path.c_str()) != 0) ok = false;
    return ok;
}

bool deleteFile(const std::string &path)
{
    return unlink(path.c_str()) == 0;
}

bool renameFile(const std::string &oldPath, const std::string &newPath)
{
    return rename(oldPath.c_str(), newPath.c_str()) == 0;
}

bool copyFile(const std::string &src, const std::string &dst)
{
    int in = open(src.c_str(), O_RDONLY);
    if (in < 0) return false;
    int out = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { close(in); return false; }

    char buf[4096];
    ssize_t n;
    bool ok = true;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t w = write(out, buf, n);
        if (w != n) { ok = false; break; }
    }
    if (n < 0) ok = false;
    close(in);
    close(out);
    return ok;
}

bool copyDirRecursive(const std::string &srcPath, const std::string &dstPath)
{
    if (!createDir(dstPath)) return false;

    DIR *dp = opendir(srcPath.c_str());
    if (!dp) return false;

    struct dirent *ent;
    bool ok = true;
    while ((ent = readdir(dp)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        std::string srcTmp = join(srcPath, ent->d_name);
        std::string desTmp = join(dstPath, ent->d_name);   // 修复版：desTmp 用 dstPath 拼
        struct stat st;
        if (stat(srcTmp.c_str(), &st) != 0) { ok = false; continue; }
        if (S_ISDIR(st.st_mode)) {
            if (!copyDirRecursive(srcTmp, desTmp)) ok = false;
        } else {
            if (!copyFile(srcTmp, desTmp)) ok = false;
        }
    }
    closedir(dp);
    return ok;
}

long long getFileSize(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;
    return (long long)st.st_size;
}

std::string join(const std::string &dir, const std::string &name)
{
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

} // namespace fileutil
