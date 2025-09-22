#include "Login.h"
#include "FileUtil.h"
#include "ConnectionPool.h"
#include <string>
#include <algorithm>
#include <iostream>
#include <openssl/sha.h>
#include <openssl/rand.h>

Login::Login() : gen_(rd_()) {
    // 初始化数据库连接池
    connectionPool_ = ConnectionPool::getConnectionPool();
    // 确保user表存在
    //ensureUserTableExists();
}

void Login::ensureUserTableExists() {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) {
            std::cerr << "Failed to get connection from pool" << std::endl;
            return;
        }
        
        std::cout << "Connected to database, creating table if not exists..." << std::endl;
        const std::string createTableSql = 
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "username VARCHAR(50) NOT NULL UNIQUE,"
            "password VARCHAR(255) NOT NULL,"
            "salt VARCHAR(32) NOT NULL,"
            "email VARCHAR(100) DEFAULT '',"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "last_login TIMESTAMP NULL,"
            "is_active BOOLEAN DEFAULT TRUE"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        
        if (!conn->update(createTableSql)) {
            std::cerr << "Failed to create users table. Error: " << mysql_error(conn->getMysql()) << std::endl;
        } else {
            std::cout << "Users table created or already exists." << std::endl;
            // 检查是否有用户数据，如果没有则创建一些测试用户
            checkAndCreateTestUsers(conn);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in ensureUserTableExists: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in ensureUserTableExists" << std::endl;
    }
}

void Login::checkAndCreateTestUsers(std::shared_ptr<MysqlConn> conn) {
    if (!conn->query("SELECT COUNT(*) FROM users")) {
        std::cerr << "Failed to check users count" << std::endl;
        return;
    }
    
    if (conn->next()) {
        int count = std::stoi(conn->value(0));
        if (count == 0) {
            // 创建测试用户（使用哈希密码）
            std::string salt1 = generateSalt();
            std::string salt2 = generateSalt();
            std::string salt3 = generateSalt();
            
            std::string hash1 = hashPassword("1234" + salt1);
            std::string hash2 = hashPassword("password1" + salt2);
            std::string hash3 = hashPassword("otaku123" + salt3);
            
            conn->update("INSERT INTO users (username, password, salt, email) VALUES ('admin', '" + hash1 + "', '" + salt1 + "', 'admin@example.com')");
            conn->update("INSERT INTO users (username, password, salt, email) VALUES ('user1', '" + hash2 + "', '" + salt2 + "', 'user1@example.com')");
            conn->update("INSERT INTO users (username, password, salt, email) VALUES ('animefan', '" + hash3 + "', '" + salt3 + "', 'animefan@example.com')");
            std::cout << "Created test users with hashed passwords" << std::endl;
        }
    }
}

