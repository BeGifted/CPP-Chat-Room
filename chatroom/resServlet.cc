#include "resServlet.h"
#include <chat/log.h>
#include <iostream>
#include <fstream>

namespace chat {
namespace http {

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

ResourceServlet::ResourceServlet(const std::string& path)
    :Servlet("ResourceServlet")
    ,m_path(path) {
}

int32_t ResourceServlet::handle(chat::http::HttpRequest::ptr request
                           , chat::http::HttpResponse::ptr response
                           , chat::http::HttpSession::ptr session) {
    auto path = m_path + "/" + request->getPath();
    CHAT_LOG_INFO(g_logger) << "handle path=" << path;
    if (path.find("..") != std::string::npos) {
        response->setBody("invalid path");
        response->setStatus(chat::http::HttpStatus::NOT_FOUND);
        return 0;
    } 
    std::ifstream ifs(path);
    if (!ifs) {
        response->setBody("invalid file");
        response->setStatus(chat::http::HttpStatus::NOT_FOUND);
        return 0;
    }

    std::stringstream ss;
    std::string line;
    while (std::getline(ifs, line)) {
        ss << line << std::endl;
    }

    response->setBody(ss.str());
    response->setHeader("content-type", "text/html;charset=utf-8");
    return 0;
}

}
}