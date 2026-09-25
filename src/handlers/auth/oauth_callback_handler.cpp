#include "oauth_callback_handler.hpp"

#include "session_cookie.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <ranges>

#include <src/handlers/api_errors.hpp>
#include <src/handlers/utils.hpp>

#include <userver/formats/json.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_response_cookie.hpp>

namespace priemman::auth {

namespace {

using userver::formats::json::Value;

std::string JsonString(const Value& doc, const char* key) {
    if (!doc.HasMember(key)) {
        return "";
    }

    const auto node = doc[key];

    if (node.IsNull()) {
        return "";
    }

    return node.As<std::string>();
}

bool JsonBool(
    const Value& doc,
    const char* key,
    bool default_value = false
) {
    if (!doc.HasMember(key)) {
        return default_value;
    }

    const auto node = doc[key];

    if (node.IsNull()) {
        return default_value;
    }

    return node.As<bool>();
}

std::string NormalizeEmail(std::string email) {
    std::ranges::transform(email, email.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return email;
}

std::string ErrorResult(
    userver::server::http::HttpResponse& res,
    userver::server::http::HttpStatus status,
    std::string_view code
) {
    res.SetStatus(status);
    res.SetContentType(errors::kProtobufContentType);

    return errors::BuildErrorResult(code);
}

constexpr std::string_view kFirstNamePlaceholder{"{{first_name}}"};
constexpr std::string_view kLoginTImePlaceholder{"{{login_time}}"};
constexpr std::string_view kDevicePlaceholder{"{{device}}"};
constexpr std::string_view kIpAddressPlaceholder{"{{ip_address}}"};

constexpr std::string_view kFallbackEmailTemplate{
    R"HTML(
<h2>Login Pertama Berhasil</h2>

<p>Halo, {{first_name}},</p>

<p>
    Selamat datang di Priemman.
</p>

<p>
    Kami mendeteksi bahwa akun Priemman Anda berhasil digunakan
    untuk login untuk pertama kalinya.
</p>

<div>
    <strong>Waktu</strong><br>
    {{login_time}}
</div>

<div>
    <strong>Perangkat</strong><br>
    {{device}}
</div>

<div>
    <strong>Alamat IP</strong><br>
    {{ip_address}}
</div>

<p>
    Jika Anda mengenali aktivitas ini, tidak ada tindakan yang perlu dilakukan.
</p>

<p>
    Jika Anda tidak merasa melakukan login ini, segera ubah kata sandi
    akun Anda dan pastikan akun Anda tetap aman.
</p>

<p>
    Salam hangat,<br>
    <strong>Tim Priemman</strong>
</p>
)HTML"
};

void AddDashboardUrlProperties(userver::yaml_config::Schema& schema) {
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }

    static const std::pair<std::string_view, std::string_view> kProps[] = {
        {"dashboard-url", "Dashboard URL for regular users after successful login"},
        {"dashboard-creator-url", "Dashboard URL for creators after successful login"},
        {"dashboard-admin-url", "Dashboard URL for admins after successful login"},
        {"domain", "Domain utama atau base domain contoh priemman.my.id"},
        {"welcome-template-path", "Template html untuk welcome user"},
        {"jwt-secret", "Secret JWT untuk token comments"}
    };

    for (const auto& [name, description] : kProps) {
        schema.properties->emplace(
            std::string{name},
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: " +
                    std::string{description} +
                    "\n"
                )
            )
        );
    }
}

void BuildWelcomeEmailHtml(
    std::string& html,
    const std::string& first_name,
    const std::string& login_time,
    const std::string& device,
    const std::string& ip_address
) {
    priemman::utils::ReplaceAllOccurrences(
        html,
        kFirstNamePlaceholder,
        first_name
    );

    priemman::utils::ReplaceAllOccurrences(
        html,
        kLoginTImePlaceholder,
        login_time
    );

    priemman::utils::ReplaceAllOccurrences(
        html,
        kDevicePlaceholder,
        device
    );

    priemman::utils::ReplaceAllOccurrences(
        html,
        kIpAddressPlaceholder,
        ip_address
    );
}

}

DashboardUrls DashboardUrls::FromConfig(
    const userver::components::ComponentConfig& config
) {
    return DashboardUrls{
        config["dashboard-url"].As<std::string>(),
        config["dashboard-creator-url"].As<std::string>(),
        config["dashboard-admin-url"].As<std::string>(),
    };
}

const std::string& DashboardUrls::ForRole(const std::string& role) const {
    if (role == "admin") {
        return admin;
    }

    if (role == "creator") {
        return creator;
    }

    return user;
}

userver::yaml_config::Schema
OAuthGoogleCallbackHandler::GetStaticConfigSchema() {
    auto schema =
        userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();

    AddDashboardUrlProperties(schema);

    return schema;
}

userver::yaml_config::Schema
OAuthGithubCallbackHandler::GetStaticConfigSchema() {
    auto schema =
        userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();

    AddDashboardUrlProperties(schema);

    return schema;
}

