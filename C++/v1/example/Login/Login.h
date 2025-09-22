#ifndef LOGIN_H
#define LOGIN_H

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "MysqlConn.h"
#include "ConnectionPool.h"

#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <string>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

class Login {
public:
    Login();
    ~Login() = default;
    
    // 处理登录相关请求，返回true表示已处理
    bool handleRequest(const HttpRequest& req, HttpResponse* resp);
    
private:
    // 解析表单数据
    std::unordered_map<std::string, std::string> parseFormData(const std::string& data);
    
    // 验证用户 credentials
    bool validateUser(const std::string& username, const std::string& password);
    
    // 用户注册
    bool registerUser(const std::string& username, const std::string& password, const std::string& email = "");
    
    // 生成带有错误信息的登录页面
    std::string getLoginPage(const std::string& errorMsg = "", const std::string& successMsg = "");
    
    // 生成注册页面
    std::string getRegisterPage(const std::string& errorMsg = "", const std::string& successMsg = "");
    
    // 生成登录成功页面
    std::string getSuccessPage(const std::string& username);
    
    // 确保用户表存在
    void ensureUserTableExists();
    
    // 检查并创建测试用户
    void checkAndCreateTestUsers(std::shared_ptr<MysqlConn> conn);
    
    // 密码哈希函数
    std::string hashPassword(const std::string& password);
    
    // 验证密码
    bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt);
    
    // 生成随机盐值
    std::string generateSalt();
    
    // 输入验证
    bool validateInput(const std::string& input, int minLength = 1, int maxLength = 100);
    
    // 检查用户名是否已存在
    bool isUsernameExists(const std::string& username);
    
    // 生成CSRF令牌
    std::string generateCSRFToken();
    
    // 验证CSRF令牌
    bool verifyCSRFToken(const std::string& token);
    
public:
    // 数据库连接池指针（临时设为public用于测试）
    ConnectionPool* connectionPool_;
    
    // 存储活跃的CSRF令牌
    std::unordered_map<std::string, std::chrono::system_clock::time_point> csrfTokens_;
    
    // 随机数生成器
    std::random_device rd_;
    std::mt19937 gen_;
};

#endif // LOGIN_H
    