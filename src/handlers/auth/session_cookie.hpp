#pragma once

#include <string>

namespace priemman::auth {

inline std::string BuildSessionCookie(const std::string& token) {
    return "session=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=2592000";
}

}  // namespace priemman::auth
