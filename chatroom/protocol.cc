#include "protocol.h"
#include <chat/util.h>

namespace chat {
namespace http {

ChatMessage::ptr ChatMessage::Create(const std::string& v) {
    Json::Value json;
    if(!chat::JsonUtil::FromString(json, v)) {
        return nullptr;
    }
    ChatMessage::ptr rt(new ChatMessage);
    auto names = json.getMemberNames();
    for(auto& i : names) {
        rt->m_datas[i] = json[i].asString();
    }
    return rt;
}

ChatMessage::ChatMessage() {
}

std::string ChatMessage::get(const std::string& name) {
    auto it = m_datas.find(name);
    return it == m_datas.end() ? "" : it->second;
}

void ChatMessage::set(const std::string& name, const std::string& val) {
    m_datas[name] = val;
}

std::string ChatMessage::toString() const {
    Json::Value json;
    for(auto& i : m_datas) {
        json[i.first] = i.second;
    }
    return chat::JsonUtil::ToString(json);
}

}
}