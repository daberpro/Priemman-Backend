#pragma once

#include <string>

namespace priemman::auth {

inline std::string BuildSessionCookie(const std::string& token) {
    return "session=" + token +
           "; Domain=priemman.my.id"   // <- agar berlaku untuk *.priemman.my.id
           "; Path=/"
           "; HttpOnly"
           "; SameSite=Lax"
           "; Secure"                  // <- wajib karena HTTPS
           "; Max-Age=2592000";
}

}  // namespace priemman::auth