OAuthGoogleCallbackHandler::OAuthGoogleCallbackHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _domain{config["domain"].As<std::string>()},
      _welcome_template{config["welcome-template-path"].As<std::string>()},
      _jwt_secret{config["jwt-secret"].As<std::string>()},
      _mysql_cluster{
          context
              .FindComponent<userver::storages::mysql::Component>("database")
              .GetCluster()
      },
      _users{&_mysql_cluster},
      _sessions{&_mysql_cluster},
      _oauth_google_component{
          &context.FindComponent<
              daberdev::components::OAuthGoogleComponent
          >(daberdev::components::OAuthGoogleComponent::kName)
      },
      _dashboards{DashboardUrls::FromConfig(config)},
      _smtp_component{
          &context.FindComponent<
              daberdev::components::SMTPClientComponent
          >(daberdev::components::SMTPClientComponent::kName)
      } {}

std::string OAuthGoogleCallbackHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();

    const auto url_state = std::string{request.GetArg("state")};
    const std::string cookie_state = request.GetCookie("oauth_state");

    if (
        url_state.empty() ||
        cookie_state.empty() ||
        url_state != cookie_state
    ) {
        return ErrorResult(
            res,
            userver::server::http::HttpStatus::kBadRequest,
            "INVALID_OR_EXPIRED_STATE"
        );
    }

    const auto doc = userver::formats::json::FromString(
        _oauth_google_component->GetData(request)
    );

    database::OAuthUserData oauth;

    oauth.provider = "GOOGLE";
    oauth.provider_user_id = JsonString(doc, "sub");
    oauth.email = NormalizeEmail(JsonString(doc, "email"));
    oauth.name = JsonString(doc, "name");
    oauth.avatar_url = JsonString(doc, "picture");
    oauth.email_verified = JsonBool(doc, "email_verified");

    if (
        oauth.provider_user_id.empty() ||
        oauth.email.empty()
    ) {
        return ErrorResult(
            res,
            userver::server::http::HttpStatus::kUnauthorized,
            "OAUTH_PROFILE_INCOMPLETE"
        );
    }

    auto result = _users.FindOrCreateFromOAuth(oauth);

    if (result.is_new_user) {
        std::string html = priemman::utils::LoadEmailTemplate(
            std::string{_welcome_template},
            std::string{kFallbackEmailTemplate}
        );

        BuildWelcomeEmailHtml(
            html,
            result.user.first_name,
            std::format(
                "{:%Y-%m-%dT%H:%M:%SZ}",
                std::chrono::time_point_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now()
                )
            ),
            request.GetHeader("User-Agent"),
            request.GetHeader("X-Real-IP")
        );

        _smtp_component->SendEmailAsync(
            oauth.email,
            "Welcome To Priemman",
            html
        );
    }

    auto session = _sessions.Create(result.user.id);

    // kunci ini harus sama persis dengan variabel SECRET di server Remark42
    auto token = jwt::create()
    .set_audience("priemman")
    .set_issued_at(std::chrono::system_clock::now())
    .set_expires_at(
        std::chrono::system_clock::now() + std::chrono::hours(24)
    )
    .set_payload_claim(
        "user",
        jwt::claim(picojson::value(picojson::object{
            {"id", picojson::value(result.user.id)},
            {"name", picojson::value(
                std::format(
                    "{} {}",
                    result.user.first_name,
                    result.user.last_name
                )
            )},
            {"picture", picojson::value(result.user.avatar_url)},
            {"email", picojson::value(result.user.email)}
        }))
    )
    .sign(jwt::algorithm::hs256{_jwt_secret});

    userver::server::http::Cookie jwt_cookie{"JWT", token};
    jwt_cookie.SetDomain("." + _domain); 
    jwt_cookie.SetPath("/");
    jwt_cookie.SetHttpOnly();
    jwt_cookie.SetSecure();
    jwt_cookie.SetSameSite("Lax");
    res.SetCookie(jwt_cookie);

    userver::server::http::Cookie xsrf_cookie{"XSRF-TOKEN",  userver::utils::generators::GenerateUuid()};
    xsrf_cookie.SetDomain("." + _domain);
    xsrf_cookie.SetPath("/");
    // xsrf_cookie.SetHttpOnly();
    xsrf_cookie.SetSecure();
    xsrf_cookie.SetSameSite("Lax");
    res.SetCookie(xsrf_cookie);

    res.SetHeader(
        std::string("Set-Cookie"),
        BuildSessionCookie(session.token, _domain)
    );

    userver::server::http::Cookie clear_cookie{"oauth_state", ""};

    clear_cookie.SetMaxAge(std::chrono::seconds(0));
    clear_cookie.SetPath("/");

    res.SetCookie(clear_cookie);

    res.SetStatus(userver::server::http::HttpStatus::kFound);
    res.SetHeader(
        std::string("Location"),
        _dashboards.ForRole(result.user.role)
    );

    return "";
}

