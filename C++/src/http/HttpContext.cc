#include "HttpContext.h"
#include "HttpRequest.h"
#include "Buffer.h"
#include "Timestamp.h"
#include <algorithm>
#include <cstdlib>
#include <cerrno>

using std::endl;
using std::cout;

// 解析请求行
bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');

    // 不是最后一个空格，并且成功获取了method并设置到request_
    if (space != end && request_.setMethod(start, space))
    {
        // 跳过空格
        start = space+1;
        // 继续寻找下一个空格
        space = std::find(start, end, ' ');
        if (space != end)
        {
            // 查看是否有请求参数
            const char* question = std::find(start, space, '?');
            if (question != space)
            {
                // 设置访问路径
                request_.setPath(start, question);
                // 设置访问变量
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space+1;
            // 获取最后的http版本
            succeed = (end-start == 8 && std::equal(start, end-1, "HTTP/1."));
            if (succeed)
            {
                if (*(end-1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end-1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }  
    return succeed;
}

// return false if any error
/*bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
    bool ok = false;
    bool hasMore = true;
    while (hasMore)
    {
        // 请求行状态
        if (state_ == kExpectRequestLine)
        {
            // 找到 \r\n 位置
            const char* crlf = buf->findCRLF();
            if (crlf)
            {
                // 从可读区读取请求行
                // [peek(), crlf + 2) 是一行
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    // readerIndex 向后移动位置直到 crlf + 2
                    buf->retrieveUntil(crlf + 2);
                    // 状态转移，接下来解析请求头
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        // 解析请求头
        /*else if (state_ == kExpectHeaders)
        {
            const char* crlf = buf->findCRLF();
            if (crlf)
            {
                // 找到 : 位置
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    // 添加状态首部
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else // colon == crlf 说明没有找到 : 了，直接返回 end
                {
                    // empty line, end of header
                    // FIXME:
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        else if (state_ == kExpectHeaders)
        {
            const char* crlf = buf->findCRLF();
            if (crlf)
            {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else 
                {
                    // 头部解析完成，处理POST请求体
                    if (request_.method() == HttpRequest::kPost) {
                        std::string lenStr = request_.getHeader("Content-Length");
                        if (!lenStr.empty()) {
                            size_t contentLen = std::stoul(lenStr);
                            state_ = kExpectBody;
                            
                            // 读取请求体并设置到request中
                            if (buf->readableBytes() >= contentLen) {
                                request_.setBody(std::string(buf->peek(), buf->peek() + contentLen));
                                buf->retrieve(contentLen);
                                state_ = kGotAll;
                                hasMore = false;
                            } else {
                                // 数据不足，等待更多数据
                                hasMore = false;
                            }
                        } else {
                            // 没有Content-Length，视为没有请求体
                            state_ = kGotAll;
                            hasMore = false;
                        }
                    } else {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        // 解析请求体，可以看到这里没有做出处理，只支持GET请求
        else if (state_ == kExpectBody)
        {
            // FIXME:
        }
    }
    return ok;
}
*/

// 新增：HTTP状态机状态定义（补充分块编码相关状态）
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;
    const size_t kMaxRequestSize = 1024 * 1024; // 1MB 最大请求限制（防内存溢出）

    while (hasMore) {
        switch (state_) {
            // 1. 解析请求行（方法、URL、协议版本）
            case kExpectRequestLine: {
                cout<<"kExpectRequestLine"<<endl;
                const char* crlf = buf->findCRLF();
                if (crlf) {
                    // 检查请求行长度是否超过限制
                    size_t requestLineLen = crlf - buf->peek();
                    if (requestLineLen > kMaxRequestSize / 2) {
                        ok = false;
                        hasMore = false;
                        break;
                    }

                    // 处理请求行
                    ok = processRequestLine(buf->peek(), crlf);
                    if (ok) {
                        request_.setReceiveTime(receiveTime);
                        buf->retrieveUntil(crlf + 2); // 消费请求行（包含\r\n）
                        state_ = kExpectHeaders;       // 切换到解析请求头状态
                    } else {
                        hasMore = false;
                    }
                } else {
                    // 未找到\r\n，等待更多数据（或请求行过长）
                    if (buf->readableBytes() > kMaxRequestSize / 2) {
                        ok = false;
                        hasMore = false;
                    }
                    hasMore = false;
                }
                break;
            }

            // 2. 解析请求头（键值对，支持折叠行）
            case kExpectHeaders: {
                cout<<"kExpectHeaders"<<endl;
                cout<<"kExpectHeaders | 缓冲区可读字节: "<<buf->readableBytes()<<endl;
                cout<<"缓冲区内容: "<<std::string(buf->peek(), buf->readableBytes())<<endl; // 打印完整数据
                const char* crlf = buf->findCRLF();
                if (crlf) {
                    const char* start = buf->peek();
                    size_t headerLen = crlf - start;

                    // 处理空行（请求头结束标志）
                    if (headerLen == 0) {
                        buf->retrieveUntil(crlf + 2); // 消费空行
                        ok = handleHeaderComplete();   // 处理请求头完成后的逻辑
                        cout<<"kExpectHeaders | handleHeaderComplete返回: "<<ok<<", 当前状态: "<<state_<<endl;
                        hasMore = false;
                        break;
                    }

                    // 解析新的头部（key: value）
                    const char* colon = std::find(start, crlf, ':');
                    if (colon == crlf) {
                        ok = false; // 缺少冒号，非法头部
                        hasMore = false;
                    } else {
                        request_.addHeader(start, colon, crlf);
                    }

                    buf->retrieveUntil(crlf + 2); // 消费当前头部行
                } else {
                    // 未找到\r\n，等待更多数据（或请求头过长）
                    cout<<"kExpectHeaders | 未找到CRLF,等待更多数据"<<endl;
                    if (buf->readableBytes() > kMaxRequestSize) {
                        ok = false;
                        hasMore = false;
                    }
                    hasMore = false;
                }
                break;
            }

            // 3. 解析固定长度请求体（Content-Length）
            case kExpectBody: {
                cout<<"kExpectBody"<<endl;
                size_t needed = request_.getContentLength() - request_.body().size();
                size_t have = buf->readableBytes();
                if (needed == 0) {
                    // 请求体长度为0，直接完成
                    state_ = kGotAll;
                    hasMore = false;
                    break;
                }

                // 读取当前可用的数据（不超过需要的长度）
                size_t readLen = std::min(needed, have);
                if (readLen > 0) {
                    request_.appendBody(buf->peek(), readLen);
                    buf->retrieve(readLen);
                }

                // 检查请求体是否完整
                if (request_.body().size() == request_.getContentLength()) {
                    state_ = kGotAll;
                    hasMore = false;
                } else {
                    // 数据不足，等待更多数据
                    hasMore = false;
                }
                break;
            }

            // 4. 解析分块编码：先读块长度（如 "1a\r\n" 表示26字节数据）
            case kExpectChunkedBody: {
                cout<<"kExpectChunkedBody"<<endl;
                const char* crlf = buf->findCRLF();
                if (crlf) {
                    const char* start = buf->peek();
                    size_t lenStrLen = crlf - start;

                    // 解析16进制块长度（支持可选的分号后的扩展信息，如 "1a;comment\r\n"）
                    char* endPtr = nullptr;
                    errno = 0;
                    unsigned long chunkLen = strtoul(start, &endPtr, 16);

                    // 校验块长度合法性
                    if (errno != 0 || endPtr > crlf || chunkLen > kMaxRequestSize) {
                        ok = false;
                        hasMore = false;
                        break;
                    }

                    // 块长度为0表示分块结束
                    if (chunkLen == 0) {
                        state_ = kExpectChunkedTrailer; // 切换到解析尾部状态
                    } else {
                        request_.setChunkedRemaining(chunkLen);
                        state_ = kExpectChunkedData; // 切换到解析块数据状态
                    }

                    buf->retrieveUntil(crlf + 2); // 消费块长度行
                } else {
                    // 未找到\r\n，等待更多数据
                    hasMore = false;
                }
                break;
            }

            // 5. 解析分块编码：读取块数据
            case kExpectChunkedData: {
                cout<<"kExpectChunkedData"<<endl;
                size_t needed = request_.getChunkedRemaining();
                size_t have = buf->readableBytes();
                if (needed == 0) {
                    ok = false;
                    hasMore = false;
                    break;
                }

                // 读取当前可用的块数据
                size_t readLen = std::min(needed, have);
                if (readLen > 0) {
                    request_.appendBody(buf->peek(), readLen);
                    buf->retrieve(readLen);
                    request_.setChunkedRemaining(needed - readLen);
                }

                // 块数据读取完成，需跳过后续的\r\n
                if (request_.getChunkedRemaining() == 0) {
                    // 检查是否有\r\n（块数据后必须跟\r\n）
                    if (buf->readableBytes() >= 2 && 
                        buf->peek()[0] == '\r' && buf->peek()[1] == '\n') {
                        buf->retrieve(2); // 消费\r\n
                        state_ = kExpectChunkedBody; // 准备读取下一个块
                    } else {
                        // 缺少\r\n，等待更多数据
                        hasMore = false;
                    }
                } else {
                    // 块数据不足，等待更多数据
                    hasMore = false;
                }
                break;
            }

            // 6. 解析分块编码：尾部（可选的响应头，实际请求中很少用）
            case kExpectChunkedTrailer: {
                cout<<"kExpectChunkedTrailer"<<endl;
                const char* crlf = buf->findCRLF();
                if (crlf) {
                    const char* start = buf->peek();
                    size_t trailerLen = crlf - start;

                    // 空行表示尾部结束
                    if (trailerLen == 0) {
                        request_.markChunkedComplete();
                        state_ = kGotAll;
                        hasMore = false;
                    } else {
                        // 解析尾部头部（实际可忽略，或按需处理）
                        const char* colon = std::find(start, crlf, ':');
                        if (colon != crlf) {
                            const char* valueStart = colon + 1;
                            while (valueStart < crlf && (*valueStart == ' ' || *valueStart == '\t')) {
                                ++valueStart;
                            }
                            request_.addHeader(start, colon, crlf);
                        }
                    }

                    buf->retrieveUntil(crlf + 2); // 消费尾部行
                } else {
                    // 未找到\r\n，等待更多数据
                    hasMore = false;
                }
                break;
            }

            // 7. 解析完成：后续数据属于下一个请求（Keep-Alive场景）
            case kGotAll: { 
                cout<<"kGotAll"<<endl;
                hasMore = false;
                break;
            }

            default: {
                ok = false;
                hasMore = false;
                break;
            }
        }
    }

    // 解析失败时重置状态，避免影响下一个请求
    if (!ok) {  
        cout<<"parse failed, reset state"<<endl;
        state_ = kExpectRequestLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    return ok;
}

// 新增：请求头解析完成后的逻辑（判断请求体类型并切换状态）
bool HttpContext::handleHeaderComplete() {
    HttpRequest::Method method = request_.method();
    if (method != HttpRequest::kPost && method != HttpRequest::kPut) {
        // 非POST/PUT请求，无请求体
        state_ = kGotAll;
        return true;
    }

    // 优先处理Transfer-Encoding（分块编码）
    std::string transferEncoding = request_.getHeader("Transfer-Encoding");
    if (!transferEncoding.empty() && transferEncoding == "chunked") {
        request_.resetChunkedState();
        state_ = kExpectChunkedBody;
        return true;
    }

    // 处理Content-Length（固定长度）
    std::string contentLengthStr = request_.getHeader("Content-Length");
    if (!contentLengthStr.empty()) {
        try {
            size_t contentLen = std::stoul(contentLengthStr);
            if (contentLen > 1024 * 1024) { // 限制最大请求体1MB
                return false;
            }
            request_.setContentLength(contentLen);
            state_ = (contentLen == 0) ? kGotAll : kExpectBody;
            return true;
        } catch (const std::invalid_argument&) {
            return false; // 非数字Content-Length
        } catch (const std::out_of_range&) {
            return false; // Content-Length超出范围
        }
    }

    // 既无Transfer-Encoding也无Content-Length，POST请求非法
    return false;
}