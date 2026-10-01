#include "project_repository.hpp"

#include "src/database/common_definition.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/storages/mysql/transaction.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

namespace {

constexpr std::string_view kSelectProject = R"sql(
    SELECT
        p.id,
        p.owner_id,
        p.title,
        p.slug,
        p.content,
        p.cover_media_id,
        p.visibility,
        p.status,
        CAST(pm.views AS SIGNED),
        CAST(pm.likes AS SIGNED),
        CAST(pm.saves AS SIGNED),
        DATE_FORMAT(p.created_at, '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(p.updated_at, '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(p.published_at, '%Y-%m-%dT%H:%i:%sZ')
    FROM projects p
    INNER JOIN projects_meta_data pm ON pm.project_id = p.id
)sql";

struct ProjectPopulatedRow {
    std::string id;
    std::string owner_id;
    std::string title;
    std::string slug;
    std::optional<std::string> content;
    std::optional<std::string> cover_media_id;
    std::string visibility;
    std::string status;
    std::int64_t views;
    std::int64_t likes;
    std::int64_t saves;
    std::optional<std::string> created_at;
    std::optional<std::string> updated_at;
    std::optional<std::string> published_at;
    std::string author_id;
    std::string author_first_name;
    std::string author_last_name;
    std::optional<std::string> author_avatar_url;
    std::optional<std::string> author_headline;
};

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

std::optional<ProjectRowPopulated> ProjectRepository::FindById(const std::string& id) const {
    const auto project = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectProject} + " WHERE p.id = ? LIMIT 1"
        },
        id
    ).AsOptionalSingleRow<ProjectRowWithMetaInfo>();

    if (!project.has_value()) {
        return std::nullopt;
    }

    const auto author = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT id, first_name, last_name, avatar_url, headline
                FROM users
                WHERE id = ?
            )sql"
        },
        project->owner_id
    ).AsOptionalSingleRow<priemman::common::PublicInfo>();

    ProjectRowPopulated result;
    result.project = *project;

    if (author.has_value()) {
        result.author = *author;
    }

    return result;
}

bool ProjectRepository::ExistsByOwnerSlug(
    const std::string& owner_id,
    const std::string& slug
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            "SELECT COUNT(*) FROM projects WHERE owner_id = ? AND slug = ?"
        },
        owner_id,
        slug
    ).AsOptionalSingleField<std::int64_t>().value_or(0) > 0;
}

ProjectRowPopulated ProjectRepository::Create(const ProjectRow& row) const {
    const std::string id = userver::utils::generators::GenerateUuid();
    const std::string meta_id = userver::utils::generators::GenerateUuid();

    auto trx = _mysql_cluster->Begin(
        userver::storages::mysql::ClusterHostType::kPrimary
    );

    trx.Execute(
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO projects (
                    id, owner_id, title, slug, cover_media_id,
                    visibility, content, status, published_at
                )
                VALUES (
                    ?, ?, ?, ?, ?, ?, ?, ?,
                    CASE WHEN ? = 'PUBLISHED' THEN NOW(6) ELSE NULL END
                )
            )sql"
        },
        id,
        row.owner_id,
        row.title,
        row.slug,
        row.cover_media_id,
        row.visibility,
        row.content,
        row.status,
        row.status
    );

    trx.Execute(
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO projects_meta_data (
                    id, project_id, views, likes, saves
                )
                VALUES (?, ?, 0, 0, 0)
            )sql"
        },
        meta_id,
        id
    );

    const auto project = trx.Execute(
        userver::storages::mysql::Query{
            std::string{kSelectProject} + " WHERE p.id = ? LIMIT 1"
        },
        id
    ).AsSingleRow<ProjectRowWithMetaInfo>();

    const auto author = trx.Execute(
        userver::storages::mysql::Query{
            R"sql(
                SELECT id, first_name, last_name, avatar_url, headline
                FROM users
                WHERE id = ?
            )sql"
        },
        project.owner_id
    ).AsOptionalSingleRow<priemman::common::PublicInfo>();

    trx.Commit();

    ProjectRowPopulated result;
    result.project = project;

    if (author.has_value()) {
        result.author = *author;
    }

    return result;
}

