#ifndef HTTP_HTTPCONTEXT_H
#define HTTP_HTTPCONTEXT_H

#include "HttpRequest.h"

class Buffer;

class HttpContext
{
public:
    // HTTP请求状态
    enum HttpRequestParseState {
        kExpectRequestLine,    // 期望解析请求行
        kExpectHeaders,        // 期望解析请求头
        kExpectBody,           // 期望解析固定长度请求体（Content-Length）
        kExpectChunkedBody,    // 期望解析分块编码的块长度
        kExpectChunkedData,    // 期望解析分块编码的块数据
        kExpectChunkedTrailer, // 期望解析分块编码的尾部（可选）
        kGotAll                // 解析完成
    };


    HttpContext()
        : state_(kExpectRequestLine)
    {
    }

    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    bool gotAll() const { return state_ == kGotAll; }

    // 重置HttpContext状态，异常安全
    void reset()
    {
        state_ = kExpectRequestLine;
        /**
         * 构造一个临时空HttpRequest对象，和当前的成员HttpRequest对象交换置空
         * 然后临时对象析构
         */
        HttpRequest dummy;
        request_.swap(dummy);
    }

    const HttpRequest& request() const { return request_; }

    HttpRequest& request() { return request_; }
    bool handleHeaderComplete();

private:
    bool processRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_;
    HttpRequest request_;
};

#endif // HTTP_HTTPCONTEXT_H