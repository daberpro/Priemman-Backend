#pragma once
#include <charconv>
#include <expected>
#include <proto/common.pb.h>
#include <proto/user.pb.h>
#include <userver/server/http/http_request.hpp>

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

} // namespace priemman::utils
