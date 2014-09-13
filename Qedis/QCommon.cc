#include "QCommon.h"
#include "UnboundedBuffer.h"
#include <limits>
#include <cstdlib>
#include <algorithm>

#define CRLF "\r\n"

struct QErrorInfo  g_errorInfo[] = {
    {sizeof "Fine"- 1,   "Fine"},
    {sizeof "Type not match"- 1,   "Type not match"},
    {sizeof "already exist"- 1,   "already exist"},
    {sizeof "not exist"- 1,   "not exist"},
    {sizeof "Paramas not match"- 1,   "Paramas not match"},
    {sizeof "Unknown command"- 1,   "Unknown command"},
};

void Int2Str(char* ptr, int nBytes, long val)
{
    snprintf(ptr, nBytes - 1, "%ld",  val);
}

bool Str2Int(const char* ptr, int nBytes, long& val)
{
    bool negtive = false;
    int i = 0;

    if (ptr[0] == '-' || ptr[0] == '+')
    {
        if (nBytes <= 1 || !isdigit(ptr[1]))
            return false;

        negtive = (ptr[0] == '-');
        i = 1;
    }

    val = 0;
    for (; i < nBytes; ++ i)
    {
        if (!isdigit(ptr[i]))
            break;

        val *= 10;
        val += ptr[i] - '0';
    }

    if (negtive)
    {
        val *= -1;
    }

    return true;
}

bool Strtol(const char* ptr, int nBytes, long* outVal)
{
    if (nBytes == 0)
        return false;

    char* pEnd = 0;
    long ret = strtol(ptr, &pEnd, 0);

    if (ret > std::numeric_limits<int>::max() ||
        ret < std::numeric_limits<int>::min())
        return false;

    //if (ret > INT32_MAX || ret < INT32_MIN)  return false;
    *outVal = ret;
    return pEnd == ptr + nBytes;
}

bool Strtof(const char* ptr, int nBytes, float* outVal)
{
    if (nBytes == 0)
        return false;

    char* pEnd = 0;
    *outVal = strtof(ptr, &pEnd);
    return pEnd == ptr + nBytes;
}


const char* Strstr(const char* ptr, int nBytes, const char* pattern, int nBytes2)
{
    if (!pattern || *pattern == 0)
        return 0;
    
    const char* ret = std::search(ptr, ptr + nBytes, pattern, pattern + nBytes2);
    return  ret == ptr + nBytes ? 0 : ret;
}

const char* SearchCRLF(const char* ptr, int nBytes)
{
    return  Strstr(ptr, nBytes, CRLF, 2);
}

int FormatInt(int value, UnboundedBuffer& reply)
{
    char val[32];
    int len = snprintf(val, sizeof val, "%d" CRLF, value);
    
    int  oldSize = reply.ReadableSize();
    reply.PushData(":", 1);
    reply.PushData(val, len);
    
    return reply.ReadableSize() - oldSize;
}

int FormatSingle(const char* str, int len, UnboundedBuffer& reply)
{
    int  oldSize = reply.ReadableSize();
    reply.PushData("+", 1);
    reply.PushData(str, len);
    reply.PushData(CRLF, 2);

    return reply.ReadableSize() - oldSize;
}

int FormatError(const char* str, int len, UnboundedBuffer& reply)
{
    int  oldSize = reply.ReadableSize();

    reply.PushData("-", 1);
    reply.PushData(str, len);
    reply.PushData(CRLF, 2);
    
    return reply.ReadableSize() - oldSize;
}

int FormatBulk(const char* str, int len, UnboundedBuffer& reply)
{
    int  oldSize = reply.ReadableSize();
    reply.PushData("$", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%d" CRLF, len);
    reply.PushData(val, tmp);

    if (str && len > 0)
    {
        reply.PushData(str, len);
        reply.PushData(CRLF, 2);
    }
    
    return reply.ReadableSize() - oldSize;
}

int PreFormatMultiBulk(int nBulk, UnboundedBuffer& reply)
{
    int  oldSize = reply.ReadableSize();
    reply.PushData("*", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%d" CRLF, nBulk);
    reply.PushData(val, tmp);

    return reply.ReadableSize() - oldSize;
}

void  ReplyErrorInfo(QError err, UnboundedBuffer& reply)
{
    const QErrorInfo& info = g_errorInfo[err];
    FormatError(info.errorStr, info.len, reply);
}

int FormatNull(UnboundedBuffer& reply)
{
    int  oldSize = reply.ReadableSize();
    reply.PushData("$", 1);

    char val[32];
    int len = snprintf(val, sizeof val - 1, "-1\r\n");
    reply.PushData(val, len);

    return reply.ReadableSize() - oldSize;
}