std::optional<ProjectRowPopulated> ProjectRepository::Update(const ProjectRow& row) const {
    auto trx = _mysql_cluster->Begin(
        userver::storages::mysql::ClusterHostType::kPrimary
    );

    const auto exec_result = trx.Execute(
        userver::storages::mysql::Query{
            R"sql(
                UPDATE projects
                SET
                    title = ?,
                    slug = ?,
                    visibility = ?,
                    status = ?,
                    content = ?,
                    published_at = CASE
                        WHEN ? = 'PUBLISHED'
                        THEN COALESCE(published_at, NOW(6))
                        ELSE published_at
                    END
                WHERE id = ? AND owner_id = ?
            )sql"
        },
        row.title,
        row.slug,
        row.visibility,
        row.status,
        row.content,
        row.status,
        row.id,
        row.owner_id
    ).AsExecutionResult();

    if (exec_result.rows_affected == 0) {
        trx.Rollback();
        return std::nullopt;
    }

    const auto project = trx.Execute(
        userver::storages::mysql::Query{
            std::string{kSelectProject} + " WHERE p.id = ? LIMIT 1"
        },
        row.id
    ).AsOptionalSingleRow<ProjectRowWithMetaInfo>();

    if (!project.has_value()) {
        trx.Rollback();
        return std::nullopt;
    }

    const auto author = trx.Execute(
        userver::storages::mysql::Query{
            R"sql(
                SELECT id, first_name, last_name, avatar_url, headline
                FROM users
                WHERE id = ?
            )sql"
        },
        project->owner_id
    ).AsOptionalSingleRow<priemman::common::PublicInfo>();

    trx.Commit();

    ProjectRowPopulated result;
    result.project = *project;

    if (author.has_value()) {
        result.author = *author;
    }

    return result;
}

bool ProjectRepository::Delete(
    const std::string& id,
    const std::string& owner_id
) const {
    const auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM projects WHERE id = ? AND owner_id = ?"
        },
        id,
        owner_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

void ProjectRepository::SetCover(
    const std::string& id,
    const std::optional<std::string>& media_id
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "UPDATE projects SET cover_media_id = ? WHERE id = ?"
        },
        media_id,
        id
    );
}

void ProjectRepository::IncrementViews(const std::string& id) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "UPDATE projects_meta_data SET views = views + 1 WHERE project_id = ?"
        },
        id
    );
}

void ProjectRepository::ReplaceStrings(
    const std::string& project_id,
    const std::string& kind,
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
            project_id,
            value,
            static_cast<std::int64_t>(order++)
        );
    }
}

void ProjectRepository::ReplaceMedia(
    const std::string& project_id,
    const std::vector<ProjectMediaRow>& media
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM project_media WHERE project_id = ?"
        },
        project_id
    );

    for (const auto& media_row : media) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO project_media (
                        id, project_id, url, media_type,
                        sort_order, cloudinary_public_id
                    )
                    VALUES (?, ?, ?, ?, ?, ?)
                )sql"
            },
            media_row.id,
            project_id,
            media_row.url,
            media_row.media_type,
            media_row.sort_order,
            media_row.cloudinary_public_id
        );
    }
}

void ProjectRepository::ReplaceCollaborators(
    const std::string& project_id,
    const std::vector<ProjectCollaboratorRow>& collabs
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM project_collaborators WHERE project_id = ?"
        },
        project_id
    );

    for (const auto& collaborator : collabs) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO project_collaborators (
                        project_id, user_id, role
                    )
                    VALUES (?, ?, ?)
                )sql"
            },
            project_id,
            collaborator.user_id,
            collaborator.role
        );
    }
}

