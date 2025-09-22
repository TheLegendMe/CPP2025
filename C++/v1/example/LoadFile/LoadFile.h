#ifndef LOAD_FILE_H
#define LOAD_FILE_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ConnectionPool.h"
#include <string>
#include <unordered_map>

class LoadFile {
public:
    LoadFile();
    ~LoadFile() = default;

    // Return true if handled
    bool handleRequest(const HttpRequest& req, HttpResponse* resp);

private:
    // Basic upload: whole file (body is the file content), headers carry filename and sha256
    bool handleSimpleUpload(const HttpRequest& req, HttpResponse* resp);
    // Instant upload by hash
    bool handleInstantUpload(const HttpRequest& req, HttpResponse* resp);
    // Chunked
    bool handleChunkInit(const HttpRequest& req, HttpResponse* resp);
    bool handleChunkUpload(const HttpRequest& req, HttpResponse* resp);
    bool handleChunkStatus(const HttpRequest& req, HttpResponse* resp);
    bool handleChunkComplete(const HttpRequest& req, HttpResponse* resp);

    // Helpers
    std::unordered_map<std::string, std::string> parseFormUrlEncoded(const std::string& body);
    bool ensureDir(const std::string& path);
    bool fileExists(const std::string& path);
    bool writeFile(const std::string& path, const char* data, size_t len, bool append = false);
    bool moveFile(const std::string& from, const std::string& to);
    std::string joinPath(const std::string& a, const std::string& b);
    bool ensureFilesTableExists();
    bool dbHasHash(const std::string& hash, std::string* outPath, long long* outSize);
    bool dbUpsertFile(const std::string& hash, const std::string& name, long long size, const std::string& path);

    std::string storageRoot_;     // e.g. example/LoadFile/storage
    ConnectionPool* connectionPool_;
};

#endif // LOAD_FILE_H


