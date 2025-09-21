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
    
    // 生成带有错误信息的登录页面
    std::string getLoginPage(const std::string& errorMsg = "");
    
    // 生成登录成功页面
    std::string getSuccessPage(const std::string& username);
    
    // 确保用户表存在
    void ensureUserTableExists();
    
    // 检查并创建测试用户
    void checkAndCreateTestUsers(std::shared_ptr<MysqlConn> conn);
    
    // 数据库连接池指针
    ConnectionPool* connectionPool_;

    
};

#endif // LOGIN_H
    