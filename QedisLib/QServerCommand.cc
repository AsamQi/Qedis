#include <sys/utsname.h>
#include <cassert>
#include <unistd.h>

#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Server.h"
#include "QDB.h"
#include "QAOF.h"
#include "QReplication.h"
#include "QConfig.h"
#include "QSlowLog.h"


namespace qedis
{

QError  select(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    int newDb = atoi(params[1].c_str());
    
    auto client = QClient::Current();
    
    if (client)
    {
        if (client->SelectDB(newDb))
            FormatOK(reply);
        else
            ReplyError(QError_invalidDB, reply);
    }
    else
    {
        QSTORE.SelectDB(newDb);
    }

    return   QError_ok;
}


QError  dbsize(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatInt(static_cast<long>(QSTORE.DBSize()), reply);
    return   QError_ok;
}

QError  flushdb(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QSTORE.dirty_ += QSTORE.DBSize();
    QSTORE.ClearCurrentDB();
    Propogate(QSTORE.GetDB(), params);
    
    FormatOK(reply);
    return   QError_ok;
}

QError  flushall(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    int currentDb = QSTORE.GetDB();
    
    QEDIS_DEFER {
        QSTORE.SelectDB(currentDb);
        Propogate(-1, params);
        QSTORE.ResetDb();
    };
    
    for (int dbno = 0; true; ++ dbno)
    {
        if (QSTORE.SelectDB(dbno) == -1)
            break;
  
        QSTORE.dirty_ += QSTORE.DBSize();
    }
    
    FormatOK(reply);
    return   QError_ok;
}

QError  bgsave(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        FormatBulk("-ERR Background save or aof already in progress",
                   sizeof "-ERR Background save or aof already in progress" - 1,
                   reply);

        return QError_ok;
    }
   
    int ret = fork();
    if (ret == 0)
    {
        {
        QDBSaver  qdb;
        qdb.Save(g_config.rdbfullname.c_str());
        }
        _exit(0);
    }
    else if (ret == -1)
    {
        FormatSingle("Background saving FAILED", 24, reply);
    }
    else
    {
        g_qdbPid = ret;
        FormatSingle("Background saving started", 25, reply);
    }

    return   QError_ok;
}

QError  save(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        FormatBulk("-ERR Background save or aof already in progress",
                   sizeof "-ERR Background save or aof already in progress" - 1,
                   reply);
        
        return QError_ok;
    }
    
    QDBSaver  qdb;
    qdb.Save(g_config.rdbfullname.c_str());
    g_lastQDBSave = time(NULL);

    FormatOK(reply);
    return   QError_ok;
}

QError  lastsave(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatInt(g_lastQDBSave, reply);
    return   QError_ok;
}

