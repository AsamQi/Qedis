#include "QSet.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;


#define GET_SET(setname)  \
    QObject  value(QType_set);  \
    QError err = QSTORE.GetValueByType(setname, value, QType_set);  \
    if (err != QError_ok)  {  \
        ReplyErrorInfo(err, reply); \
        return err;  \
    }

#define GET_OR_SET_SET(setname)  \
    QObject value(QType_set);  \
    QError err = QSTORE.GetValueByType(setname, value, QType_set);  \
    if (err != QError_ok && err != QError_notExist)  {  \
        ReplyErrorInfo(err, reply); \
        return err;  \
    }   \
    if (err == QError_notExist) { \
        value.value.Reset(new QSet);  \
        err = QSTORE.SetValue(setname, value);  \
        if (err != QError_ok)  {  \
            ReplyErrorInfo(err, reply);  \
            return err; \
        } \
    }

bool _set_random(const QSet& set, QString& res)
{
#if 0
    if (set.empty())
    {
        ERR << "set is empty";
        return false;
    }
        
    //INF << "set bucket_count " << set.bucket_count();

    while (true)
    {
        int bucket = rand() % set.bucket_count();
        INF << "set bucket " << bucket << ", and bucket size " << set.bucket_size(bucket);
        if (set.bucket_size(bucket) == 0)
            continue;

        int lucky = rand() % set.bucket_size(bucket);
        QSet::const_local_iterator it = set.begin(bucket);
        while (lucky > 0)
        {
            ++ it;
            -- lucky;
        }
        
        res = *it;
        return true;
    }
#endif

    return true;
}

QError  spop(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "spop");

    GET_SET(params[1]);

    const PSET& set  = value.CastSet();

    QString res;
    if (_set_random(*set, res))
    {
        FormatSingle(res.c_str(), static_cast<int>(res.size()), reply);
        set->erase(res);
    }
    else
        FormatSingle("", 0, reply);
    
    return   QError_ok;
}


QError  srandmember(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "srandmember");
    
    ReplyErrorInfo(QError_paramNotMatch, reply);

    return   QError_ok;
}

QError  sadd(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "sadd");
   
    GET_OR_SET_SET(params[1]);
    
    int  res = 0;
    const PSET&  set = value.CastSet();
    for (int i = 2; i < params.size(); ++ i)
        if (set->insert(params[i]).second)
            ++ res;
    
    FormatInt(res, reply);

    return   QError_ok;
}

QError  scard(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "scard");

    GET_SET(params[1]);

    const PSET&  set  = value.CastSet();
    int  size = static_cast<int>(set->size());
    cout << "scard fine= " << size << endl;
    
    FormatInt(size, reply);
    return   QError_ok;
}

QError  srem(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "srem");

    GET_SET(params[1]);

    const PSET& set  = value.CastSet();
    int res = 0;
    for (int i = 2; i < params.size(); ++ i)
    {
        if (set->erase(params[i]) != 0)
            ++ res;
    }
    
    FormatInt(res, reply);
    return   QError_ok;
}

QError  sismember(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "sismember");
    
    GET_SET(params[1]);
    
    const PSET& set  = value.CastSet();
    int res = static_cast<int>(set->count(params[2]));
    
    FormatInt(res, reply);

    return   QError_ok;
}

QError  smembers(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "smembers");

    GET_SET(params[1]);

    const PSET& set = value.CastSet();

    PreFormatMultiBulk(static_cast<int>(set->size()), reply);
    for (QSet::const_iterator it(set->begin()); it != set->end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);

    return   QError_ok;
}

QError  smove(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "smove");
    
    GET_SET(params[1]);
    
    const PSET& set = value.CastSet();
    int  ret = static_cast<int>(set->erase(params[3]));
    
    if (ret != 0)
    {
        QObject  dst(QType_set);
        err = QSTORE.GetValueByType(params[2], dst, QType_set);
        if (err == QError_notExist)
        {
            err = QError_ok;
            dst.value.Reset(new QSet());
            QSTORE.SetValue(params[2], dst);
        }
        
        if (err == QError_ok)
        {
            PSET set = dst.CastSet();
            set->insert(params[3]);
        }
    }
    
    FormatInt(ret, reply);
    return err;
}



