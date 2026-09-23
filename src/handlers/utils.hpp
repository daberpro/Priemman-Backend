#pragma once
#include <charconv>
#include <expected>
#include <proto/common.pb.h>
#include <proto/user.pb.h>
#include <userver/server/http/http_request.hpp>
#include <userver/components/component_config.hpp>
#include <userver/logging/log.hpp>
#include <fstream>
#include <src/handlers/api_errors.hpp>

constexpr std::int64_t kDefaultPageSize = 20;
constexpr std::int64_t kMaxPageSize = 50;
constexpr std::size_t kMaxInt64Digits = 19;

namespace priemman::utils {
inline std::string BuildResult(std::string_view message) {
  priemman::v1::Result result;
  result.set_is_error(false);
  result.set_message(std::string{message});
  return result.SerializeAsString();
}

inline std::expected<int64_t, std::string> ParseInt(std::string_view value) {
  int result{};
  auto [ptr, ec] =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (ec != std::errc{} || ptr != value.data() + value.size()) {
    return std::unexpected<std::string>("Invalid integer");
  }
  return result;
}


[[nodiscard("ParsePageSize return value cannot be ignored")]]
inline std::int64_t ParsePageSize(
    const userver::server::http::HttpRequest& request
) {
    const auto arg = request.GetArg("page_size");

    if (arg.empty() || arg.size() > kMaxInt64Digits) {
        return kDefaultPageSize;
    }

    std::int64_t value{};

    const auto [ptr, ec] =
        std::from_chars(
            arg.data(),
            arg.data() + arg.size(),
            value
        );

    if (ec != std::errc{} || ptr != arg.data() + arg.size()) {
        return kDefaultPageSize;
    }

    if (value <= 0) {
        return kDefaultPageSize;
    }

    return value > kMaxPageSize ? kMaxPageSize : value;
}

[[nodiscard("ParseOffset return value cannot be ignored")]]
inline std::int64_t ParseOffset(
    const userver::server::http::HttpRequest& request
) {
    const auto token = request.GetArg("page_token");

    if (token.empty() || token.size() > kMaxInt64Digits) {
        return 0;
    }

    std::int64_t value{};

    const auto [ptr, ec] =
        std::from_chars(
            token.data(),
            token.data() + token.size(),
            value
        );

    if (ec != std::errc{} || ptr != token.data() + token.size()) {
        return 0;
    }

    return value < 0 ? 0 : value;
}

[[nodiscard("Normalize result are not used")]]
inline std::string NormalizeEmail(std::string email) {
    std::transform(email.begin(), email.end(), email.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return email;
}

inline void ReplaceAllOccurrences(std::string& haystack, std::string_view needle, std::string_view replacement) {
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        haystack.replace(pos, needle.length(), replacement);
        pos += replacement.length();
    }
}

[[nodiscard("Email template are not used")]]
inline std::string LoadEmailTemplate(const std::string& template_path, const std::string& FallbackEmailTemplate) {
    std::ifstream template_file{template_path};
    if (!template_file.is_open()) {
        LOG_ERROR() << "template not found at '" << template_path
                    << "', falling back to built-in template";
        return FallbackEmailTemplate;
    }

    std::string content{
        std::istreambuf_iterator<char>{template_file},
        std::istreambuf_iterator<char>{}
    };
    if (content.empty()) {
        LOG_ERROR() << "template at '" << template_path
                    << "' is empty, falling back to built-in template";
        return FallbackEmailTemplate;
    }

    LOG_INFO() << "Loaded template from " << template_path;
    return content;
}

inline std::string ErrorResult(
    const std::string& code,
    const std::string& message
) {
    return errors::BuildErrorResult(code, message);
}


} // namespace priemman::utils
