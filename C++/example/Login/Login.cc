#include "Login.h"
#include "FileUtil.h"
#include "ConnectionPool.h"
#include <string>
#include <algorithm>
#include <iostream>

Login::Login() {
    // 初始化数据库连接池
    connectionPool_ = ConnectionPool::getConnectionPool();
    // 确保user表存在
    ensureUserTableExists();
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
            "password VARCHAR(100) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
        
        if (!conn->update(createTableSql)) {
            std::cerr << "Failed to create users table. Error: " << std::endl;
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
            // 创建测试用户
            conn->update("INSERT INTO users (username, password) VALUES ('admin', '1234')");
            conn->update("INSERT INTO users (username, password) VALUES ('user1', 'password1')");
            conn->update("INSERT INTO users (username, password) VALUES ('animefan', 'otaku123')");
            std::cout << "Created test users" << std::endl;
        }
    }
}

bool Login::handleRequest(const HttpRequest& req, HttpResponse* resp) {
    // 处理登录页面请求
    if (req.path() == "/login" && req.method() == HttpRequest::kGet) {
        std::cout<<"login get"<<std::endl;
        std::string content = getLoginPage();
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
        std::cout<<"接收到登录请求: username = "<< username<<" password = "<<password<<std::endl;
        
        if (validateUser(username, password)) {
            // 登录成功
            std::cout<<"登录成功: "<< username<<std::endl;
            std::string content = getSuccessPage(username);
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setContentType("text/html");
            resp->setBody(content);
        } else {
            // 登录失败，显示错误信息
            std::cout<<"登录失败: username = "<< username<<" 用户名或密码错误"<<std::endl;
            std::string content = getLoginPage("用户名或密码错误，请重试");
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
        std::cout << "验证用户: username='" << username << "', password='" << password << "'" << std::endl;
        
        if (username.empty()) {
            std::cout << "警告: 用户名为空，验证失败" << std::endl;
            return false;
        }
        
        auto conn = connectionPool_->getConnection();
        if (!conn) {
            std::cerr << "错误: 无法从连接池获取数据库连接" << std::endl;
            return false;
        }
        
        // 注意：这里使用简单的字符串转义来防止SQL注入
        // 实际生产环境应使用真正的参数化查询
        std::string safeUsername = username;
        // 转义单引号
        size_t pos = 0;
        while ((pos = safeUsername.find("'", pos)) != std::string::npos) {
            safeUsername.replace(pos, 1, "''");
            pos += 2;
        }
        
        std::string query = "SELECT password FROM users WHERE username = '" + safeUsername + "'";
        std::cout << "执行SQL查询: " << query << std::endl;
        
        if (conn->query(query)) {
            if (conn->next()) {
                std::string storedPassword = conn->value(0);
                std::cout << "查询到用户密码: '" << storedPassword << "'" << std::endl;
                // 实际应用中应该使用密码哈希，这里为了简单直接比较
                bool result = (storedPassword == password);
                std::cout << "密码匹配结果: " << (result ? "成功" : "失败") << std::endl;
                return result;
            } else {
                std::cout << "用户不存在: " << username << std::endl;
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

std::string Login::getLoginPage(const std::string& errorMsg) {
    std::string content;
    if (FileUtil::readFile("www/login.html", content)) {
        // 替换错误信息占位符
        size_t pos = content.find("{{error_message}}");
        if (pos != std::string::npos) {
            content.replace(pos, 18, errorMsg);
        }
        return content;
    }
    // 如果模板文件读取失败，返回简单的备用页面
    return "<html><head><title>登录 - 次元AI助手</title></head>"
           "<body><h1>次元AI助手 - 登录</h1>"
           "<p style='color:red'>" + errorMsg + "</p>"
           "<form method='post' action='/login/doLogin'>"
           "用户名: <input type='text' name='username'><br>"
           "密码: <input type='password' name='password'><br>"
           "<input type='submit' value='登录'>"
           "</form></body></html>";
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

