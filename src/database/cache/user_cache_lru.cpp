#include "user_cache_lru.hpp"

namespace priemman::cache::user {

    UserData UserCache::DoGetByKey([[maybe_unused]] const UserId& user_id){
        auto user = _users.FindById(user_id);
        if (!user.has_value()){
            throw std::runtime_error("User not found");
        };

        const auto about = _users.FindAbout(user_id);
        const auto wx = _accounts.ListWorkExperiences(user_id);

        return handlers::mapper::ToUserProto(*user, about, wx);
    }

}