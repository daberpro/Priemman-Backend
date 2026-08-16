#include "oauth_initiate_handler.hpp"
namespace priemman::auth {

    std::string InitiateGoogleOAuthHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const {
            _oauth_google_component->Auth(request);
            return "";
    };

    std::string InitiateGithubOAuthHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const {
            _oauth_github_component->Auth(request);
            return "";
    };

}
