#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <string>

class FileUtil {
public:
    // 读取文件内容到字符串
    static bool readFile(const std::string& filePath, std::string& content);
    
    // 检查文件是否存在
    static bool fileExists(const std::string& filePath);
    
    // 获取文件大小
    static size_t getFileSize(const std::string& filePath);
};

#endif // FILE_UTIL_H
    