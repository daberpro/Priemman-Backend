#pragma once
#include <string>
#include <format>

namespace priemman::auth {

inline std::string BuildSessionCookie(const std::string& token, const std::string& domain) {
    return std::format(
        "session={}; Domain={}; Path=/; HttpOnly; SameSite=Lax; Secure; Max-Age=2592000",
        token, domain);
}

inline std::string BuildSessionClearCookie(const std::string& domain) {
    return std::format(
        "session=; Domain={}; Path=/; HttpOnly; SameSite=Lax; Secure; Max-Age=0",
        domain);
}

}  // namespace priemman::auth
