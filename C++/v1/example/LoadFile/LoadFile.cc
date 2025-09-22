#include "LoadFile.h"
#include "../Login/FileUtil.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

LoadFile::LoadFile() {
    storageRoot_ = "/home/oym/muduo/network/example/LoadFile/storage";
    ensureDir(storageRoot_);
    connectionPool_ = ConnectionPool::getConnectionPool();
    // 确保 files 表存在
    // 忽略失败以不阻塞上传流程
    ensureFilesTableExists();
}

bool LoadFile::handleRequest(const HttpRequest& req, HttpResponse* resp) {
    const std::string& p = req.path();
    if (p == "/cloud" && req.method() == HttpRequest::kGet) {
        std::string content;
        if (FileUtil::readFile("www/cloud.html", content)) {
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("text/html");
            resp->setBody(content);
        } else {
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
        return true;
    }
    if (p == "/cloud/upload" && req.method() == HttpRequest::kPost) return handleSimpleUpload(req, resp);
    if (p == "/cloud/instant" && req.method() == HttpRequest::kPost) return handleInstantUpload(req, resp);
    if (p == "/cloud/chunk/init" && req.method() == HttpRequest::kPost) return handleChunkInit(req, resp);
    if (p == "/cloud/chunk/upload" && req.method() == HttpRequest::kPost) return handleChunkUpload(req, resp);
    if (p == "/cloud/chunk/status" && req.method() == HttpRequest::kPost) return handleChunkStatus(req, resp);
    if (p == "/cloud/chunk/complete" && req.method() == HttpRequest::kPost) return handleChunkComplete(req, resp);
    return false;
}

std::unordered_map<std::string, std::string> LoadFile::parseFormUrlEncoded(const std::string& body) {
    std::unordered_map<std::string, std::string> kv;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t eq = body.find('=', pos);
        size_t amp = body.find('&', pos);
        if (eq == std::string::npos) break;
        std::string key = body.substr(pos, eq - pos);
        std::string val;
        if (amp == std::string::npos) {
            val = body.substr(eq + 1);
            pos = body.size();
        } else {
            val = body.substr(eq + 1, amp - eq - 1);
            pos = amp + 1;
        }
        kv[key] = val;
    }
    return kv;
}

bool LoadFile::ensureDir(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) return S_ISDIR(st.st_mode);
    return ::mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

bool LoadFile::fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string LoadFile::joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

bool LoadFile::ensureFilesTableExists() {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) return false;
        std::string sql =
            "CREATE TABLE IF NOT EXISTS files ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "hash VARCHAR(128) NOT NULL UNIQUE,"
            "name VARCHAR(255) NOT NULL,"
            "size BIGINT DEFAULT 0,"
            "path VARCHAR(512) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        return conn->update(sql);
    } catch (...) {
        return false;
    }
}

bool LoadFile::dbHasHash(const std::string& hash, std::string* outPath, long long* outSize) {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) return false;
        std::string safe = hash;
        size_t pos = 0; while ((pos = safe.find("'", pos)) != std::string::npos) { safe.replace(pos, 1, "''"); pos += 2; }
        std::string q = "SELECT path,size FROM files WHERE hash='" + safe + "'";
        if (conn->query(q) && conn->next()) {
            if (outPath) *outPath = conn->value(0);
            if (outSize) *outSize = std::stoll(conn->value(1));
            return true;
        }
    } catch (...) {}
    return false;
}

bool LoadFile::dbUpsertFile(const std::string& hash, const std::string& name, long long size, const std::string& path) {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) return false;
        auto esc = [](std::string s){ size_t p=0; while((p=s.find("'",p))!=std::string::npos){ s.replace(p,1,"''"); p+=2;} return s; };
        std::string sh = esc(hash), sn = esc(name), sp = esc(path);
        std::string sql = "INSERT INTO files(hash,name,size,path) VALUES('" + sh + "','" + sn + "'," + std::to_string(size) + ", '" + sp + "') "
                          "ON DUPLICATE KEY UPDATE name='" + sn + "', size=" + std::to_string(size) + ", path='" + sp + "'";
        return conn->update(sql);
    } catch (...) { return false; }
}

bool LoadFile::writeFile(const std::string& path, const char* data, size_t len, bool append) {
    FILE* f = fopen(path.c_str(), append ? "ab" : "wb");
    if (!f) return false;
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    return n == len;
}

bool LoadFile::moveFile(const std::string& from, const std::string& to) {
    return ::rename(from.c_str(), to.c_str()) == 0;
}

