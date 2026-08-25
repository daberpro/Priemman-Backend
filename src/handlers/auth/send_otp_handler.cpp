#include "send_otp_handler.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <random>

#include <openssl/rand.h>
#include <proto/auth.pb.h>
#include <src/handlers/api_errors.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_status.hpp>

namespace priemman::auth {

namespace {
constexpr std::string_view kOtpPlaceholder = "{{OTP_CODE}}";
constexpr std::string_view kEmailPlaceholder = "{{EMAIL}}";
constexpr std::string_view kDefaultEmailTemplatePath = "../templates/otp_email.html";

// Fallback sederhana bila file template tidak ditemukan saat startup
constexpr std::string_view kFallbackEmailTemplate = R"HTML(
<!DOCTYPE html>
<html lang="id">
<head><meta charset="UTF-8"><title>Kode Verifikasi Priemman</title></head>
<body style="margin:0; padding:0; background-color:#f4f4f7; font-family:sans-serif;">
<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="background-color:#f4f4f7; padding:40px 0;">
<tr><td align="center">
<table role="presentation" width="480" cellpadding="0" cellspacing="0" style="background-color:#ffffff; border-radius:12px; overflow:hidden;">
<tr><td style="background-color:#111827; padding:32px 40px; text-align:center;">
<span style="font-size:20px; font-weight:700; color:#ffffff;">Priemman Studio</span>
</td></tr>
<tr><td style="padding:40px;">
<h1 style="margin:0 0 16px 0; font-size:22px; color:#111827;">Kode verifikasi kamu</h1>
<p style="margin:0 0 28px 0; font-size:15px; color:#4b5563;">Gunakan kode di bawah ini untuk masuk. Berlaku selama <strong>5 menit</strong>.</p>
<div style="background-color:#f9fafb; border:1px solid #e5e7eb; border-radius:10px; padding:24px; text-align:center;">
<span style="font-size:36px; font-weight:700; letter-spacing:10px; color:#111827; font-family:monospace;">{{OTP_CODE}}</span>
</div>
</td></tr>
</table>
</td></tr>
</table>
</body></html>
)HTML";

std::string NormalizeEmail(std::string email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return email;
}

void ReplaceAllOccurrences(std::string& haystack, std::string_view needle, std::string_view replacement) {
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        haystack.replace(pos, needle.length(), replacement);
        pos += replacement.length();
    }
}

std::string LoadEmailTemplate(const userver::components::ComponentConfig& config) {
    const auto template_path = config["email-template-path"].As<std::string>(
        std::string{kDefaultEmailTemplatePath});

    std::ifstream template_file{template_path};
    if (!template_file.is_open()) {
        LOG_ERROR() << "OTP email template not found at '" << template_path
                    << "', falling back to built-in template";
        return std::string{kFallbackEmailTemplate};
    }

    std::string content{
        std::istreambuf_iterator<char>{template_file},
        std::istreambuf_iterator<char>{}};
    if (content.empty()) {
        LOG_ERROR() << "OTP email template at '" << template_path
                    << "' is empty, falling back to built-in template";
        return std::string{kFallbackEmailTemplate};
    }

    LOG_INFO() << "Loaded OTP email template from " << template_path;
    return content;
}
}  // namespace

SendOtpHandler::SendOtpHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::server::handlers::HttpHandlerBase(config, context),
      _mysql_cluster(
          context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
      ),
      _smtp_component(
          &context.FindComponent<daberdev::components::SMTPClientComponent>("daberdev-smtp-component-client")
      ),
      _otp_repo(&_mysql_cluster),
      _email_template(LoadEmailTemplate(config)) {
}

userver::yaml_config::Schema SendOtpHandler::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::server::handlers::HttpHandlerBase>(R"(
        type: object
        description: Send OTP handler config
        additionalProperties: false
        properties:
            email-template-path:
                type: string
                description: Path to the OTP email HTML template file
    )");
}

std::string SendOtpHandler::GenerateOtpCode() {
    unsigned char bytes[4];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(100000, 999999);
        return std::to_string(dist(rng));
    }
    std::uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(bytes));
    return std::to_string(100000 + (value % 900000));
}

std::string SendOtpHandler::BuildOtpEmailHtml(const std::string& email, const std::string& otp_code) const {
    std::string html{_email_template};
    ReplaceAllOccurrences(html, kOtpPlaceholder, otp_code);
    ReplaceAllOccurrences(html, kEmailPlaceholder, email);
    return html;
}

std::string SendOtpHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& http_response = request.GetHttpResponse();

    priemman::v1::SendOtpRequest request_body;
    if (!request_body.ParseFromString(request.RequestBody())) {
        http_response.SetStatus(userver::server::http::HttpStatus::kBadRequest);
        http_response.SetContentType(errors::kProtobufContentType);
        return errors::BuildErrorResult("INVALID_REQUEST_BODY");
    }

    const std::string email = NormalizeEmail(request_body.email());
    if (email.empty()) {
        http_response.SetStatus(userver::server::http::HttpStatus::kBadRequest);
        http_response.SetContentType(errors::kProtobufContentType);
        return errors::BuildErrorResult("EMAIL_REQUIRED");
    }

    const std::string otp_code = GenerateOtpCode();
    const std::string ip_address = request.GetRemoteAddress().PrimaryAddressString();

    // ====== SIMPAN KE DATABASE ======
    const bool created = _otp_repo.CreateChallenge(
        email,
        database::OtpRepository::HashOtp(otp_code),
        ip_address,
        std::chrono::seconds{300},
        std::chrono::seconds{30}
    );

    priemman::v1::SendOtpResponse response;
    response.set_cooldown_seconds(30);

    if (!created) {
        http_response.SetStatus(userver::server::http::HttpStatus::kTooManyRequests);
        response.set_success(false);
        response.set_message("Too many requests, please wait before requesting a new code");
        http_response.SetContentType("application/x-protobuf");
        return response.SerializeAsString();
    }

    LOG_INFO() << "Generated OTP for " << email;

    _smtp_component->SendEmailAsync(
        email,
        "Kode Verifikasi Priemman Studio",
        BuildOtpEmailHtml(email, otp_code)
    );

    response.set_success(true);
    response.set_message(std::format("Email sent to {}", email));
    http_response.SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}  // namespace priemman::auth
