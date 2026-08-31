#pragma once

#include <string>
#include <format>

namespace priemman::auth {

inline std::string BuildSessionCookie(const std::string& token, const std::string& domain) {
    return std::format(R"s(
           session={}
           ; Domain={}
           ; Path=/
           ; HttpOnly
           ; SameSite=Lax
           ; Secure
           ; Max-Age=2592000
    )s",token, domain);
}

// session_cookie.hpp
inline std::string BuildSessionClearCookie(const std::string& domain) {
    return std::format(R"s(
        session=
        ; Domain={}
        ; Path=/
        ; HttpOnly
        ; SameSite=Lax
        ; Secure
        ; Max-Age=0
    )s",domain);
}


}  // namespace priemman::auth