OAuthGithubCallbackHandler::OAuthGithubCallbackHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _domain{config["domain"].As<std::string>()},
      _welcome_template{config["welcome-template-path"].As<std::string>()},
      _jwt_secret{config["jwt-secret"].As<std::string>()},
      _mysql_cluster{
          context
              .FindComponent<userver::storages::mysql::Component>("database")
              .GetCluster()
      },
      _users{&_mysql_cluster},
      _sessions{&_mysql_cluster},
      _oauth_github_component{
          &context.FindComponent<
              daberdev::components::OAuthGithubComponent
          >(daberdev::components::OAuthGithubComponent::kName)
      },
      _dashboards{DashboardUrls::FromConfig(config)},
      _smtp_component{
          &context.FindComponent<
              daberdev::components::SMTPClientComponent
          >(daberdev::components::SMTPClientComponent::kName)
      } {}

std::string OAuthGithubCallbackHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();

    const auto url_state = std::string{request.GetArg("state")};
    const std::string cookie_state = request.GetCookie("oauth_state");

    if (
        url_state.empty() ||
        cookie_state.empty() ||
        url_state != cookie_state
    ) {
        return ErrorResult(
            res,
            userver::server::http::HttpStatus::kBadRequest,
            "INVALID_OR_EXPIRED_STATE"
        );
    }

    const auto doc = userver::formats::json::FromString(
        _oauth_github_component->GetData(request)
    );

    database::OAuthUserData oauth;

    oauth.provider = "GITHUB";

    if (doc.HasMember("id") && !doc["id"].IsNull()) {
        const auto& id_node = doc["id"];

        oauth.provider_user_id =
            id_node.IsString()
                ? id_node.As<std::string>()
                : std::to_string(id_node.As<std::int64_t>());
    }

    const auto login = JsonString(doc, "login");

    oauth.email = NormalizeEmail(JsonString(doc, "email"));
    oauth.email_verified = JsonBool(doc, "email_verified");

    if (
        oauth.email.empty() &&
        !oauth.provider_user_id.empty()
    ) {
        oauth.email =
            oauth.provider_user_id +
            "+" +
            login +
            "@users.noreply.github.com";

        oauth.email_verified = true;
    }

    oauth.name = JsonString(doc, "name");

    if (oauth.name.empty()) {
        oauth.name = login;
    }

    oauth.avatar_url = JsonString(doc, "avatar_url");

    if (
        oauth.provider_user_id.empty() ||
        oauth.email.empty()
    ) {
        return ErrorResult(
            res,
            userver::server::http::HttpStatus::kUnauthorized,
            "OAUTH_PROFILE_INCOMPLETE"
        );
    }

    auto result = _users.FindOrCreateFromOAuth(oauth);

    if (result.is_new_user) {
        std::string html = priemman::utils::LoadEmailTemplate(
            std::string{_welcome_template},
            std::string{kFallbackEmailTemplate}
        );

        BuildWelcomeEmailHtml(
            html,
            result.user.first_name,
            std::format(
                "{:%Y-%m-%dT%H:%M:%SZ}",
                std::chrono::time_point_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now()
                )
            ),
            request.GetHeader("User-Agent"),
            request.GetHeader("X-Real-IP")
        );

        _smtp_component->SendEmailAsync(
            oauth.email,
            "Welcome To Priemman",
            html
        );
    }

    auto session = _sessions.Create(result.user.id);

    auto token = jwt::create()
    .set_audience("priemman")
    .set_issued_at(std::chrono::system_clock::now())
    .set_expires_at(
        std::chrono::system_clock::now() + std::chrono::hours(24)
    )
    .set_payload_claim(
        "user",
        jwt::claim(picojson::value(picojson::object{
            {"id", picojson::value(result.user.id)},
            {"name", picojson::value(
                std::format(
                    "{} {}",
                    result.user.first_name,
                    result.user.last_name
                )
            )},
            {"picture", picojson::value(result.user.avatar_url)},
            {"email", picojson::value(result.user.email)}
        }))
    )
    .sign(jwt::algorithm::hs256{_jwt_secret});

    userver::server::http::Cookie jwt_cookie{"JWT", token};
    jwt_cookie.SetDomain("." + _domain); 
    jwt_cookie.SetPath("/");
    jwt_cookie.SetHttpOnly();
    jwt_cookie.SetSecure();
    jwt_cookie.SetSameSite("Lax");
    res.SetCookie(jwt_cookie);

    userver::server::http::Cookie xsrf_cookie{"XSRF-TOKEN",  userver::utils::generators::GenerateUuid()};
    xsrf_cookie.SetDomain("." + _domain);
    xsrf_cookie.SetPath("/");
    // xsrf_cookie.SetHttpOnly();
    xsrf_cookie.SetSecure();
    xsrf_cookie.SetSameSite("Lax");
    res.SetCookie(xsrf_cookie);

    res.SetHeader(
        std::string("Set-Cookie"),
        BuildSessionCookie(session.token, _domain)
    );

    userver::server::http::Cookie clear_cookie{"oauth_state", ""};

    clear_cookie.SetMaxAge(std::chrono::seconds(0));
    clear_cookie.SetPath("/");

    res.SetCookie(clear_cookie);

    res.SetStatus(userver::server::http::HttpStatus::kFound);
    res.SetHeader(
        std::string("Location"),
        _dashboards.ForRole(result.user.role)
    );

    return "";
}

}