#include "collection_repository.hpp"

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>
#include <userver/utils/uuid4.hpp>

namespace priemman::database {

namespace {
constexpr std::string_view kSelectCollection = R"sql(
    SELECT
        id, owner_id, title, description, visibility,
        DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(updated_at, '%Y-%m-%dT%H:%i:%sZ')
    FROM collections
)sql";
}  // namespace

CollectionRepository::CollectionRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

std::optional<CollectionRow> CollectionRepository::FindById(
    const std::string& id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectCollection} + " WHERE id = ? LIMIT 1"
        },
        id
    ).AsOptionalSingleRow<CollectionRow>();
}

std::string CollectionRepository::Create(const CollectionRow& row) const {
    const std::string id = userver::utils::generators::GenerateUuid();
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO collections (id, owner_id, title, description, visibility)
                VALUES (?, ?, ?, ?, ?)
            )sql"
        },
        id, row.owner_id, row.title, row.description, row.visibility
    );
    return id;
}

bool CollectionRepository::Update(const CollectionRow& row) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE collections
                SET title = ?, description = ?, visibility = ?
                WHERE id = ? AND owner_id = ?
            )sql"
        },
        row.title, row.description, row.visibility,
        row.id, row.owner_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

bool CollectionRepository::Delete(
    const std::string& id, const std::string& owner_id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM collections WHERE id = ? AND owner_id = ?"
        },
        id, owner_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

std::vector<CollectionRow> CollectionRepository::ListByOwner(
    const std::string& owner_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectCollection} +
            " WHERE owner_id = ? ORDER BY created_at DESC"
        },
        owner_id
    ).AsVector<CollectionRow>();
}

void CollectionRepository::ReplaceProjects(
    const std::string& collection_id, const std::vector<std::string>& project_ids
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM collection_projects WHERE collection_id = ?"
        },
        collection_id
    );

    std::size_t order = 0;
    for (const auto& project_id : project_ids) {
        _mysql_cluster->Execute(
            userver::storages::mysql::ClusterHostType::kPrimary,
            userver::storages::mysql::Query{
                R"sql(
                    INSERT INTO collection_projects (collection_id, project_id, sort_order)
                    VALUES (?, ?, ?)
                )sql"
            },
            collection_id, project_id, static_cast<std::int64_t>(order++)
        );
    }
}

std::vector<std::string> CollectionRepository::ListProjectIds(
    const std::string& collection_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT project_id FROM collection_projects
                WHERE collection_id = ?
                ORDER BY sort_order ASC
            )sql"
        },
        collection_id
    ).AsVector<std::string>(userver::storages::mysql::kFieldTag);  // <-- TAMBAHKAN INI
}

}  // namespace priemman::database