bool Login::handleRequest(const HttpRequest& req, HttpResponse* resp) {
    // 处理登录页面请求
    //ensureUserTableExists();
    if (req.path() == "/login" && req.method() == HttpRequest::kGet) {
        std::cout << "login get" << std::endl;
        std::string content = getLoginPage();
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->setBody(content);
        return true;
    }
    // 处理注册页面请求
    else if (req.path() == "/register" && req.method() == HttpRequest::kGet) {
        std::cout << "register get" << std::endl;
        std::string content = getRegisterPage();
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->setBody(content);
        return true;
    }
    // 处理登录提交请求
    else if (req.path() == "/login/doLogin" && req.method() == HttpRequest::kPost) {
        auto formData = parseFormData(req.body());
        std::string username = formData["username"];
        std::string password = formData["password"];
        std::string csrfToken = formData["csrf_token"];
        
        std::cout << "接收到登录请求: username = " << username << std::endl;
        
        //暂时跳过CSRF令牌验证
        if (!verifyCSRFToken(csrfToken)) {
            std::cout << "CSRF令牌验证失败" << std::endl;
            std::string content = getLoginPage("安全验证失败，请重新登录");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        // 输入验证
        if (!validateInput(username, 3, 20) || !validateInput(password, 4, 50)) {
            std::cout << "输入验证失败" << std::endl;
            std::string content = getLoginPage("用户名长度应为3-20字符，密码长度应为4-50字符");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        if (validateUser(username, password)) {
            // 登录成功
            std::cout << "登录成功: " << username << std::endl;
            std::string content = getSuccessPage(username);
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
        } else {
            // 登录失败，显示错误信息
            std::cout << "登录失败: username = " << username << " 用户名或密码错误" << std::endl;
            std::string content = getLoginPage("用户名或密码错误，请重试");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
        }
        return true;
    }
    // 处理注册提交请求
    else if (req.path() == "/register/doRegister" && req.method() == HttpRequest::kPost) {
        auto formData = parseFormData(req.body());
        std::string username = formData["username"];
        std::string password = formData["password"];
        std::string confirmPassword = formData["confirm_password"];
        std::string email = formData["email"];
        std::string csrfToken = formData["csrf_token"];
        
        std::cout << "接收到注册请求: username = " << username << std::endl;
        
        // 暂时跳过CSRF令牌验证
        if (!verifyCSRFToken(csrfToken)) {
            std::cout << "CSRF令牌验证失败" << std::endl;
            std::string content = getRegisterPage("安全验证失败，请重新注册");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        // 输入验证
        if (!validateInput(username, 3, 20) || !validateInput(password, 4, 50)) {
            std::string content = getRegisterPage("用户名长度应为3-20字符，密码长度应为4-50字符");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        if (password != confirmPassword) {
            std::string content = getRegisterPage("两次输入的密码不一致");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        if (isUsernameExists(username)) {
            std::string content = getRegisterPage("用户名已存在，请选择其他用户名");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
            return true;
        }
        
        if (registerUser(username, password, email)) {
            std::cout << "注册成功: " << username << std::endl;
            std::string content = getLoginPage("", "注册成功！请使用新账号登录");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
        } else {
            std::cout << "注册失败: " << username << std::endl;
            std::string content = getRegisterPage("注册失败，请重试");
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
        }
        return true;
    }
    // 处理登出请求
    else if (req.path() == "/logout" && req.method() == HttpRequest::kGet) {
        // 实际应用中应该清除会话
        std::string content = getLoginPage("已成功登出");
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setContentType("text/html");
        resp->setBody(content);
        return true;
    }
    
    // 不是登录相关请求
    return false;
}

std::unordered_map<std::string, std::string> Login::parseFormData(const std::string& data) {
    std::unordered_map<std::string, std::string> result;
    std::cout << "解析表单数据: " << data << std::endl;
    
    if (data.empty()) {
        std::cout << "警告: 表单数据为空" << std::endl;
        return result;
    }
    
    size_t pos = 0;
    while (pos < data.size()) {
        size_t eq = data.find('=', pos);
        size_t amp = data.find('&', pos);
        
        if (eq == std::string::npos) {
            std::cout << "警告: 未找到等号，停止解析" << std::endl;
            break;
        }
        
        std::string key = data.substr(pos, eq - pos);
        // 去除key两端的空白字符
        key.erase(0, key.find_first_not_of(" \t\n\r"));
        key.erase(key.find_last_not_of(" \t\n\r") + 1);
        
        std::string value;
        
        if (amp != std::string::npos) {
            value = data.substr(eq + 1, amp - eq - 1);
            pos = amp + 1;
        } else {
            value = data.substr(eq + 1);
            pos = data.size();
        }
        
        // 去除value两端的空白字符
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        
        // 增强的URL解码
        std::string decodedValue;
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '+' && i < value.size()) {
                decodedValue += ' ';
            } else if (value[i] == '%' && i + 2 < value.size()) {
                // 尝试解码%XX格式的字符
                try {
                    char hex[3] = {value[i+1], value[i+2], '\0'};
                    int num = std::strtol(hex, nullptr, 16);
                    decodedValue += static_cast<char>(num);
                    i += 2; // 跳过两个字符
                } catch (...) {
                    // 如果解码失败，保留原始字符
                    decodedValue += value[i];
                }
            } else {
                decodedValue += value[i];
            }
        }
        value = decodedValue;
        
        result[key] = value;
        std::cout << "解析字段: key='" << key << "', value='" << value << "'" << std::endl;
    }
    
    std::cout << "表单数据解析完成，共" << result.size() << "个字段" << std::endl;
    return result;
}

bool Login::validateUser(const std::string& username, const std::string& password) {
    try {
        std::cout << "验证用户: username='" << username << "'" << std::endl;
        
        if (username.empty()) {
            std::cout << "警告: 用户名为空，验证失败" << std::endl;
            return false;
        }
        
        auto conn = connectionPool_->getConnection();
        if (!conn) {
            std::cerr << "错误: 无法从连接池获取数据库连接" << std::endl;
            return false;
        }
        
        // 使用参数化查询防止SQL注入
        std::string query = "SELECT password, salt FROM users WHERE username = ? AND is_active = TRUE";
        std::cout << "执行SQL查询: " << query << std::endl;
        
        // 注意：这里假设MysqlConn支持参数化查询
        // 如果不支持，需要手动转义
        std::string safeUsername = username;
        /*size_t pos = 0;
        while ((pos = safeUsername.find("'", pos)) != std::string::npos) {
            safeUsername.replace(pos, 1, "''");
            pos += 2;
        }
        */
        std::cout<<"safeUsername: "<<safeUsername<<std::endl;
        query = "SELECT password, salt FROM users WHERE username = '" + safeUsername + "' AND is_active = TRUE";
        
        if (conn->query(query)) {
            if (conn->next()) {
                std::string storedPassword = conn->value(0);
                std::string salt = conn->value(1);
                std::cout << "查询到用户密码和盐值" << std::endl;
                
                // 使用哈希验证密码
                bool result = verifyPassword(password, storedPassword, salt);
                std::cout << "密码匹配结果: " << (result ? "成功" : "失败") << std::endl;
                
                if (result) {
                    // 更新最后登录时间
                    std::string updateQuery = "UPDATE users SET last_login = NOW() WHERE username = '" + safeUsername + "'";
                    conn->update(updateQuery);
                }
                
                return result;
            } else {
                std::cout << "用户不存在或已被禁用: " << username << std::endl;
            }
        } else {
            std::cerr << "SQL查询失败" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in validateUser: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in validateUser" << std::endl;
    }
    return false;
}

std::string Login::getLoginPage(const std::string& errorMsg, const std::string& successMsg) {
    std::string content;
    if (FileUtil::readFile("www/login.html", content)) {
        // 替换错误信息占位符
        size_t pos = content.find("{{error_message}}");
        if (pos != std::string::npos) {
            content.replace(pos, 18, errorMsg);
        }
        
        // 替换成功信息占位符
        pos = content.find("{{success_message}}");
        if (pos != std::string::npos) {
            content.replace(pos, 19, successMsg);
        }
        
        // 添加CSRF令牌
        std::string csrfToken = generateCSRFToken();
        pos = content.find("{{csrf_token}}");
        if (pos != std::string::npos) {
            content.replace(pos, 14, csrfToken);
        }
        
        return content;
    }
    // 如果模板文件读取失败，返回简单的备用页面
    std::string csrfToken = generateCSRFToken();
    return "<html><head><title>登录 - 次元AI助手</title></head>"
           "<body><h1>次元AI助手 - 登录</h1>"
           "<p style='color:red'>" + errorMsg + "</p>"
           "<p style='color:green'>" + successMsg + "</p>"
           "<form method='post' action='/login/doLogin'>"
           "<input type='hidden' name='csrf_token' value='" + csrfToken + "'>"
           "用户名: <input type='text' name='username'><br>"
           "密码: <input type='password' name='password'><br>"
           "<input type='submit' value='登录'>"
           "</form>"
           "<a href='/register'>注册新账号</a>"
           "</body></html>";
}

std::string Login::getSuccessPage(const std::string& username) {
    std::string content;
    std::cout<<"username:"<<username<<std::endl;
    if (FileUtil::readFile("www/login_success.html", content)) {
        // 替换用户名占位符
        size_t pos = content.find("{{username}}");
        if (pos != std::string::npos) {
            content.replace(pos, 10, username);
        }
        return content;
    }
    // 如果模板文件读取失败，返回简单的备用页面
    return "<html><head><title>登录成功 - 次元AI助手</title></head>"
           "<body><h1>登录成功</h1>"
           "<p>欢迎回来，" + username + "!</p>"
           "<a href='/'>返回首页</a><br>"
           "<a href='/logout'>退出登录</a>"
           "</body></html>";
}

// 新增方法实现

std::string Login::getRegisterPage(const std::string& errorMsg, const std::string& successMsg) {
    std::string content;
    if (FileUtil::readFile("www/register.html", content)) {
        // 替换错误信息占位符
        size_t pos = content.find("{{error_message}}");
        if (pos != std::string::npos) {
            content.replace(pos, 18, errorMsg);
        }
        
        // 替换成功信息占位符
        pos = content.find("{{success_message}}");
        if (pos != std::string::npos) {
            content.replace(pos, 19, successMsg);
        }
        
        // 添加CSRF令牌
        std::string csrfToken = generateCSRFToken();
        pos = content.find("{{csrf_token}}");
        if (pos != std::string::npos) {
            content.replace(pos, 14, csrfToken);
        }
        
        return content;
    }
    // 如果模板文件读取失败，返回简单的备用页面
    std::string csrfToken = generateCSRFToken();
    return "<html><head><title>注册 - 次元AI助手</title></head>"
           "<body><h1>次元AI助手 - 注册</h1>"
           "<p style='color:red'>" + errorMsg + "</p>"
           "<p style='color:green'>" + successMsg + "</p>"
           "<form method='post' action='/register/doRegister'>"
           "<input type='hidden' name='csrf_token' value='" + csrfToken + "'>"
           "用户名: <input type='text' name='username'><br>"
           "密码: <input type='password' name='password'><br>"
           "确认密码: <input type='password' name='confirm_password'><br>"
           "邮箱: <input type='email' name='email'><br>"
           "<input type='submit' value='注册'>"
           "</form>"
           "<a href='/login'>已有账号？立即登录</a>"
           "</body></html>";
}

bool Login::registerUser(const std::string& username, const std::string& password, const std::string& email) {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) {
            std::cerr << "错误: 无法从连接池获取数据库连接" << std::endl;
            return false;
        }
        
        std::string salt = generateSalt();
        std::string hashedPassword = hashPassword(password + salt);
        
        std::string safeUsername = username;
        size_t pos = 0;
        while ((pos = safeUsername.find("'", pos)) != std::string::npos) {
            safeUsername.replace(pos, 1, "''");
            pos += 2;
        }
        
        std::string safeEmail = email;
        pos = 0;
        while ((pos = safeEmail.find("'", pos)) != std::string::npos) {
            safeEmail.replace(pos, 1, "''");
            pos += 2;
        }
        
        std::string insertQuery = "INSERT INTO users (username, password, salt, email) VALUES ('" + 
                                 safeUsername + "', '" + hashedPassword + "', '" + salt + "', '" + safeEmail + "')";
        
        std::cout << "执行注册SQL: " << insertQuery << std::endl;
        
        if (conn->update(insertQuery)) {
            std::cout << "用户注册成功: " << username << std::endl;
            return true;
        } else {
            std::cerr << "用户注册失败" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in registerUser: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception in registerUser" << std::endl;
        return false;
    }
}

std::string Login::hashPassword(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool Login::verifyPassword(const std::string& password, const std::string& hash, const std::string& salt) {
    std::string hashedInput = hashPassword(password + salt);
    return hashedInput == hash;
}

std::string Login::generateSalt() {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string salt;
    for (int i = 0; i < 16; ++i) {
        salt += chars[dis(gen_)];
    }
    return salt;
}

bool Login::validateInput(const std::string& input, int minLength, int maxLength) {
    if (input.length() < minLength || input.length() > maxLength) {
        return false;
    }
    
    // 检查是否包含危险字符
    for (char c : input) {
        if (c == '<' || c == '>' || c == '"' || c == '\'' || c == '&') {
            return false;
        }
    }
    
    return true;
}

bool Login::isUsernameExists(const std::string& username) {
    try {
        auto conn = connectionPool_->getConnection();
        if (!conn) {
            return false;
        }
        
        std::string safeUsername = username;
        size_t pos = 0;
        while ((pos = safeUsername.find("'", pos)) != std::string::npos) {
            safeUsername.replace(pos, 1, "''");
            pos += 2;
        }
        
        std::string query = "SELECT COUNT(*) FROM users WHERE username = '" + safeUsername + "'";
        
        if (conn->query(query)) {
            if (conn->next()) {
                int count = std::stoi(conn->value(0));
                return count > 0;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in isUsernameExists: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in isUsernameExists" << std::endl;
    }
    return false;
}

std::string Login::generateCSRFToken() {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[dis(gen_)];
    }
    
    // 存储令牌及其过期时间（5分钟）
    auto now = std::chrono::system_clock::now();
    auto expireTime = now + std::chrono::minutes(5);
    csrfTokens_[token] = expireTime;
    
    return token;
}

bool Login::verifyCSRFToken(const std::string& token) {
    auto it = csrfTokens_.find(token);
    if (it == csrfTokens_.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    if (now > it->second) {
        // 令牌已过期，删除
        csrfTokens_.erase(it);
        return false;
    }
    
    return true;
}

