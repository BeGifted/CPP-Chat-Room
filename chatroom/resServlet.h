#ifndef __CHAT_HTTP_RESOURCE_SERVLET_H__
#define __CHAT_HTTP_RESOURCE_SERVLET_H__

#include <chat/http/servlet.h>

namespace chat {
namespace http {

class ResourceServlet :public chat::http::Servlet {
public:
    typedef std::shared_ptr<ResourceServlet> ptr;
    ResourceServlet(const std::string& path);
    virtual int32_t handle(chat::http::HttpRequest::ptr request
                   , chat::http::HttpResponse::ptr response
                   , chat::http::HttpSession::ptr session) override;

private:
    std::string m_path;
};

}
}

#endif