std::vector<std::string> ProjectRepository::ListStrings(
    const std::string& project_id,
    const std::string& kind
) const {
    const auto [table, column] = KindToTable(kind);

    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            "SELECT " + column + " FROM " + table +
            " WHERE project_id = ? ORDER BY sort_order ASC"
        },
        project_id
    ).AsVector<std::string>(userver::storages::mysql::kFieldTag);
}

std::vector<ProjectMediaRow> ProjectRepository::ListMedia(
    const std::string& project_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT id, url, media_type,
                       CAST(sort_order AS SIGNED),
                       cloudinary_public_id
                FROM project_media
                WHERE project_id = ?
                ORDER BY sort_order ASC
            )sql"
        },
        project_id
    ).AsVector<ProjectMediaRow>();
}

bool ProjectRepository::IsMediaReferenced(
    const std::string& project_id,
    const std::string& cloudinary_public_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT COUNT(*)
                FROM project_media
                WHERE project_id = ?
                  AND cloudinary_public_id = ?
            )sql"
        },
        project_id,
        cloudinary_public_id
    ).AsOptionalSingleField<std::int64_t>().value_or(0) > 0;
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

std::vector<ProjectRowWithMetaInfo> ProjectRepository::ListByOwner(
    const std::string& owner_id,
    const std::string& status_filter,
    std::int64_t limit,
    std::int64_t offset
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectProject} +
            R"sql(
                WHERE p.owner_id = ?
                  AND p.status = ?
                ORDER BY p.created_at DESC
                LIMIT ? OFFSET ?
            )sql"
        },
        owner_id,
        status_filter,
        limit,
        offset
    ).AsVector<ProjectRowWithMetaInfo>();
}

std::vector<ProjectRowPopulated> ProjectRepository::ListPublic(
    std::int64_t limit,
    std::int64_t offset
) const {
    const auto rows = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT
                    p.id,
                    p.owner_id,
                    p.title,
                    p.slug,
                    p.content,
                    p.cover_media_id,
                    p.visibility,
                    p.status,
                    CAST(pm.views AS SIGNED),
                    CAST(pm.likes AS SIGNED),
                    CAST(pm.saves AS SIGNED),
                    DATE_FORMAT(p.created_at, '%Y-%m-%dT%H:%i:%sZ'),
                    DATE_FORMAT(p.updated_at, '%Y-%m-%dT%H:%i:%sZ'),
                    DATE_FORMAT(p.published_at, '%Y-%m-%dT%H:%i:%sZ'),
                    u.id,
                    u.first_name,
                    u.last_name,
                    u.avatar_url,
                    u.headline
                FROM projects p
                INNER JOIN projects_meta_data pm ON pm.project_id = p.id
                INNER JOIN users u ON u.id = p.owner_id
                WHERE p.status = 'PUBLISHED'
                  AND p.visibility = 'PUBLIC'
                ORDER BY p.published_at DESC
                LIMIT ? OFFSET ?
            )sql"
        },
        limit,
        offset
    ).AsVector<ProjectPopulatedRow>();

    std::vector<ProjectRowPopulated> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
        ProjectRowPopulated populated;

        populated.project.id = row.id;
        populated.project.owner_id = row.owner_id;
        populated.project.title = row.title;
        populated.project.slug = row.slug;
        populated.project.content = row.content;
        populated.project.cover_media_id = row.cover_media_id;
        populated.project.visibility = row.visibility;
        populated.project.status = row.status;
        populated.project.views = row.views;
        populated.project.likes = row.likes;
        populated.project.saves = row.saves;
        populated.project.created_at = row.created_at;
        populated.project.updated_at = row.updated_at;
        populated.project.published_at = row.published_at;

        populated.author.id = row.author_id;
        populated.author.first_name = row.author_first_name;
        populated.author.last_name = row.author_last_name;
        populated.author.avatar_url = *row.author_avatar_url;
        populated.author.headline = *row.author_headline;

        result.push_back(std::move(populated));
    }

    return result;
}

}  // namespace priemman::database