QError  client(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    // getname   setname    kill  list
    QError   err = QError_ok;
    
    if (params[1].size() == 7 && strncasecmp(params[1].c_str(), "getname", 7) == 0)
    {
        if (params.size() != 2)
            ReplyError(err = QError_param, reply);
        else
            FormatBulk(QClient::Current()->GetName(),
                       reply);
    }
    else if (params[1].size() == 7 && strncasecmp(params[1].c_str(), "setname", 7) == 0)
    {
        if (params.size() != 3)
        {
            ReplyError(err = QError_param, reply);
        }
        else
        {
            QClient::Current()->SetName(params[2]);
            FormatOK(reply);
        }
    }
    else if (params[1].size() == 4 && strncasecmp(params[1].c_str(), "kill", 4) == 0)
    {
        // only kill current client
        QClient::Current()->OnError();
        FormatOK(reply);
    }
    else if (params[1].size() == 4 && strncasecmp(params[1].c_str(), "list", 4) == 0)
    {
        FormatOK(reply);
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
}

static int Suicide()
{
    int* ptr = nullptr;
    *ptr = 0;
    
    return *ptr;
}

QError  debug(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QError err = QError_ok;
    
    if (strncasecmp(params[1].c_str(), "segfault", 8) == 0 && params.size() == 2)
    {
        Suicide();
        assert (false);
    }
    else if (strncasecmp(params[1].c_str(), "object", 6) == 0 && params.size() == 3)
    {
        QObject* obj = nullptr;
        err = QSTORE.GetValue(params[2], obj);
        
        if (err != QError_ok)
        {
            ReplyError(err, reply);
        }
        else
        {
            // ref count,  encoding
            char buf[512];
            int  len = snprintf(buf, sizeof buf, "ref count:%ld, encoding:%s",
                                obj->value.use_count(),
                                EncodingStringInfo(obj->encoding));
            FormatBulk(buf, len, reply);
        }
    }
    else
    {
        ReplyError(err = QError_param, reply);
    }
    
    return   err;
}


QError  shutdown(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    Server::Instance()->Terminate();
    return   QError_ok;
}



QError  sync(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient* cli = QClient::Current();
    auto slave = cli->GetSlaveInfo();
    if (!slave)
    {
        cli->SetSlaveInfo();
        slave = cli->GetSlaveInfo();
        QREPL.AddSlave(cli);
    }
    
    if (slave->state == QSlaveState_wait_bgsave_end ||
        slave->state == QSlaveState_online)
    {
        WRN << cli->GetName() << " state is "
            << slave->state << ", ignore this sync request";
        return QError_ok;
    }
    
    slave->state = QSlaveState_wait_bgsave_start;
    QREPL.TryBgsave();

    return QError_ok;
}


QError  ping(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatSingle("PONG", 4, reply);
    return   QError_ok;
}

QError  echo(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    FormatBulk(params[1], reply);
    return   QError_ok;
}

QError  slaveof(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params[1] == "no" && params[2] == "one")
    {
        QREPL.SetMasterAddr(nullptr, 0);
    }
    else
    {
        QREPL.SetMasterAddr(params[1].c_str(), std::stoi(params[2]));
        QREPL.SetMasterState(QReplState_none);
    }
    
    FormatOK(reply);
    return QError_ok;
}


QError  info(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    char buf[1024];
    int  offset = 0;
    int  n = 0;
    
    // server
    struct utsname  name;
    uname(&name);
    n = snprintf(buf + offset, sizeof buf - 1 - offset,
                                "# Server\n"
                                "qedis_mode:standalone\n"
                                "os:%s %s %s\n"
                                "tcp_port:%hu\n"
                                , name.sysname, name.release, name.machine
                                , g_config.port);
    offset += n;

    // clients
    n = snprintf(buf + offset, sizeof buf - 1 - offset,
                                          "# Clients\n"
                                          "connected_clients:%lu\n"
                                          "blocked_clients:%lu\n"
                                          , Server::Instance()->TCPSize()
                                          , QSTORE.BlockedSize());
    offset += n;
    // replication
    /*# Replication
     role:master
     connected_slaves:1
     slave0:127.0.0.1,0,online*/
    
    FormatSingle(buf, offset, reply);

    return   QError_ok;
}


QError  monitor(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QClient::AddCurrentToMonitor();
    
    FormatOK(reply);
    return   QError_ok;
}

QError  auth(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (QSTORE.CheckPassword(params[1]))
    {
        QClient::Current()->SetAuth();
        FormatOK(reply);
    }
    else
    {
        ReplyError(QError_errAuth, reply);
    }
    
    return   QError_ok;
}

QError  slowlog(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params[1] == "len")
    {
        FormatInt(static_cast<long>(QSlowLog::Instance().GetLogsCount()), reply);
    }
    else if (params[1] == "reset")
    {
        QSlowLog::Instance().ClearLogs();
        FormatOK(reply);
    }
    else if (params[1] == "get")
    {
        const long limit = static_cast<long>(QSlowLog::Instance().GetLogsCount());
        long realCnt = limit;
        if (params.size() == 3)
        {
            if (!Strtol(params[2].c_str(), params[2].size(), &realCnt))
            {
                ReplyError(QError_syntax, reply);
                return QError_syntax;
            }
        }
        
        if (realCnt > limit)
            realCnt = limit;
        
        PreFormatMultiBulk(realCnt, reply);
        for (const auto& item : QSlowLog::Instance().GetLogs())
        {
            if (realCnt -- == 0)
                break;
            
            PreFormatMultiBulk(2, reply);
            FormatInt(static_cast<long>(item.used), reply);
            
            PreFormatMultiBulk(static_cast<long>(item.cmds.size()), reply);
            for (const auto& c : item.cmds)
            {
                FormatBulk(c, reply);
            }
        }
    }
    else
    {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }
    
    return   QError_ok;
}
    
}
