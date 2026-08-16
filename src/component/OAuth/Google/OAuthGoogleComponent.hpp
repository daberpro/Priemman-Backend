#pragma once
#include "../OAuthComponent.hpp"
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>

namespace daberdev::components {
    class OAuthGoogleComponent final : public OAuthComponent {
    public:
        static constexpr std::string_view kName{"daberdev-oauth-google-component"};
        explicit OAuthGoogleComponent(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : OAuthComponent(config, context) {}

        std::string GetToken(const userver::server::http::HttpRequest& req) override {
            auto code = req.GetArg("code");

            auto encode = [](const std::string& v) {
                return userver::http::UrlEncode(v);
            };

            const std::string form_body = fmt::format(
                "client_id={}&client_secret={}&code={}&redirect_uri={}&grant_type=authorization_code",
                encode(m_client_id),
                encode(m_client_secret),
                encode(code),
                encode(m_redirect_url)
            );

            auto token = m_client->CreateRequest()
                .url(m_token_url)
                .data(form_body)
                .headers({
                    {"Accept", "application/json"},
                    {"Content-Type", "application/x-www-form-urlencoded"}
                })
                .post()
                .timeout(std::chrono::milliseconds(5000))
                .perform();

            token->raise_for_status();
            auto result = userver::formats::json::FromString(token->body());

            if (result.HasMember("error")) {
                std::string error_msg = result.HasMember("error_description")
                    ? result["error_description"].template As<std::string>()
                    : result["error"].template As<std::string>();
                throw std::runtime_error("Google OAuth Error: " + error_msg);
            }

            return result["access_token"].template As<std::string>();
        }

        std::string GetData(const userver::server::http::HttpRequest& req) override {
            std::string access_token = GetToken(req);
            auto response = m_client->CreateRequest()
                .get(m_api_url)
                .headers({
                    {"Authorization", "Bearer " + access_token},
                    {"Accept", "application/json"}
                })
                .timeout(std::chrono::milliseconds(5000))
                .perform();

            response->raise_for_status();
            return response->body();
        }
    };
}
