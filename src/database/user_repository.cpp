#include "user_repository.hpp"
#include "project_repository.hpp"
#include <stdexcept>
#include <userver/storages/mysql/cluster.hpp>
#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>
#include <userver/utils/uuid7.hpp>
#include <vector>

namespace priemman::database {

namespace {

void ValidateRole(const std::string& role) {
    if (role != "user" && role != "creator" && role != "admin") {
        throw std::invalid_argument("Invalid role: " + role);
    }
}

}  // namespace

UserRepository::UserRepository(std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster)
    : _mysql_cluster(*mysql_cluster) {}

std::optional<User> UserRepository::FindById(const std::string& id) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT id, email, first_name, last_name, headline, company, city, country, website_url, avatar_url, is_onboarded, role
            FROM users WHERE id = ? LIMIT 1
        )sql"},
        id
    ).AsOptionalSingleRow<User>();
}

std::optional<User> UserRepository::FindByEmail(const std::string& email) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{R"sql(
            SELECT id, email, first_name, last_name, headline, company, city, country, website_url, avatar_url, is_onboarded, role
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
                   u.city, u.country, u.website_url, u.avatar_url, u.is_onboarded, u.role
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
        user.role = "user";    // default DB, INSERT tidak menyertakan role

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
                city = ?, country = ?, website_url = ?, about_title = ?, about_description = ?,
                avatar_url = ?
            WHERE id = ?
        )sql"},
        patch.first_name, patch.last_name, patch.headline, patch.company,
        patch.city, patch.country, patch.website_url,
        patch.about_me.title, patch.about_me.description, patch.avatar_url,
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
    user.role = "user";    // default DB, INSERT tidak menyertakan role

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

std::vector<AdminUserRow> UserRepository::ListUsers(
    std::int64_t limit,
    std::int64_t offset,
    const std::string& role_filter
) const {
    std::string query = R"sql(
        SELECT id, email, first_name, last_name, role,
               DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ')
        FROM users
    )sql";
    if (!role_filter.empty()) {
        ValidateRole(role_filter);
        query += " WHERE role = '" + role_filter + "'";
    }
    query += " ORDER BY created_at DESC LIMIT ? OFFSET ?";

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{query},
        limit, offset
    ).AsVector<AdminUserRow>();
}

std::int64_t UserRepository::CountUsers(const std::string& role_filter) const {
    std::string query = "SELECT COUNT(*) FROM users";
    if (!role_filter.empty()) {
        ValidateRole(role_filter);
        query += " WHERE role = '" + role_filter + "'";
    }

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{query}
    ).AsOptionalSingleField<std::int64_t>().value_or(0);
}

bool UserRepository::SetRole(const std::string& user_id, const std::string& role) const {
    ValidateRole(role);

    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{R"sql(
            UPDATE users SET role = ? WHERE id = ?
        )sql"},
        role, user_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

bool UserRepository::ActionLike(const std::string& user_id, const std::string& project_id) const {
    auto trx = _mysql_cluster->Begin(userver::storages::mysql::ClusterHostType::kPrimary);
    const auto id = userver::utils::generators::GenerateUuid();
    const auto result = trx.Execute(
      userver::storages::mysql::Query{
          R"sql(
            INSERT IGNORE INTO project_likes (id,user_id,project_id)
            VALUES (?,?,?)
          )sql"
      },
      id,
      user_id,
      project_id
    ).AsExecutionResult();

    if(result.rows_affected > 0){
        trx.Execute(
            userver::storages::mysql::Query{
                R"sql(
                    UPDATE projects
                    SET likes = likes + 1
                    WHERE id = ?
                )sql"
            },
            project_id
        );
        trx.Commit();
        return true;
    }
    return false;
}

bool UserRepository::ActionUnLike(const std::string& user_id, const std::string& project_id) const {
    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
          R"sql(
            DELETE FROM project_likes
            WHERE user_id = ? AND project_id = ?
          )sql"
      },
      user_id,
      project_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

bool UserRepository::ActionView(const std::string& user_id, const std::string& project_id) const {
    auto trx = _mysql_cluster->Begin(userver::storages::mysql::ClusterHostType::kPrimary);
    const auto id = userver::utils::generators::GenerateUuid();
    const auto result = trx.Execute(
      userver::storages::mysql::Query{
          R"sql(
            INSERT IGNORE INTO project_views (id,user_id,project_id)
            VALUES (?,?,?)
          )sql"
      },
      id,
      user_id,
      project_id
    ).AsExecutionResult();

    if(result.rows_affected > 0){
        trx.Execute(
            userver::storages::mysql::Query{
                R"sql(
                    UPDATE projects
                    SET views = views + 1
                    WHERE id = ?
                )sql"
            },
            project_id
        );
        trx.Commit();
        return true;
    }
    return false;
}

bool UserRepository::ActionSave(const std::string& user_id, const std::string& project_id) const {
    auto trx = _mysql_cluster->Begin(userver::storages::mysql::ClusterHostType::kPrimary);
    const auto id = userver::utils::generators::GenerateUuid();
    const auto result = trx.Execute(
      userver::storages::mysql::Query{
          R"sql(
            INSERT IGNORE INTO project_saved (id,user_id,project_id)
            VALUES (?,?,?)
          )sql"
      },
      id,
      user_id,
      project_id
    ).AsExecutionResult();

    if(result.rows_affected > 0){
        trx.Execute(
            userver::storages::mysql::Query{
                R"sql(
                    UPDATE projects
                    SET saves = saves + 1
                    WHERE id = ?
                )sql"
            },
            project_id
        );
        trx.Commit();
        return true;
    }
    return false;
}

bool UserRepository::ActionUnSave(const std::string& user_id, const std::string& project_id) const {
    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
          R"sql(
            DELETE FROM project_saved
            WHERE user_id = ? AND project_id = ?
          )sql"
      },
      user_id,
      project_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

std::vector<ProjectSummaryRow> UserRepository::ListLikedProjects(
    const std::string& user_id,
    std::int64_t limit,
    std::int64_t offset
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    p.id AS project_id,
                    p.title AS title,
                    p.cover_media_id AS thumbnail,
                    u.first_name AS first_name,
                    u.last_name AS last_name,
                FROM project_likes pl
                INNER JOIN projects p
                    ON pl.project_id = p.id
                INNER JOIN users u
                    ON p.owner_id = u.id
                WHERE pl.user_id = ?
                    AND p.status = 'PUBLISHED'
                    AND p.visibility = 'PUBLIC'
                ORDER BY pl.created_at DESC
                LIMIT ? OFFSET ?;
            )sql"
        },
        user_id,
        limit,
        offset
    ).AsVector<ProjectSummaryRow>();
}

std::vector<ProjectSummaryRow> UserRepository::ListSavedProjects(
    const std::string& user_id,
    std::int64_t limit,
    std::int64_t offset
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    p.id AS project_id,
                    p.title,
                    p.cover_media_id AS thumbnail,
                    u.first_name,
                    u.last_name
                FROM project_saved ps
                INNER JOIN projects p
                    ON ps.project_id = p.id
                INNER JOIN users u
                    ON p.owner_id = u.id
                WHERE ps.user_id = ?
                    AND p.status = 'PUBLISHED'
                    AND p.visibility = 'PUBLIC'
                ORDER BY ps.created_at DESC
                LIMIT ? OFFSET ?;
            )sql"
        },
        user_id,
        limit,
        offset
    ).AsVector<ProjectSummaryRow>();
}

}
