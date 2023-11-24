#ifndef __CHAT_CHAT_SERVLET_H__
#define __CHAT_CHAT_SERVLET_H__

#include "protocol.h"
#include <chat/http/ws_servlet.h>
#include <map>
#include <string>


namespace chat {
namespace http {

class ChatWSServlet : public WSServlet {
public:
    typedef std::shared_ptr<ChatWSServlet> ptr;
    ChatWSServlet();
    virtual int32_t onConnect(HttpRequest::ptr header
                              ,WSSession::ptr session) override;
    virtual int32_t onClose(HttpRequest::ptr header
                             ,WSSession::ptr session) override;
    virtual int32_t handle(HttpRequest::ptr header
                           ,WSFrameMessage::ptr msg
                           ,WSSession::ptr session) override;

    std::pair<std::string, std::string> getInfo(const std::string &id);
    void addInfo(const std::string &id, const std::string &name, const std::string &avatar);
    void session_notify(ChatMessage::ptr msg, WSSession::ptr session = nullptr);
    int32_t SendMessage(WSSession::ptr session, WSFrameMessage::ptr msg);
    int32_t SendMessage(WSSession::ptr session, ChatMessage::ptr msg);
    void session_del(const std::string& id);
    std::string session_find(WSSession::ptr session);
    void session_add(const std::string& id, WSSession::ptr session);
    bool session_exists(const std::string& id);

private:
    chat::RWMutex m_mutex;
    std::map<std::string, WSSession::ptr> m_sessions;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_users;

};

}
}

#endif