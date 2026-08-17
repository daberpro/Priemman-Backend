#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct User {
    std::string id;
    std::string email;
    std::string first_name;
    std::string last_name;
    std::string headline;
    std::string company;
    std::string city;
    std::string country;
    std::string website_url;
    std::string avatar_url;
    std::int8_t is_onboarded; // Diubah ke std::int8_t
};

struct OAuthUserData {
    std::string provider;
    std::string provider_user_id;
    std::string email;
    std::string name;
    std::string avatar_url;
    bool email_verified; // Boleh tetap bool karena ini DTO dari JSON
};

struct FindOrCreateResult {
    User user;
    bool is_new_user;
};

struct BasicInfoPatch {
    std::string first_name;
    std::string last_name;
    std::string headline;
    std::string company;
    std::string city;
    std::string country;
    std::string website_url;
};

struct AboutInfo {
    std::string title;
    std::string description;
};


class UserRepository {
public:
    explicit UserRepository(std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster);
    std::optional<User> FindById(const std::string& id) const;
    std::optional<User> FindByEmail(const std::string& email) const;
    FindOrCreateResult FindOrCreateFromOAuth(const OAuthUserData& oauth) const;
    FindOrCreateResult FindOrCreateFromEmail(const std::string& email) const;
    void UpdateBasicInfo(const std::string& user_id, const BasicInfoPatch& patch) const;
    std::optional<AboutInfo> FindAbout(const std::string& user_id) const;
private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}
