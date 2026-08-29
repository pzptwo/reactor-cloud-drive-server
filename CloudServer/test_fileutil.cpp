// 文件操作工具层单元测试：
// 覆盖 建目录/列目录/建文件/复制/重命名/删除文件/递归复制/递归删除/文件大小/路径拼接
#include "fileutil.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using namespace fileutil;

// 创建文件并写入内容
static bool writeFile(const std::string &path, const char *content)
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    ssize_t n = write(fd, content, strlen(content));
    close(fd);
    return n == (ssize_t)strlen(content);
}

int main()
{
    const std::string base = "test_fileutil_dir";

    // 清理历史残留
    if (exists(base)) assert(deleteDirRecursive(base));

    // ---- 建目录 ----
    assert(createDir(base));
    assert(exists(base) && isDir(base));
    assert(createDir(base));   // 已存在仍返回 true（兼容重复创建）

    // ---- 列目录（空）----
    auto empty = listDir(base);
    assert(empty.size() == 0);

    // ---- 建文件 ----
    std::string f1 = join(base, "a.txt");
    std::string f2 = join(base, "b.txt");
    assert(writeFile(f1, "hello fileutil"));
    assert(writeFile(f2, "second file"));

    // ---- 列目录（2 个文件，类型=1）----
    auto list = listDir(base);
    assert(list.size() == 2);
    for (auto &fi : list) {
        assert(fi.iFileType == 1);
        assert(strlen(fi.caFileName) > 0);
    }

    // ---- 子目录 ----
    std::string sub = join(base, "sub");
    assert(createDir(sub));
    assert(writeFile(join(sub, "inner.txt"), "inner"));

    // ---- 重命名 ----
    std::string f1_new = join(base, "a_renamed.txt");
    assert(renameFile(f1, f1_new));
    assert(!exists(f1) && exists(f1_new));

    // ---- 复制文件 ----
    std::string f2_copy = join(base, "b_copy.txt");
    assert(copyFile(f2, f2_copy));
    assert(getFileSize(f2) == getFileSize(f2_copy));
    assert(getFileSize(f2) == (long long)strlen("second file"));

    // ---- 递归复制目录（含文件+子目录）----
    std::string base2 = base + "_copy";
    assert(copyDirRecursive(base, base2));
    assert(exists(join(base2, "sub")) && isDir(join(base2, "sub")));
    assert(exists(join(join(base2, "sub"), "inner.txt")));
    assert(exists(join(base2, "a_renamed.txt")));

    // ---- 递归删除 ----
    assert(deleteDirRecursive(base2));
    assert(!exists(base2));
    assert(deleteDirRecursive(base));
    assert(!exists(base));

    // ---- 路径拼接 ----
    assert(join("a", "b") == "a/b");
    assert(join("a/", "b") == "a/b");

    // ---- 删除文件（不存在返回 false）----
    assert(!deleteFile("/no/such/file/xyz"));

    printf("文件工具层测试全部通过\n");
    return 0;
}
