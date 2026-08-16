#include "send_otp_handler.hpp"

#include <format>
#include <random>

#include <proto/auth.pb.h>
#include <proto/common.pb.h>

#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_status.hpp>

namespace priemman::auth {

namespace {

constexpr std::string_view kOtpPlaceholder = "{{OTP_CODE}}";

constexpr std::string_view kOtpEmailTemplate = R"HTML(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Kode Verifikasi Priemman</title>
</head>
<body style="margin:0; padding:0; background-color:#f4f4f7; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;">

<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="background-color:#f4f4f7; padding: 40px 0;">
  <tr>
    <td align="center">

      <table role="presentation" width="480" cellpadding="0" cellspacing="0" style="background-color:#ffffff; border-radius: 12px; overflow:hidden; box-shadow: 0 2px 8px rgba(0,0,0,0.06);">

        <tr>
          <td style="background-color:#111827; padding: 32px 40px; text-align:center;">
            <span style="font-size:20px; font-weight:700; color:#ffffff; letter-spacing: -0.5px;">Priemman Studio</span>
          </td>
        </tr>

        <tr>
          <td style="padding: 40px 40px 24px 40px;">
            <p style="margin:0 0 8px 0; font-size:15px; color:#6b7280;">Halo,</p>
            <h1 style="margin:0 0 16px 0; font-size:22px; color:#111827; font-weight:700;">Kode verifikasi kamu</h1>
            <p style="margin:0 0 28px 0; font-size:15px; line-height:1.6; color:#4b5563;">
              Gunakan kode di bawah ini untuk masuk ke akun Priemman Studio kamu. Kode ini berlaku selama <strong>5 menit</strong>.
            </p>
          </td>
        </tr>

        <tr>
          <td style="padding: 0 40px 32px 40px;">
            <table role="presentation" width="100%" cellpadding="0" cellspacing="0">
              <tr>
                <td align="center" style="background-color:#f9fafb; border: 1px solid #e5e7eb; border-radius: 10px; padding: 24px;">
                  <span style="font-size:36px; font-weight:700; letter-spacing: 10px; color:#111827; font-family: 'Courier New', monospace;">
                    {{OTP_CODE}}
                  </span>
                </td>
              </tr>
            </table>
          </td>
        </tr>

        <tr>
          <td style="padding: 0 40px 32px 40px;">
            <p style="margin:0; font-size:13px; line-height:1.6; color:#9ca3af;">
              Jangan bagikan kode ini kepada siapa pun, termasuk pihak yang mengaku dari tim Priemman. Jika kamu tidak meminta kode ini, abaikan email ini.
            </p>
          </td>
        </tr>

        <tr>
          <td style="padding: 0 40px;">
            <hr style="border:none; border-top:1px solid #e5e7eb; margin:0;">
          </td>
        </tr>

        <tr>
          <td style="padding: 24px 40px 32px 40px;" align="center">
            <p style="margin:0; font-size:12px; color:#9ca3af;">
              &copy; 2026 Priemman Studio. Semua hak dilindungi.
            </p>
          </td>
        </tr>

      </table>

    </td>
  </tr>
</table>

</body>
</html>
)HTML";

}  // namespace

std::string SendOtpHandler::GenerateOtpCode() {
    // TODO: pertimbangkan crypto-secure RNG (mis. std::random_device penuh,
    // bukan hanya sebagai seed) untuk production.
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(100000, 999999);
    return std::to_string(dist(rng));
}

std::string SendOtpHandler::BuildOtpEmailHtml(const std::string& otp_code) {
    std::string html{kOtpEmailTemplate};
    auto pos = html.find(kOtpPlaceholder);
    if (pos != std::string::npos) {
        html.replace(pos, kOtpPlaceholder.length(), otp_code);
    }
    return html;
}

std::string SendOtpHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {

    auto body = request.RequestBody();
    priemman::v1::SendOtpRequest request_body;

    if (!request_body.ParseFromString(body)) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        userver::formats::json::ValueBuilder builder;
        builder["error"] = "Invalid request body";
        return userver::formats::json::ToString(builder.ExtractValue());
    }

    if (request_body.email().empty()) {
        request.GetHttpResponse().SetStatus(
            userver::server::http::HttpStatus::kBadRequest);
        userver::formats::json::ValueBuilder builder;
        builder["error"] = "Email is required";
        return userver::formats::json::ToString(builder.ExtractValue());
    }

    const std::string otp_code = GenerateOtpCode();
    const std::string email_html = BuildOtpEmailHtml(otp_code);

    LOG_INFO() << "Generated OTP for " << request_body.email();

    // TODO: hash otp_code (bcrypt) & simpan ke tabel OtpRecord (MySQL)
    // dengan expiresAt (TTL 5 menit) & attempts = 0, sebelum kirim email.

    _smtp_component->SendEmailAsync(
        request_body.email(),
        "Kode Verifikasi Priemman Studio",
        email_html
    );

    priemman::v1::SendOtpResponse response;
    response.set_success(true);
    response.set_message(std::format("Email sent to {}", request_body.email()));
    response.set_cooldown_seconds(30);

    request.GetHttpResponse().SetContentType("application/x-protobuf");
    return response.SerializeAsString();
}

}
