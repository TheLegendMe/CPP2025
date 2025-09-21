#include "FileUtil.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

bool FileUtil::readFile(const std::string& filePath, std::string& content) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    
    return true;
}

bool FileUtil::fileExists(const std::string& filePath) {
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

size_t FileUtil::getFileSize(const std::string& filePath) {
    struct stat buffer;
    if (stat(filePath.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return 0;
}
    