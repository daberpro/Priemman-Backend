#include "verify_otp_handler.hpp"
#include "session_cookie.hpp"
#include "src/database/otp_repository.hpp"

#include <algorithm>
#include <cctype>

#include <proto/auth.pb.h>
#include <string>
#include <src/handlers/api_errors.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <src/handlers/utils.hpp>

namespace priemman::auth {

namespace {

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

std::string NormalizeEmail(std::string email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return email;
}

void AddProperties(userver::yaml_config::Schema& schema) {
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }
    static const std::pair<std::string_view, std::string_view> kProps[] = {
        {"domain","Domain utama atau base domain contoh priemman.my.id"},
        {"welcome-template-path","Template html untuk welcome user"},
        {"jwt-secret", "Secret JWT untuk token comments"}
    };
    for (const auto& [name, description] : kProps) {
        schema.properties->emplace(
            std::string{name},
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: " + std::string{description} + "\n")
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
    priemman::utils::ReplaceAllOccurrences(html, kFirstNamePlaceholder, first_name);
    priemman::utils::ReplaceAllOccurrences(html, kLoginTImePlaceholder, login_time);
    priemman::utils::ReplaceAllOccurrences(html, kDevicePlaceholder, device);
    priemman::utils::ReplaceAllOccurrences(html, kIpAddressPlaceholder, ip_address);
}

}  // namespace

userver::yaml_config::Schema VerifyOtpHandler::GetStaticConfigSchema() {
    auto schema = userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();
    AddProperties(schema);
    return schema;
}

VerifyOtpHandler::VerifyOtpHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _domain{config["domain"].As<std::string>()},
      _welcome_template(config["welcome-template-path"].As<std::string>()),
      _jwt_secret{config["jwt-secret"].As<std::string>()},
      _mysql_cluster(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ),
      _otp_repo(&_mysql_cluster),
      _users(&_mysql_cluster),
      _sessions(&_mysql_cluster),
      _smtp_component( &context.FindComponent<daberdev::components::SMTPClientComponent>(daberdev::components::SMTPClientComponent::kName)) {
}

std::string VerifyOtpHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();

    const auto error_response = [&res](
        userver::server::http::HttpStatus status,
        std::string_view code
    ) {
        res.SetStatus(status);
        res.SetContentType(errors::kProtobufContentType);
        return errors::BuildErrorResult(code);
    };

    priemman::v1::VerifyOtpRequest req;
    if (!req.ParseFromString(request.RequestBody())) {
        return error_response(
            userver::server::http::HttpStatus::kBadRequest,
            "INVALID_REQUEST_BODY");
    }

    const std::string email = NormalizeEmail(req.email());
    const std::string otp_hash = req.otp();
    const std::string ip_address = request.GetRemoteAddress().PrimaryAddressString();

    const database::OtpResult valid = _otp_repo.VerifyAndConsume(email, otp_hash, ip_address);

    switch (valid) {
        case database::OtpResult::kInvalidCode: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "INVALID_OTP_CODE");
        }
        case database::OtpResult::kNotFoundOrExpired: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "INVALID_OR_EXPIRED_OTP");
        }
        case database::OtpResult::kEmailSuspended: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "EMAIL_SUSPENDED");
        }
        case database::OtpResult::kIpSuspended: {
            return error_response(
                userver::server::http::HttpStatus::kUnauthorized,
                "IP_SUSPENDED");
        }
        case database::OtpResult::kSuccess: {
            break;
        }
    }

    auto result = _users.FindOrCreateFromEmail(email);
    if(result.is_new_user){
        std::string html = priemman::utils::LoadEmailTemplate(std::string{_welcome_template}, std::string{kFallbackEmailTemplate});
        BuildWelcomeEmailHtml(
            html, 
            result.user.first_name, 
            std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now())),
            request.GetHeader("User-Agent"), 
            request.GetHeader("X-Real-IP")
        );
        _smtp_component->SendEmailAsync(
            email,
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

    userver::server::http::Cookie xsrf_cookie{"XSRF-TOKEN",  userver::utils::generators::GenerateUuid()};
    xsrf_cookie.SetDomain("." + _domain);
    xsrf_cookie.SetPath("/");
    // xsrf_cookie.SetHttpOnly();
    xsrf_cookie.SetSecure();
    xsrf_cookie.SetSameSite("Lax");
    res.SetCookie(xsrf_cookie);

    userver::server::http::Cookie jwt_cookie{"JWT", token};
    jwt_cookie.SetDomain("." + _domain); 
    jwt_cookie.SetPath("/");
    jwt_cookie.SetHttpOnly();
    jwt_cookie.SetSecure();
    jwt_cookie.SetSameSite("Lax");
    res.SetCookie(jwt_cookie);

    res.SetHeader(std::string("Set-Cookie"), BuildSessionCookie(session.token, _domain));

    priemman::v1::VerifyOtpResponse response;
    response.set_session_token(session.token);
    response.set_is_new_user(result.is_new_user);

    res.SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}  // namespace priemman::auth
