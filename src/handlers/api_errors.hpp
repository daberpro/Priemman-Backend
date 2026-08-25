#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <proto/common.pb.h>

namespace priemman::errors {

constexpr std::string_view kProtobufContentType = "application/x-protobuf";

using ErrorMessages = std::unordered_map<std::string_view, std::string_view>;

inline const ErrorMessages& Messages() {
    static const ErrorMessages kMessages = {
        {"UNAUTHORIZED", "Missing or invalid session token"},
        {"FORBIDDEN", "You do not have permission to perform this action"},
        {"NOT_FOUND", "Resource not found"},
        {"RATE_LIMITED", "Too many requests, please try again later"},
        {"METHOD_NOT_ALLOWED", "Unsupported method"},
        {"INVALID_REQUEST_BODY", "Invalid request body"},
        {"INVALID_BODY", "Invalid request body"},
        {"INVALID_ID", "Id is required"},
        {"EMAIL_REQUIRED", "Email is required"},
        {"INVALID_OTP_CODE", "Invalid OTP code"},
        {"INVALID_OR_EXPIRED_OTP", "OTP is invalid or has expired"},
        {"EMAIL_SUSPENDED", "This email is temporarily suspended"},
        {"IP_SUSPENDED", "Your IP address is temporarily suspended"},
        {"INVALID_OR_EXPIRED_STATE", "Invalid or expired OAuth state"},
        {"OAUTH_PROFILE_INCOMPLETE", "OAuth profile is incomplete"},
        {"INVALID_TITLE", "Title is required"},
        {"INVALID_MEDIA", "Invalid media"},
        {"MAX_FILES_EXCEEDED", "Too many files in one request"},
        {"INVALID_ROLE", "Role must be user, creator, or admin"},
        {"INVALID_STATUS", "Invalid status"},
        {"ALREADY_CREATOR", "You are already a creator"},
        {"UPGRADE_ALREADY_REQUESTED", "Upgrade request already submitted"},
        {"REVIEW_FAILED", "Review action failed"},
        {"CONFIRM_FAILED", "Confirm action failed"},
    };
    return kMessages;
}

inline std::string MessageFor(std::string_view code) {
    const auto it = Messages().find(code);
    if (it == Messages().end()) {
        return std::string{code};
    }
    return std::string{it->second};
}

inline std::string BuildErrorResult(std::string_view code, std::string_view message) {
    priemman::v1::Result result;
    result.set_is_error(true);
    result.set_message(std::string{message});
    result.mutable_error_detail()->set_code(std::string{code});
    result.mutable_error_detail()->set_message(std::string{message});
    return result.SerializeAsString();
}

inline std::string BuildErrorResult(std::string_view code) {
    return BuildErrorResult(code, MessageFor(code));
}

}  // namespace priemman::errors
