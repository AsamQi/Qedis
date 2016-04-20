//
//  main.cpp
//  qedis
//
//  Created by Bert Young on 14-1-25.
//  Copyright (c) 2014年 Bert Young. All rights reserved.
//

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

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

#include "Qedis.h"

const unsigned Qedis::kRunidSize = 40;

Qedis::Qedis() : port_(0), masterPort_(0), runid_(new char[kRunidSize + 1]())
{
}

Qedis::~Qedis()
{
}

static void Usage()
{
    std::cerr << "Usage:  ./qedis-server [/path/to/redis.conf] [options]\n\
        ./qedis-server -v or --version\n\
        ./qedis-server -h or --help\n\
Examples:\n\
        ./qedis-server (run the server with default conf)\n\
        ./qedis-server /etc/redis/6379.conf\n\
        ./qedis-server --port 7777\n\
        ./qedis-server --port 7777 --slaveof 127.0.0.1 8888\n\
        ./qedis-server /etc/myredis.conf --loglevel verbose\n";
}

bool  Qedis::ParseArgs(int ac, char* av[])
{
    for (int i = 0; i < ac; i ++)
    {
        if (cfgFile_.empty() && ::access(av[i], R_OK) == 0)
        {
            cfgFile_ = av[i];
            continue;
        }
        else if (strncasecmp(av[i], "-v", 2) == 0 ||
                 strncasecmp(av[i], "--version", 9) == 0)
        {
            std::cerr << "Qedis Server v="
                      << QEDIS_VERSION
                      << " bits="
                      << (sizeof(void*) == 8 ? 64 : 32)
                      << std::endl;

            exit(0);
            return true;
        }
        else if (strncasecmp(av[i], "-h", 2) == 0 ||
                 strncasecmp(av[i], "--help", 6) == 0)
        {
            Usage();
            exit(0);
            return true;
        }
        else if (strncasecmp(av[i], "--port", 6) == 0)
        {
            if (++i == ac)
            {
                return false;
            }
            port_ = static_cast<unsigned short>(std::atoi(av[i]));
        }
        else if (strncasecmp(av[i], "--loglevel", 10) == 0)
        {
            if (++i == ac)
            {
                return false;
            }
            logLevel_ = std::string(av[i]);
        }
        else if (strncasecmp(av[i], "--slaveof", 9) == 0)
        {
            if (i + 2 >= ac)
            {
                return false;
            }
            
            master_ = std::string(av[++i]);
            masterPort_ = static_cast<unsigned short>(std::atoi(av[++i]));
        }
        else
        {
            std::cerr << "Unknow option " << av[i] << std::endl;
            return false;
        }
    }
    
    return true;
}


std::shared_ptr<StreamSocket>   Qedis::_OnNewConnection(int connfd)
{
    using namespace qedis;
    
    SocketAddr  peer;
    Socket::GetPeerAddr(connfd,  peer);

    std::shared_ptr<QClient>    pNewTask(new QClient());
    if (pNewTask->Init(connfd))
    {
        const bool peerIsMaster = (peer == QReplication::Instance().GetMasterInfo().addr);
        if (peerIsMaster)
        {
            QReplication::Instance().GetMasterInfo().state = QReplState_connected;
            QReplication::Instance().SetMaster(pNewTask);
            
            pNewTask->SetName("MasterConnection");
            pNewTask->SetFlag(ClientFlag_master);
        }
    }
    else
    {
        pNewTask.reset();
    }
    
    return  pNewTask;
}

Time  g_now;

static void  QdbCron()
{
    using namespace qedis;
    
    if (g_qdbPid != -1)
        return;
    
    if (g_now.MilliSeconds() > 1000UL * (g_lastQDBSave + static_cast<unsigned>(g_config.saveseconds)) &&
        QStore::dirty_ >= g_config.savechanges)
    {
        int ret = fork();
        if (ret == 0)
        {
            {
                QDBSaver  qdb;
                qdb.Save(g_config.rdbfullname.c_str());
                std::cerr << "ServerCron child save rdb done, exiting child\n";
            }  //  make qdb to be destructed before exit
            _exit(0);
        }
        else if (ret == -1)
        {
            ERR << "fork qdb save process failed";
        }
        else
        {
            g_qdbPid = ret;
        }
            
        INF << "ServerCron save rdb file " << g_config.rdbfullname;
    }
}

