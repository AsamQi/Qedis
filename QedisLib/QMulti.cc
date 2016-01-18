#include "QMulti.h"
#include "QClient.h"
#include "QStore.h"
#include "Log/Logger.h"
#include "Timer.h"

namespace qedis
{

QMulti&    QMulti::Instance()
{
    static QMulti  mt;
    return mt;
}
    
void  QMulti::Watch(QClient* client, int dbno, const QString& key)
{
    if (client->Watch(dbno, key))
    {
        Clients& cls = clients_[dbno][key];
        cls.push_back(std::static_pointer_cast<QClient>(client->shared_from_this()));
    }
}


void QMulti::Unwatch(QClient* client)
{
    client->UnWatch();
}


void QMulti::Multi(QClient* client)
{
    client->ClearMulti();
    client->SetFlag(ClientFlag_multi);
}

bool QMulti::Exec(QClient* client)
{
    return client->Exec();
}

void QMulti::Discard(QClient* client)
{
    client->ClearMulti();
    client->UnWatch();
}


void  QMulti::NotifyDirty(int dbno, const QString& key)
{
    INF << "Try NotifyDirty " << key.c_str() << ", dbno " << dbno;
    auto tmpDbIter = clients_.find(dbno);
    if (tmpDbIter == clients_.end())
        return;
    
    auto& dbWatchedKeys = tmpDbIter->second;
    auto it = dbWatchedKeys.find(key);
    if (it == dbWatchedKeys.end())
        return;
    
    Clients& cls = it->second;
    for (auto itCli(cls.begin()); itCli != cls.end(); )
    {
        auto client(itCli->lock());
        if (!client)
        {
            WRN << "erase not exist client ";
            itCli = cls.erase(itCli);
        }
        else
        {
            if (client.get() != QClient::Current() && client->NotifyDirty(dbno, key))
            {
                WRN << "erase dirty client ";
                itCli = cls.erase(itCli);
            }
            else
            {
                ++ itCli;
            }
        }
    }
        
    if (cls.empty())
    {
        dbWatchedKeys.erase(it);
    }
}

// multi commands
QError  watch(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    if (client->IsFlagOn(ClientFlag_multi))
    {
        ReplyError(QError_watch, reply);
        return  QError_watch;
    }
    
    std::for_each(++ params.begin(), params.end(), [client](const QString& s) {
        QMulti::Instance().Watch(client, QSTORE.GetDB(), s);
    } );
    
    FormatOK(reply);
    return QError_ok;
}

QError  unwatch(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Unwatch(client);
    FormatOK(reply);
    return QError_ok;
}

QError  multi(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Multi(client);
    FormatOK(reply);
    return QError_ok;
}

QError  exec(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    if (!client->IsFlagOn(ClientFlag_multi))
    {
        ReplyError(QError_noMulti, reply);
        return QError_noMulti;
    }
    if (!QMulti::Instance().Exec(client))
    {
        ReplyError(QError_dirtyExec, reply);
        return  QError_dirtyExec;
    }
    return QError_ok;
}

QError  discard(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Discard(client);
    FormatOK(reply);
    return QError_ok;
}

}