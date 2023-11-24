#include "chatServlet.h"
#include <chat/log.h>
#include <chat/util.h>
#include "json.hpp"

namespace chat {
namespace http {

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();


bool ChatWSServlet::session_exists(const std::string& id) {
    CHAT_LOG_INFO(g_logger) << "session_exists id=" << id;
    chat::RWMutex::ReadLock lock(m_mutex);
    auto it = m_sessions.find(id);
    return it != m_sessions.end();
}

void ChatWSServlet::session_add(const std::string& id, WSSession::ptr session) {
    CHAT_LOG_INFO(g_logger) << "session_add id=" << id;
    chat::RWMutex::WriteLock lock(m_mutex);
    m_sessions[id] = session;
}

std::string ChatWSServlet::session_find(WSSession::ptr session) {
    CHAT_LOG_INFO(g_logger) << "session_find session=" << session;
    chat::RWMutex::ReadLock lock(m_mutex);
    for (auto& c : m_sessions) {
        if (c.second == session) {
            return c.first;
        }
    }
    return "";
}

void ChatWSServlet::session_del(const std::string& id) {
    CHAT_LOG_INFO(g_logger) << "session_del del=" << id;
    chat::RWMutex::WriteLock lock(m_mutex);
    m_sessions.erase(id);
    m_users.erase(id);
}

int32_t ChatWSServlet::SendMessage(WSSession::ptr session, ChatMessage::ptr msg) {
    CHAT_LOG_INFO(g_logger) << msg->toString() << " - " << session;
    return session->sendMessage(msg->toString()) > 0 ? 0 : 1;
}

int32_t ChatWSServlet::SendMessage(WSSession::ptr session, WSFrameMessage::ptr msg) {
    CHAT_LOG_INFO(g_logger) << msg->getData() << " - " << session;
    return session->sendMessage(msg) > 0 ? 0 : 1;
}

void ChatWSServlet::session_notify(ChatMessage::ptr msg, WSSession::ptr session) {
    chat::RWMutex::ReadLock lock(m_mutex);
    auto sessions = m_sessions;
    lock.unlock();

    for(auto& i : sessions) {
        if(i.second == session) {
            continue;
        }
        SendMessage(i.second, msg);
    }
}

ChatWSServlet::ChatWSServlet(): WSServlet("chat_servlet") {
    m_users["group"] = std::make_pair("聊天室", "./static/avatar/group.png");
}

int32_t ChatWSServlet::onConnect(HttpRequest::ptr header, WSSession::ptr session) {
    CHAT_LOG_INFO(g_logger) << "on Connect " << session;
    return 0;
}

int32_t ChatWSServlet::onClose(HttpRequest::ptr header, WSSession::ptr session) {
    auto id = header->getHeader("$id");
    CHAT_LOG_INFO(g_logger) << "on Close " << session << " id=" << id;
    if (!id.empty()) {
        session_del(id);
        ChatMessage::ptr nty(new ChatMessage);
        nty->set("type", "user_change_response");
        nty->set("time", chat::Time2Str());
        nty->set("name", id);
        nty->set("id", id);
        nty->set("code", "2");
        session_notify(nty);
    }
    return 0;
}

int32_t ChatWSServlet::handle(HttpRequest::ptr header, WSFrameMessage::ptr msgx, WSSession::ptr session) {
    CHAT_LOG_INFO(g_logger) << "handle " << session
            << " opcode=" << msgx->getOpcode()
            << " data=" << msgx->getData();

    auto msg = ChatMessage::Create(msgx->getData());
    auto id = header->getHeader("$id");
    if(!msg) {
        if(!id.empty()) {
            chat::RWMutex::WriteLock lock(m_mutex);
            m_sessions.erase(id);
        }
        return 1;
    }

    ChatMessage::ptr rsp(new ChatMessage);
    auto type = msg->get("type");
    if (type == "login_request") {
        rsp->set("type", "login_response");
        auto name = msg->get("name");
        auto avatar = msg->get("avatar");
        if (name.empty()) {
            rsp->set("result", "400");
            rsp->set("msg", "name is null");
            return SendMessage(session, rsp);
        }
        if (!id.empty()) {
            rsp->set("result", "401");
            rsp->set("msg", "logined");
            return SendMessage(session, rsp);
        }
        if (session_exists(id)) {
            rsp->set("result", "402");
            rsp->set("msg", "name exists");
            return SendMessage(session, rsp);
        }
        id = name;
        header->setHeader("$id", id);
        rsp->set("id", id);
        rsp->set("result", "200");
        rsp->set("msg", "ok");
        rsp->set("time", chat::Time2Str());
        rsp->set("name", name);
        rsp->set("avatar", avatar);
        session_add(name, session);

        addInfo(id, name, avatar);
        return SendMessage(session, rsp);
    } else if (type == "chat_init_request") {
        nlohmann::json rsp_new;
        rsp_new["type"] = "chat_init_response";
        rsp_new["time"] = chat::Time2Str();
        
        for (const auto& info : m_users) {
            if (info.first == "group") {
                continue;
            }
            nlohmann::json user;
            user["id"] = info.first;
            user["name"] = info.second.first;
            user["avatar"] = info.second.second;
            rsp_new["data"].push_back(user);
        }
        int32_t rt = SendMessage(session, std::make_shared<http::WSFrameMessage>(msgx->getOpcode(), rsp_new.dump()));

        ChatMessage::ptr nty(new ChatMessage);
        auto info = getInfo(id);
        auto name = info.first;
        auto avatar = info.second;
        nty->set("type", "user_change_response");
        nty->set("time", chat::Time2Str());
        nty->set("code", "1");
        nty->set("id", id);
        nty->set("name", name);
        nty->set("avatar", avatar);
        session_notify(nty, session);

        return rt;
    } else if(type == "chat_request") {
        rsp = msg;
        std::cout << msg->toString() << std::endl;
        rsp->set("type", "chat_response");
        rsp->set("server", "server");
        if (id.empty()) {
            rsp->set("result", "501");
            rsp->set("msg", "not login");
            return SendMessage(session, rsp);
        }
        rsp->set("result", "200");

        if (msg->get("to") == "group") {
            session_notify(rsp, session);
        } else {
            auto to_conn = m_sessions[msg->get("to")];
            return SendMessage(to_conn, rsp);
        }
    }
    return 0;
}

std::pair<std::string, std::string> ChatWSServlet::getInfo(const std::string &id) {
    RWMutex::ReadLock lock(m_mutex);
    auto it = m_users.find(id);
    return it == m_users.end() ? std::make_pair("", "") : it->second;
}

void ChatWSServlet::addInfo(const std::string &id, const std::string &name, const std::string &avatar) {
    RWMutex::WriteLock lock(m_mutex);
    m_users[id] = std::make_pair(name, avatar);
}

}
}