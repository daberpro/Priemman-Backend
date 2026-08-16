#include "ping.hpp"
namespace priemman {

    std::string PingHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const {
            return "pong";
    }

}
