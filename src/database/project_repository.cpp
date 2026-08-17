#include "project_repository.hpp"

#include <stdexcept>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

namespace {

constexpr std::string_view kSelectProject = R"sql(
    SELECT
        id, owner_id, title, slug, description, cover_media_id,
        visibility, status,
        CAST(views AS SIGNED), CAST(likes AS SIGNED), CAST(saves AS SIGNED),
        DATE_FORMAT(created_at,   '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(updated_at,   '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(published_at, '%Y-%m-%dT%H:%i:%sZ')
    FROM projects
)sql";

// kind -> (table, column); whitelist biar aman dari injeksi.
std::pair<std::string, std::string> KindToTable(const std::string& kind) {
    if (kind == "tools") return {"project_tools", "tool"};
    if (kind == "disciplines") return {"project_disciplines", "discipline"};
    if (kind == "tags") return {"project_tags", "tag"};
    throw std::invalid_argument("Unknown project list kind: " + kind);
}

}  // namespace

ProjectRepository::ProjectRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::optional<ProjectRow> ProjectRepository::FindById(const std::string& id) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectProject} + " WHERE id = ? LIMIT 1"
        },
        id
    ).AsOptionalSingleRow<ProjectRow>();
}

bool ProjectRepository::ExistsByOwnerSlug(
    const std::string& owner_id, const std::string& slug
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(SELECT COUNT(*) FROM projects WHERE owner_id = ? AND slug = ?)sql"
        },
        owner_id, slug
    ).AsOptionalSingleField<std::int64_t>().value_or(0) > 0;
}

std::string ProjectRepository::Create(const ProjectRow& row) const {
    const std::string id = userver::utils::generators::GenerateUuid();
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO projects (
                    id, owner_id, title, slug, description, cover_media_id,
                    visibility, status, published_at
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?,
                        CASE WHEN ? = 'PUBLISHED' THEN NOW(6) ELSE NULL END)
            )sql"
        },
        id, row.owner_id, row.title, row.slug, row.description,
        row.cover_media_id, row.visibility, row.status, row.status
    );
    return id;
}

bool ProjectRepository::Update(const ProjectRow& row) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE projects
                SET title = ?, slug = ?, description = ?,
                    visibility = ?, status = ?,
                    published_at = CASE WHEN ? = 'PUBLISHED'
                        THEN COALESCE(published_at, NOW(6))
                        ELSE published_at END
                WHERE id = ? AND owner_id = ?
            )sql"
        },
        row.title, row.slug, row.description,
        row.visibility, row.status, row.status,
        row.id, row.owner_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

bool ProjectRepository::Delete(
    const std::string& id, const std::string& owner_id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM projects WHERE id = ? AND owner_id = ?"
        },
        id, owner_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

void ProjectRepository::SetCover(
    const std::string& id, const std::optional<std::string>& media_id
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "UPDATE projects SET cover_media_id = ? WHERE id = ?"
        },
        media_id, id
    );
}

void ProjectRepository::IncrementViews(const std::string& id) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "UPDATE projects SET views = views + 1 WHERE id = ?"
        },
        id
    );
}

void ProjectRepository::ReplaceStrings(
    const std::string& project_id, const std::string& kind,
    const std::vector<std::string>& values
) const {
    const auto [table, column] = KindToTable(kind);

    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM " + table + " WHERE project_id = ?"
        },
        project_id
    );

    std::size_t order = 0;
    for (const auto& value : values) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                "INSERT INTO " + table +
                " (project_id, " + column + ", sort_order) VALUES (?, ?, ?)"
            },
            project_id, value, static_cast<std::int64_t>(order++)
        );
    }
}

void ProjectRepository::ReplaceMedia(
    const std::string& project_id, const std::vector<ProjectMediaRow>& media
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM project_media WHERE project_id = ?"
        },
        project_id
    );

    for (const auto& m : media) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO project_media (id, project_id, url, media_type, sort_order)
                    VALUES (?, ?, ?, ?, ?)
                )sql"
            },
            m.id, project_id, m.url, m.media_type, m.sort_order
        );
    }
}

void ProjectRepository::ReplaceCollaborators(
    const std::string& project_id, const std::vector<ProjectCollaboratorRow>& collabs
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM project_collaborators WHERE project_id = ?"
        },
        project_id
    );

    for (const auto& c : collabs) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO project_collaborators (project_id, user_id, role)
                    VALUES (?, ?, ?)
                )sql"
            },
            project_id, c.user_id, c.role
        );
    }
}

std::vector<std::string> ProjectRepository::ListStrings(
    const std::string& project_id, const std::string& kind
) const {
    const auto [table, column] = KindToTable(kind);
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            "SELECT " + column + " FROM " + table +
            " WHERE project_id = ? ORDER BY sort_order ASC"
        },
        project_id
    ).AsVector<std::string>(userver::storages::mysql::kFieldTag);  // <-- TAMBAHKAN INI
}

std::vector<ProjectMediaRow> ProjectRepository::ListMedia(
    const std::string& project_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT id, url, media_type, CAST(sort_order AS SIGNED)
                FROM project_media
                WHERE project_id = ?
                ORDER BY sort_order ASC
            )sql"
        },
        project_id
    ).AsVector<ProjectMediaRow>();
}

std::vector<ProjectCollaboratorRow> ProjectRepository::ListCollaborators(
    const std::string& project_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT user_id, role
                FROM project_collaborators
                WHERE project_id = ?
                ORDER BY created_at ASC
            )sql"
        },
        project_id
    ).AsVector<ProjectCollaboratorRow>();
}

std::vector<ProjectRow> ProjectRepository::ListByOwner(
    const std::string& owner_id, const std::string& status_filter,
    std::int64_t limit, std::int64_t offset
) const {
    if (status_filter.empty()) {
        return _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kSecondary,
            userver::storages::mysql::Query{
                std::string{kSelectProject} +
                " WHERE owner_id = ? ORDER BY created_at DESC LIMIT ? OFFSET ?"
            },
            owner_id, limit, offset
        ).AsVector<ProjectRow>();
    }
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectProject} +
            " WHERE owner_id = ? AND status = ?"
            " ORDER BY created_at DESC LIMIT ? OFFSET ?"
        },
        owner_id, status_filter, limit, offset
    ).AsVector<ProjectRow>();
}

std::vector<ProjectRow> ProjectRepository::ListPublic(
    std::int64_t limit, std::int64_t offset
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectProject} +
            " WHERE status = 'PUBLISHED' AND visibility = 'PUBLIC'"
            " ORDER BY published_at DESC LIMIT ? OFFSET ?"
        },
        limit, offset
    ).AsVector<ProjectRow>();
}

}  // namespace priemman::database