QSet& QSet_diff(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(l.begin()); 
            it != l.end();
            ++ it)
    {
        if (r.find(*it) == r.end())
        {
            result.insert(*it);
        }
    }

    return result;
}

QSet& QSet_inter(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(l.begin()); 
            it != l.end();
            ++ it)
    {
        if (r.find(*it) != r.end())
        {
            result.insert(*it);
        }
    }

    return result;
}


QSet& QSet_union(const QSet& l, const QSet& r, QSet& result)
{
    for (QSet::const_iterator it(r.begin());
         it != r.end();
         ++ it)
    {
        result.insert(*it);
    }
    
    for (QSet::const_iterator it(l.begin());
         it != l.end();
         ++ it)
    {
        result.insert(*it);
    }
    
    return result;
}

enum SetOperation
{
    SetOperation_diff,
    SetOperation_inter,
    SetOperation_union,
};

static void  _set_operation(const vector<QString>& params,
                            int offset,
                            QSet& res,
                            SetOperation oper)
{
    QObject  value;
    QError err = QSTORE.GetValueByType(params[offset], value, QType_set);
    if (err != QError_ok && oper != SetOperation_union)
        return;

    const PSET& set = value.CastSet();
    if (set)
        res = *set;
    
    for (int i = offset + 1; i < params.size(); ++ i)
    {
        QObject  value;
        QError err = QSTORE.GetValueByType(params[i], value, QType_set);
        if (err != QError_ok)
        {
            if (oper == SetOperation_inter)
            {
                res.clear();
                return;
            }
            continue;
        }
        
        QSet tmp;
        const PSET r = value.CastSet();
        if (oper == SetOperation_diff)
            QSet_diff(res, *r, tmp);
        else if (oper == SetOperation_inter)
            QSet_inter(res, *r, tmp);
        else if (oper == SetOperation_union)
            QSet_union(res, *r, tmp);
        
        res.swap(tmp);
        
        if (oper != SetOperation_union && res.empty())
            return;
    }
}

QError  sdiffstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet();
    _set_operation(params, 2, *res, SetOperation_diff);

    PreFormatMultiBulk(static_cast<int>(res->size()), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);

    QObject value(QType_set);
    value.value.Reset(res);
    
    return QSTORE.SetValue(params[1], value);
}

QError  sdiff(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_diff);
    
    PreFormatMultiBulk(static_cast<int>(res.size()), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);
    
    return QError_ok;
}


QError  sinter(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_inter);
    
    
    PreFormatMultiBulk(static_cast<int>(res.size()), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);
    
    return QError_ok;
}

QError  sinterstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet;
    _set_operation(params, 2, *res, SetOperation_inter);
    
    PreFormatMultiBulk(static_cast<int>(res->size()), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);

    QObject  value(QType_set);
    value.value.Reset(res);

    return  QSTORE.SetValue(params[1], value);
}


QError  sunion(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet res;
    _set_operation(params, 1, res, SetOperation_union);
    
    PreFormatMultiBulk(static_cast<int>(res.size()), reply);
    for (QSet::const_iterator it(res.begin()); it != res.end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);
    
    return QError_ok;
}

QError  sunionstore(const vector<QString>& params, UnboundedBuffer& reply)
{
    QSet* res = new QSet;
    _set_operation(params, 2, *res, SetOperation_union);
    
    PreFormatMultiBulk(static_cast<int>(res->size()), reply);
    for (QSet::const_iterator it(res->begin()); it != res->end(); ++ it)
        FormatBulk(it->c_str(), static_cast<int>(it->size()), reply);
    
    QObject  value(QType_set);
    value.value.Reset(res);
    
    return  QSTORE.SetValue(params[1], value);
}