bool LoadFile::handleSimpleUpload(const HttpRequest& req, HttpResponse* resp) {
    // Expect headers: X-Filename, X-File-Hash (optional)
    std::string filename = req.getHeader("X-Filename");
    if (filename.empty()) filename = "upload.bin";
    std::string fileHash = req.getHeader("X-File-Hash");
    std::string target = fileHash.empty() ? joinPath(storageRoot_, filename) : joinPath(storageRoot_, fileHash);
    if (!writeFile(target, req.body().data(), req.body().size(), false)) {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false,\"msg\":\"write failed\"}");
        return true;
    }
    if (!fileHash.empty()) {
        dbUpsertFile(fileHash, filename, (long long)req.body().size(), target);
        std::string named = joinPath(storageRoot_, filename);
        if (!fileExists(named)) {
            ::link(target.c_str(), named.c_str());
        }
    }
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody("{\"ok\":true}");
    return true;
}

bool LoadFile::handleInstantUpload(const HttpRequest& req, HttpResponse* resp) {
    auto kv = parseFormUrlEncoded(req.body());
    std::string hash = kv["hash"]; // client-side precomputed sha256/md5 string
    std::string name = kv["name"]; // desired filename
    if (hash.empty() || name.empty()) {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false,\"msg\":\"missing hash/name\"}");
        return true;
    }
    std::string dbPath; long long dbSize = 0;
    bool inDb = dbHasHash(hash, &dbPath, &dbSize);
    std::string blob = inDb ? dbPath : joinPath(storageRoot_, hash);
    if (!fileExists(blob)) {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false,\"instant\":false}");
        return true;
    }
    std::string link = joinPath(storageRoot_, name);
    if (!moveFile(blob, link)) {
        // fallback: hard link
        ::link(blob.c_str(), link.c_str());
    }
    dbUpsertFile(hash, name, dbSize, link);
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody("{\"ok\":true,\"instant\":true}");
    return true;
}

bool LoadFile::handleChunkInit(const HttpRequest& req, HttpResponse* resp) {
    auto kv = parseFormUrlEncoded(req.body());
    std::string uploadId = kv["uploadId"]; // client provided id (e.g., hash)
    std::string filename = kv["name"]; 
    if (uploadId.empty() || filename.empty()) {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false,\"msg\":\"missing uploadId/name\"}");
        return true;
    }
    std::string dir = joinPath(storageRoot_, uploadId);
    ensureDir(dir);
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody("{\"ok\":true}");
    return true;
}

bool LoadFile::handleChunkUpload(const HttpRequest& req, HttpResponse* resp) {
    // headers: X-UploadId, X-Chunk-Index
    std::string uploadId = req.getHeader("X-UploadId");
    std::string idxStr = req.getHeader("X-Chunk-Index");
    if (uploadId.empty() || idxStr.empty()) {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false,\"msg\":\"missing uploadId/chunk index\"}");
        return true;
    }
    std::string dir = joinPath(storageRoot_, uploadId);
    ensureDir(dir);
    std::string chunkPath = joinPath(dir, std::string("chunk_") + idxStr);
    if (!writeFile(chunkPath, req.body().data(), req.body().size(), false)) {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false}");
        return true;
    }
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody("{\"ok\":true}");
    return true;
}

bool LoadFile::handleChunkStatus(const HttpRequest& req, HttpResponse* resp) {
    auto kv = parseFormUrlEncoded(req.body());
    std::string uploadId = kv["uploadId"];
    int maxCheck = 10000; // simple scan
    int count = 0;
    if (!uploadId.empty()) {
        std::string dir = joinPath(storageRoot_, uploadId);
        for (int i = 0; i < maxCheck; ++i) {
            std::string p = joinPath(dir, std::string("chunk_") + std::to_string(i));
            if (fileExists(p)) ++count; else break;
        }
    }
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(std::string("{\"ok\":true,\"count\":") + std::to_string(count) + "}");
    return true;
}

bool LoadFile::handleChunkComplete(const HttpRequest& req, HttpResponse* resp) {
    auto kv = parseFormUrlEncoded(req.body());
    std::string uploadId = kv["uploadId"];
    std::string filename = kv["name"];
    std::string dir = joinPath(storageRoot_, uploadId);
    std::string target = joinPath(storageRoot_, filename);
    // merge
    FILE* out = fopen(target.c_str(), "wb");
    if (!out) {
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setContentType("application/json");
        resp->setBody("{\"ok\":false}");
        return true;
    }
    for (int i = 0;; ++i) {
        std::string chunkPath = joinPath(dir, std::string("chunk_") + std::to_string(i));
        if (!fileExists(chunkPath)) break;
        FILE* in = fopen(chunkPath.c_str(), "rb");
        if (!in) break;
        char buf[1 << 20];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
            fwrite(buf, 1, n, out);
        }
        fclose(in);
        ::unlink(chunkPath.c_str());
    }
    fclose(out);
    // remove dir best-effort
    ::rmdir(dir.c_str());
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody("{\"ok\":true}");
    return true;
}


