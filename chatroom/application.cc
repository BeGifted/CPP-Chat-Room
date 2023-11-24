#include "application.h"
#include <unistd.h>
#include <signal.h>
#include <chat/tcp_server.h>
#include <chat/daemon.h>
#include <chat/config.h>
#include <chat/env.h>
#include <chat/log.h>
#include <chat/worker.h>
#include <chat/http/ws_server.h>
#include <chat/http/http_server.h>
#include <chat/util.h>
#include "resServlet.h"
#include "chatServlet.h"

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

static chat::ConfigVar<std::string>::ptr g_server_work_path =
    chat::Config::Lookup("server.work_path"
            ,std::string("/apps/work/chat")
            , "server work path");

static chat::ConfigVar<std::string>::ptr g_server_pid_file =
    chat::Config::Lookup("server.pid_file"
            ,std::string("chat.pid")
            , "server pid file");

static chat::ConfigVar<std::vector<TcpServerConf> >::ptr g_servers_conf
    = chat::Config::Lookup("servers", std::vector<TcpServerConf>(), "http server config");

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
}

bool Application::init(int argc, char** argv) {
    m_argc = argc;
    m_argv = argv;

    chat::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    chat::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    chat::EnvMgr::GetInstance()->addHelp("c", "conf path default: ./conf");
    chat::EnvMgr::GetInstance()->addHelp("p", "print help");

    bool is_print_help = false;
    if (!chat::EnvMgr::GetInstance()->init(argc, argv)) {
        is_print_help = true;
    }

    if (chat::EnvMgr::GetInstance()->has("p")) {
        is_print_help = true;
    }

    std::string conf_path = chat::EnvMgr::GetInstance()->getConfigPath();
    CHAT_LOG_INFO(g_logger) << "load conf path:" << conf_path;
    chat::Config::LoadFromConfDir(conf_path);

    m_module = std::make_shared<Module>("chat room", "1.0", "");

    m_module->onBeforeArgsParse(argc, argv);

    if (is_print_help) {
        chat::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    m_module->onAfterArgsParse(argc, argv);

    int run_type = 0;
    if (chat::EnvMgr::GetInstance()->has("s")) {
        run_type = 1;
    }
    if (chat::EnvMgr::GetInstance()->has("d")) {  //daemon
        run_type = 2;
    }

    if (run_type == 0) {
        chat::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
    if (chat::FSUtil::IsRunningPidfile(pidfile)) {
        CHAT_LOG_ERROR(g_logger) << "server is running:" << pidfile;
        return false;
    }

    if (!chat::FSUtil::Mkdir(g_server_work_path->getValue())) {
        CHAT_LOG_FATAL(g_logger) << "create work path [" << g_server_work_path->getValue()
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Application::run() {
    bool is_daemon = chat::EnvMgr::GetInstance()->has("d");
    return start_daemon(m_argc, m_argv, std::bind(&Application::main, this, std::placeholders::_1,
                                                  std::placeholders::_2), is_daemon);
}

int Application::main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    CHAT_LOG_INFO(g_logger) << "main";
    std::string conf_path = chat::EnvMgr::GetInstance()->getConfigPath();
    chat::Config::LoadFromConfDir(conf_path, true);
    {
        std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
        std::ofstream ofs(pidfile);
        if (!ofs) {
            CHAT_LOG_ERROR(g_logger) << "open pidfile " << pidfile << " failed";
            return false;
        }
        ofs << getpid();
    }

    m_mainIOManager.reset(new chat::IOManager(1, true, "main"));
    m_mainIOManager->schedule(std::bind(&Application::run_fiber, this));
    m_mainIOManager->addTimer(2000, [](){
    }, true);
    m_mainIOManager->stop();
    return 0;
}

int Application::run_fiber() {
    bool has_error = false;
    if (!m_module->onLoad()) {
        CHAT_LOG_ERROR(g_logger) << "module name="
            << m_module->getName() << " version=" << m_module->getVersion()
            << " filename=" << m_module->getFilename();
        has_error = true;
    }
    if (has_error) {
        _exit(0);
    }

    chat::WorkerMgr::GetInstance()->init();

    auto http_confs = g_servers_conf->getValue();
    std::vector<TcpServer::ptr> svrs;
    for(auto& i : http_confs) {
        CHAT_LOG_DEBUG(g_logger) << std::endl << LexicalCast<TcpServerConf, std::string>()(i);

        std::vector<Address::ptr> address;
        for(auto& a : i.address) {
            size_t pos = a.find(":");
            if (pos == std::string::npos) {
                address.push_back(UnixAddress::ptr(new UnixAddress(a)));
                continue;
            }
            int32_t port = atoi(a.substr(pos + 1).c_str());
            auto addr = chat::IPAddress::Create(a.substr(0, pos).c_str(), port);
            if (addr) {
                address.push_back(addr);
                continue;
            }
            std::vector<std::pair<Address::ptr, uint32_t> > result;
            if (chat::Address::GetInterfaceAddresses(result, a.substr(0, pos))) {
                for(auto& x : result) {
                    auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
                    if (ipaddr) {
                        ipaddr->setPort(atoi(a.substr(pos + 1).c_str()));
                    }
                    address.push_back(ipaddr);
                }
                continue;
            }

            auto aaddr = chat::Address::LookupAny(a);  //host
            if (aaddr) {
                address.push_back(aaddr);
                continue;
            }
            CHAT_LOG_ERROR(g_logger) << "invalid address: " << a;
            _exit(0);
        }
        IOManager* accept_worker = chat::IOManager::GetThis();
        IOManager* io_worker = chat::IOManager::GetThis();
        IOManager* process_worker = chat::IOManager::GetThis();
        if (!i.accept_worker.empty()) {
            accept_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.accept_worker).get();
            if (!accept_worker) {
                CHAT_LOG_ERROR(g_logger) << "accept_worker: " << i.accept_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.io_worker.empty()) {
            io_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.io_worker).get();
            if (!io_worker) {
                CHAT_LOG_ERROR(g_logger) << "io_worker: " << i.io_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.process_worker.empty()) {
            process_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.process_worker).get();
            if (!process_worker) {
                CHAT_LOG_ERROR(g_logger) << "process_worker: " << i.process_worker << " not exists";
                _exit(0);
            }
        }

        TcpServer::ptr server;
        if (i.type == "http") {
            server.reset(new chat::http::HttpServer(i.keepalive, process_worker, io_worker, accept_worker));
        } else if(i.type == "ws") {
            server.reset(new chat::http::WSServer(process_worker, io_worker, accept_worker));
        } else {
            CHAT_LOG_ERROR(g_logger) << "invalid server type=" << i.type << LexicalCast<TcpServerConf, std::string>()(i);
            _exit(0);
        }
        if (!i.name.empty()) {
            server->setName(i.name);
        }
        std::vector<Address::ptr> fails;
        if (!server->bind(address, fails, i.ssl)) {
            for(auto& x : fails) {
                CHAT_LOG_ERROR(g_logger) << "bind address fail:" << *x;
            }
            _exit(0);
        }
        if (i.ssl) {
            if (!server->loadCertificates(i.cert_file, i.key_file)) {
                CHAT_LOG_ERROR(g_logger) << "loadCertificates fail, cert_file=" << i.cert_file << " key_file=" << i.key_file;
            }
        }
        server->setConf(i);
        m_servers[i.type].push_back(server);
        svrs.push_back(server);
    }

    m_module->onServerReady();

    for(auto& i : svrs) {
        i->start();
    }

    m_module->onServerUp();

    return 0;
}

bool Application::getServer(const std::string& type, std::vector<TcpServer::ptr>& svrs) {
    auto it = m_servers.find(type);
    if(it == m_servers.end()) {
        return false;
    }
    svrs = it->second;
    return true;
}

void Application::listAllServer(std::map<std::string, std::vector<TcpServer::ptr> >& servers) {
    servers = m_servers;
}



Module::Module(const std::string& name
            ,const std::string& version
            ,const std::string& filename
            ,uint32_t type)
    :m_name(name)
    ,m_version(version)
    ,m_filename(filename)
    ,m_id(name + "/" + version)
    ,m_type(type) {
}

void Module::onBeforeArgsParse(int argc, char** argv) {
}

void Module::onAfterArgsParse(int argc, char** argv) {
}

bool Module::handleRequest(chat::Message::ptr req
                           ,chat::Message::ptr rsp
                           ,chat::Stream::ptr stream) {
    CHAT_LOG_DEBUG(g_logger) << "handleRequest req=" << req->toString()
            << " rsp=" << rsp->toString() << " stream=" << stream;
    return true;
}

bool Module::handleNotify(chat::Message::ptr notify
                          ,chat::Stream::ptr stream) {
    CHAT_LOG_DEBUG(g_logger) << "handleNotify nty=" << notify->toString()
            << " stream=" << stream;
    return true;
}

bool Module::onLoad() {
    CHAT_LOG_INFO(g_logger) << "on load";
    return true;
}

bool Module::onUnload() {
    CHAT_LOG_INFO(g_logger) << "on unload";
    return true;
}

bool Module::onConnect(chat::Stream::ptr stream) {
    return true;
}

bool Module::onDisconnect(chat::Stream::ptr stream) {
    return true;
}

bool Module::onServerReady() {
    CHAT_LOG_INFO(g_logger) << "on Server Ready";
    std::vector<chat::TcpServer::ptr> svrs;
    // if (!chat::Application::GetInstance()->getServer("http", svrs)) {
    //     CHAT_LOG_INFO(g_logger) << "no httpserver alive";
    //     return false;
    // }

    // for(auto& i : svrs) {
    //     chat::http::HttpServer::ptr http_server = std::dynamic_pointer_cast<chat::http::HttpServer>(i);
    //     chat::http::ServletDispatch::ptr slt_dispatch = http_server->getServletDispatch();
    //     chat::http::ResourceServlet::ptr slt(new chat::http::ResourceServlet(chat::EnvMgr::GetInstance()->getCwd()));
    //     slt_dispatch->addGlobServlet("/html/*", slt);
    //     CHAT_LOG_INFO(g_logger) << "add HTTP Servlet";
    // }

    // svrs.clear();
    if (!chat::Application::GetInstance()->getServer("ws", svrs)) {
        CHAT_LOG_INFO(g_logger) << "no ws alive";
        return false;
    }

    for(auto& i : svrs) {
        chat::http::WSServer::ptr ws_server = std::dynamic_pointer_cast<chat::http::WSServer>(i);
        chat::http::ServletDispatch::ptr slt_dispatch = ws_server->getWSServletDispatch();
        chat::http::ChatWSServlet::ptr slt(new chat::http::ChatWSServlet);
        slt_dispatch->addServlet("/chat", slt);
        CHAT_LOG_INFO(g_logger) << "add WS Servlet";
    }

    return true;
}

bool Module::onServerUp() {
    CHAT_LOG_INFO(g_logger) << "on Server Up";
    return true;
}




}