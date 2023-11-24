#ifndef __CHAT_APPLICATION_H__
#define __CHAT_APPLICATION_H__

#include <chat/http/http_server.h>
#include <memory>
#include <chat/stream.h>
#include <chat/singleton.h>
#include <chat/mutex.h>
#include <chat/protocol.h>
#include <map>
#include <unordered_map>
#include "chatServlet.h"
#include "resServlet.h"

namespace chat {

class Module {
public:
    enum Type {
        MODULE = 0
    };

    typedef std::shared_ptr<Module> ptr;

    Module(const std::string& name
            ,const std::string& version
            ,const std::string& filename
            ,uint32_t type = MODULE);
    virtual ~Module() {}

    virtual void onBeforeArgsParse(int argc, char** argv);
    virtual void onAfterArgsParse(int argc, char** argv);

    virtual bool onLoad();
    virtual bool onUnload();

    virtual bool onConnect(chat::Stream::ptr stream);
    virtual bool onDisconnect(chat::Stream::ptr stream);
    
    virtual bool onServerReady();
    virtual bool onServerUp();

    virtual bool handleRequest(chat::Message::ptr req
                               ,chat::Message::ptr rsp
                               ,chat::Stream::ptr stream);
    virtual bool handleNotify(chat::Message::ptr notify
                              ,chat::Stream::ptr stream);


    const std::string& getName() const { return m_name;}
    const std::string& getVersion() const { return m_version;}
    const std::string& getFilename() const { return m_filename;}
    const std::string& getId() const { return m_id;}

    void setFilename(const std::string& v) { m_filename = v;}

    uint32_t getType() const { return m_type;}

    void registerService(const std::string& server_type, const std::string& domain, const std::string& service);
    
protected:
    std::string m_name;
    std::string m_version;
    std::string m_filename;
    std::string m_id;
    uint32_t m_type;
};

class Application {

public:
    Application();

    static Application* GetInstance() { return s_instance;}
    bool init(int argc, char** argv);
    bool run();

    bool getServer(const std::string& type, std::vector<TcpServer::ptr>& svrs);
    void listAllServer(std::map<std::string, std::vector<TcpServer::ptr> >& servers);

private:
    int main(int argc, char** argv);
    int run_fiber();
private:
    int m_argc = 0;
    char** m_argv = nullptr;

    std::map<std::string, std::vector<TcpServer::ptr> > m_servers;
    IOManager::ptr m_mainIOManager;
    static Application* s_instance;

    Module::ptr m_module;

};



}

#endif