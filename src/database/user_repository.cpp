#include "user_repository.hpp"
#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

UserRepository::UserRepository(std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster)
    : _mysql_cluster(*mysql_cluster) {}

std::optional<User> UserRepository::FindById(const std::string& id) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT id, email, first_name, last_name, headline, company, city, country, website_url, avatar_url, is_onboarded
            FROM users WHERE id = ? LIMIT 1
        )sql"},
        id
    ).AsOptionalSingleRow<User>();
}

std::optional<User> UserRepository::FindByEmail(const std::string& email) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT id, email, first_name, last_name, headline, company, city, country, website_url, avatar_url, is_onboarded
            FROM users WHERE email = ? LIMIT 1
        )sql"},
        email
    ).AsOptionalSingleRow<User>();
}

FindOrCreateResult UserRepository::FindOrCreateFromOAuth(const OAuthUserData& oauth) const {
    // 1. Cek apakah oauth account sudah ada
    auto existing_oauth = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT u.id, u.email, u.first_name, u.last_name, u.headline, u.company,
                   u.city, u.country, u.website_url, u.avatar_url, u.is_onboarded
            FROM oauth_accounts oa
            INNER JOIN users u ON u.id = oa.user_id
            WHERE oa.provider = ? AND oa.provider_user_id = ?
            LIMIT 1
        )sql"},
        oauth.provider, oauth.provider_user_id
    ).AsOptionalSingleRow<User>();

    if (existing_oauth.has_value()) {
        return {.user = *existing_oauth, .is_new_user = false};
    }

    // 2. Cek user by email
    auto existing_user = FindByEmail(oauth.email);
    User user;
    bool is_new_user = false;

    if (!existing_user.has_value()) {
        user.id = userver::utils::generators::GenerateUuid();
        user.email = oauth.email;
        const auto separator = oauth.name.find(' ');
        if (separator == std::string::npos) {
            user.first_name = oauth.name;
            user.last_name = "";
        } else {
            user.first_name = oauth.name.substr(0, separator);
            user.last_name = oauth.name.substr(separator + 1);
        }
        user.headline = ""; user.company = ""; user.city = ""; user.country = "";
        user.website_url = ""; user.avatar_url = oauth.avatar_url;
        user.is_onboarded = 0; // std::int8_t

        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO users (
                        id, email, first_name, last_name, headline, company,
                        city, country, website_url, avatar_url, is_onboarded,
                        about_title, about_description
                    )
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                )sql"
            },
            user.id,
            user.email,
            user.first_name,
            user.last_name,
            user.headline,
            user.company,
            user.city,
            user.country,
            user.website_url,
            user.avatar_url,
            user.is_onboarded,
            std::string{""}, // about_title
            std::string{""}  // about_description
        );
        is_new_user = true;
    } else {
        user = *existing_user;
    }

    // 3. Insert oauth_account
    const std::int8_t verified_int = oauth.email_verified ? 1 : 0; // Cast bool ke int8_t
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary, // FIX: kPrimary (sebelumnya kSecondary)
        userver::storages::mysql::Query{R"sql(
            INSERT INTO oauth_accounts (id, user_id, provider, provider_user_id, provider_email, email_verified)
            VALUES (?, ?, ?, ?, ?, ?)
        )sql"},
        userver::utils::generators::GenerateUuid(),
        user.id,
        oauth.provider,
        oauth.provider_user_id,
        oauth.email,
        verified_int
    );

    return {.user = user, .is_new_user = is_new_user};
}

void UserRepository::UpdateBasicInfo(
    const std::string& user_id,
    const BasicInfoPatch& patch
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            UPDATE users
            SET first_name = ?, last_name = ?, headline = ?, company = ?,
                city = ?, country = ?, website_url = ?
            WHERE id = ?
        )sql"},
        patch.first_name, patch.last_name, patch.headline, patch.company,
        patch.city, patch.country, patch.website_url,
        user_id
    );
}

std::optional<AboutInfo> UserRepository::FindAbout(
    const std::string& user_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT about_title, about_description
            FROM users
            WHERE id = ?
            LIMIT 1
        )sql"},
        user_id
    ).AsOptionalSingleRow<AboutInfo>();
}

FindOrCreateResult UserRepository::FindOrCreateFromEmail(const std::string& email) const {
    auto existing = FindByEmail(email);
    if (existing.has_value()) {
        return {.user = *existing, .is_new_user = false};
    }

    User user;
    user.id = userver::utils::generators::GenerateUuid();
    user.email = email;
    user.first_name = "";
    user.last_name = "";
    user.headline = "";
    user.company = "";
    user.city = "";
    user.country = "";
    user.website_url = "";
    user.avatar_url = "";
    user.is_onboarded = 0; // std::int8_t

    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO users (
                    id, email, first_name, last_name, headline, company,
                    city, country, website_url, avatar_url, is_onboarded,
                    about_title, about_description
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )sql"
        },
        user.id, user.email, user.first_name, user.last_name,
        user.headline, user.company, user.city, user.country,
        user.website_url, user.avatar_url, user.is_onboarded,
        std::string{""}, std::string{""}
    );

    return {.user = user, .is_new_user = true};
}

}