static void LoadDbFromDisk()
{
    using namespace qedis;
    
    //  USE AOF RECOVERY FIRST, IF FAIL, THEN RDB
    QAOFLoader aofLoader;
    if (aofLoader.Load(QAOFThreadController::Instance().GetAofFile().c_str()))
    {
        const auto& cmds = aofLoader.GetCmds();
        for (const auto& cmd : cmds)
        {
            const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd[0]);
            QCommandTable::ExecuteCmd(cmd, info);
        }
    }
    else
    {
        QDBLoader  loader;
        loader.Load(g_config.rdbfullname.c_str());
    }
}

bool Qedis::_Init()
{
    using namespace qedis;
    
    getRandomHexChars(runid_.get(), kRunidSize);
    
    if (port_ != 0)
        g_config.port = port_;

    if (!logLevel_.empty())
        g_config.loglevel = logLevel_;
    
    if (!master_.empty())
    {
        g_config.masterIp = master_;
        g_config.masterPort = masterPort_;
    }
    
    // process log
    {
        unsigned int level = ConvertLogLevel(g_config.loglevel), dest = 0;

        if (g_config.logdir == "stdout")
            dest = logConsole;
        else
            dest = logFILE;
        
        g_log = LogManager::Instance().CreateLog(level, dest, g_config.logdir.c_str());
    }
    
    SocketAddr addr(g_config.ip.c_str() , g_config.port);
    
    if (!Server::TCPBind(addr))
    {
        ERR << "can not bind socket on port " << addr.GetPort();
        return false;
    }

    QCommandTable::Init();
    QCommandTable::AliasCommand(g_config.aliases);
    QSTORE.Init(g_config.databases);
    QSTORE.password_ = g_config.password;
    QSTORE.InitExpireTimer();
    QSTORE.InitBlockedTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    if (g_config.appendonly)
        QAOFThreadController::Instance().SetAofFile(g_config.appendfilename);
    
    LoadDbFromDisk();

    QAOFThreadController::Instance().Start();

    QSlowLog::Instance().SetThreshold(g_config.slowlogtime);
    QSlowLog::Instance().SetLogLimit(static_cast<std::size_t>(g_config.slowlogmaxlen));
    
    {
        auto cronTimer = TimerManager::Instance().CreateTimer();
        cronTimer->Init(1000 / qedis::g_config.hz);
        cronTimer->SetCallback([]() {
                QdbCron();
        });
        TimerManager::Instance().AddTimer(cronTimer);
    }

    {
        auto repTimer = TimerManager::Instance().CreateTimer();
        repTimer->Init(100 * 5);
        repTimer->SetCallback([&]() {
            qedis::QReplication::Instance().Cron();
        });
        TimerManager::Instance().AddTimer(repTimer);
    }
    
    // master ip
    if (!g_config.masterIp.empty())
    {
        QReplication::Instance().GetMasterInfo().addr.Init(g_config.masterIp.c_str(),
                                                           g_config.masterPort);
    }
    
    return  true;
}

static void CheckChild()
{
    using namespace qedis;

    if (g_qdbPid == -1 && g_rewritePid == -1)
        return;

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
            if (QReplication::Instance().IsBgsaving())
                QReplication::Instance().OnRdbSaveDone();
            else
                QReplication::Instance().TryBgsave();
        }
        else if (pid == g_rewritePid)
        {
            INF << pid << " rewrite process success done.";
            QAOFThreadController::RewriteDoneHandler(exitcode, bysignal);
        }
        else
        {
            ERR << pid << " is not rdb or aof process ";
            assert (false);
        }
    }
}

bool Qedis::_RunLogic()
{
    g_now.Now();
    TimerManager::Instance().UpdateTimers(g_now);
    
    CheckChild();
    
    if (!Server::_RunLogic())
        std::this_thread::sleep_for(std::chrono::microseconds(100));

    return  true;
}


void    Qedis::_Recycle()
{
    qedis::QAOFThreadController::Instance().Stop();
}


int main(int ac, char* av[])
{
    Qedis  svr;
    if (!svr.ParseArgs(ac - 1, av + 1))
    {
        Usage();
        return -1;
    }

    if (!svr.GetConfigName().empty())
    {
        if (!LoadQedisConfig(svr.GetConfigName().c_str(), qedis::g_config))
        {
            std::cerr << "Load config file [" << svr.GetConfigName() << "] failed!\n";
            return false;
        }
    }
    else
    {
        std::cerr << "No config file specified, using the default config.\n";
    }
    
    svr.MainLoop(qedis::g_config.daemonize);
    
    return 0;
}


