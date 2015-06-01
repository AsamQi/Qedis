//
//  main.cpp
//  qedis
//
//  Created by Bert Young on 14-1-25.
//  Copyright (c) 2014年 Bert Young. All rights reserved.
//

#include <iostream>

#include "Server.h"
#include "Log/Logger.h"
#include "Timer.h"

#include "QClient.h"
#include "QStore.h"
#include "QCommand.h"

#include "QPubsub.h"
#include "QDB.h"
#include "QAOF.h"
#include "QConfig.h"
#include "QSlowLog.h"


class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
private:
    std::shared_ptr<StreamSocket>   _OnNewConnection(int fd);
    bool    _Init();
    bool    _RunLogic();
    void    _Recycle();
};


Qedis::Qedis()
{
}

Qedis::~Qedis()
{
}


std::shared_ptr<StreamSocket>   Qedis::_OnNewConnection(int connfd)
{
    SocketAddr  peer;
    Socket::GetPeerAddr(connfd,  peer);

    std::shared_ptr<QClient>    pNewTask(new QClient());
    if (!pNewTask->Init(connfd))
        pNewTask.reset();
    
    return  pNewTask;
}

static void LoadDbFromFile()
{
    //  USE AOF RECOVERY FIRST, IF FAIL, THEN RDB
    QAOFLoader aofLoader;
    if (aofLoader.Load(g_aofFileName))
    {
        const auto& cmds = aofLoader.GetCmds();
        for (const auto& cmd : cmds)
        {
            const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd[0]);
            UnboundedBuffer reply;
            QCommandTable::ExecuteCmd(cmd, info, reply);
        }
    }
    else
    {
        QDBLoader  loader;
        loader.Load(g_qdbFile);
    }
}

bool Qedis::_Init()
{
    if (!LoadQedisConfig("qedis.conf", g_config))
    {
        std::cerr << "can not load qedis.conf\n";
        return false;
    }
    
    // daemon must be first, before descriptor open, threads create
    if (g_config.daemonize)
    {
        daemon(1, 0);
    }
    
    g_log = LogManager::Instance().CreateLog(logALL, logALL, "./qedislog/");
    
    SocketAddr addr("0.0.0.0", g_config.port);
    
    if (!Server::TCPBind(addr))
    {
        ERR << "can not bind socket on port " << addr.GetPort();
        return false;
    }

    QCommandTable::Init();
    QSTORE.Init(g_config.databases);
    QSTORE.InitExpireTimer();
    QSTORE.InitBlockedTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    LoadDbFromFile();

    QAOFThreadController::Instance().Start();
    
    // slow log
    QSlowLog::Instance().SetThreshold(g_config.slowlogtime);
    
    return  true;
}

Time  g_now;

bool Qedis::_RunLogic()
{
    g_now.Now();
    TimerManager::Instance().UpdateTimers(g_now);
    
    if (g_qdbPid != -1 || g_rewritePid != -1)
    {
        int    statloc;

        pid_t  pid = wait3(&statloc,WNOHANG,NULL);
        if (pid != 0 && pid != -1)
        {
            int exitcode = WEXITSTATUS(statloc);
            int bysignal = 0;

            if (WIFSIGNALED(statloc)) bysignal = WTERMSIG(statloc);
            
            if (pid == g_qdbPid)
            {
                QDBSaver::SaveDoneHandler(exitcode, bysignal);
            }
            else if (pid == g_rewritePid)
            {
                INF << pid << " aof process success done.";
                QAOFThreadController::RewriteDoneHandler(exitcode, bysignal);
            }
            else
            {
                ERR << pid << " is not rdb or aof process ";
                assert (false);
            }
        }
    }
    
    if (!Server::_RunLogic())
        Thread::YieldCPU();

    return  true;
}


void    Qedis::_Recycle()
{
    QAOFThreadController::Instance().Stop();
}

int main()
{
    Qedis  svr;
    svr.MainLoop();
    
    return 0;
}


