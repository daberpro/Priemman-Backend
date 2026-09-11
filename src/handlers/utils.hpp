#pragma once
#include <proto/common.pb.h>
#include <proto/user.pb.h>
#include <charconv>
#include <expected>

namespace priemman::utils {
    inline std::string BuildResult(std::string_view message){
        priemman::v1::Result result;
        result.set_is_error(false);
        result.set_message(std::string{message});
        return result.SerializeAsString();
    }

    inline std::expected<int64_t, std::string> ParseInt(std::string_view value) {
        int result{};
        auto [ptr, ec] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            result
        );
        if (ec != std::errc{} || ptr != value.data() + value.size()) {
            return std::unexpected<std::string>("Invalid integer");
        }
        return result;
    }
}
