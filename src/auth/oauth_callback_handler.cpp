#include "oauth_callback_handler.hpp"
#include <string>
#include <userver/http/content_type.hpp>

namespace priemman::auth {

    std::string OAuthGoogleCallbackHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const {
            auto& res = request.GetHttpResponse();
            res.SetContentType(userver::http::content_type::kApplicationJson);

            // TODO: FindOrCreateUser(email, name, avatar_url, AuthProvider::GOOGLE) — shared logic
            // TODO: generate session token, simpan ke tabel Session
            // TODO: redirect ke frontend dengan Set-Cookie

            // request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kFound);
            // request.GetHttpResponse().SetHeader("Location", "http://localhost:3000/dashboard");

            return _oauth_google_component->GetData(request);
    }

    std::string OAuthGithubCallbackHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const {
            auto& res = request.GetHttpResponse();
            res.SetContentType(userver::http::content_type::kApplicationJson);

            // TODO: FindOrCreateUser(email, name, avatar_url, AuthProvider::GITHUB) — shared logic
            // TODO: generate session token, simpan ke tabel Session
            // TODO: redirect ke frontend dengan Set-Cookie

            // request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kFound);
            // request.GetHttpResponse().SetHeader("Location", "http://localhost:3000/dashboard");

            return _oauth_github_component->GetData(request);
    }

}
