/*
 * =====================================================================================
 *
 *       Filename: main.cpp
 *        Created: 08/31/2015 08:52:57 PM
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */
#include <asio.hpp>
#include <ctime>

#include "log.hpp"
#include "dbpod.hpp"
#include "memorypn.hpp"
#include "mapbindb.hpp"
#include "actorpool.hpp"
#include "netdriver.hpp"
#include "argparser.hpp"
#include "mainwindow.hpp"
#include "scriptwindow.hpp"
#include "serverargparser.hpp"
#include "serverconfigurewindow.hpp"
#include "databaseconfigurewindow.hpp"

#include <iostream>
#include "coro.hpp"

ServerArgParser          *g_serverArgParser;
Log                      *g_log;
MemoryPN                 *g_memoryPN;
ActorPool                *g_actorPool;
NetDriver                *g_netDriver;
DBPodN                   *g_DBPodN;

MapBinDB                 *g_mapBinDB;
ScriptWindow             *g_scriptWindow;
MainWindow               *g_mainWindow;
MonoServer               *g_monoServer;
ServerConfigureWindow    *g_serverConfigureWindow;
DatabaseConfigureWindow  *g_databaseConfigureWindow;
ActorMonitorWindow       *g_actorMonitorWindow;
ActorThreadMonitorWindow *g_actorThreadMonitorWindow;

int main(int argc, char *argv[])
{
    std::srand((unsigned int)std::time(nullptr));
    try{
        arg_parser stCmdParser(argc, argv);

        // start FLTK multithreading support
        Fl::lock();

        g_serverArgParser          = new ServerArgParser(stCmdParser);
        g_log                      = new Log("mir2x-monoserver-v0.1");
        g_scriptWindow             = new ScriptWindow();
        g_mainWindow               = new MainWindow();
        g_monoServer               = new MonoServer();
        g_memoryPN                 = new MemoryPN();
        g_mapBinDB                 = new MapBinDB();
        g_serverConfigureWindow    = new ServerConfigureWindow();
        g_databaseConfigureWindow  = new DatabaseConfigureWindow();
        g_actorPool                = new ActorPool(g_serverArgParser->ActorPoolThread);
        g_DBPodN                   = new DBPodN();
        g_netDriver                = new NetDriver();
        g_actorMonitorWindow       = new ActorMonitorWindow();
        g_actorThreadMonitorWindow = new ActorThreadMonitorWindow();

        g_mainWindow->ShowAll();

        while(Fl::wait() > 0){
            switch((uintptr_t)(Fl::thread_message())){
                case 0:
                    {
                        // FLTK will send 0 automatically
                        // to update the widgets and handle events
                        //
                        // if main loop or child thread need to flush
                        // call Fl::awake(0) to force Fl::wait() to terminate
                        break;
                    }
                case 2:
                    {
                        // propagate all exceptions to main thread
                        // then log it in main thread and request restart
                        //
                        // won't handle exception in threads
                        // all threads need to call Fl::awake(2) to propagate exception(s) caught
                        try{
                            g_monoServer->DetectException();
                        }catch(const std::exception &except){
                            std::string firstExceptStr;
                            g_monoServer->LogException(except, &firstExceptStr);
                            g_monoServer->Restart(firstExceptStr);
                        }
                        break;
                    }
                case 1:
                default:
                    {
                        // pase the gui requests in the queue
                        // designed to send Fl::awake(1) to notify gui
                        g_monoServer->parseNotifyGUIQ();
                        break;
                    }
            }
        }
    }catch(const std::exception &e){
        // use raw log directly
        // no gui available because we are out of gui event loop
        g_log->addLog(LOGTYPE_WARNING, "Exception in main thread: %s", e.what());
    }catch(...){
        g_log->addLog(LOGTYPE_WARNING, "Unknown exception caught in main thread");
    }
    return 0;
}
