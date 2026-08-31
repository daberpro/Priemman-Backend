#include "media_repository.hpp"

#include <userver/storages/mysql/cluster_host_type.hpp>
#include <userver/storages/mysql/query.hpp>

namespace priemman::database {

namespace {

constexpr std::string_view kSelectMediaUpload = R"sql(
    SELECT
        public_id, user_id, resource_type, status,
        DATE_FORMAT(created_at,  '%Y-%m-%dT%H:%i:%sZ'),
        DATE_FORMAT(attached_at, '%Y-%m-%dT%H:%i:%sZ')
    FROM media_uploads
)sql";

}  // namespace

MediaRepository::MediaRepository(
    std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
)
    : _mysql_cluster(*mysql_cluster) {
}

void MediaRepository::InsertOrphan(
    const std::string& public_id,
    const std::string& user_id,
    const std::string& resource_type
) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                INSERT INTO media_uploads (public_id, user_id, resource_type, status)
                VALUES (?, ?, ?, 'orphan')
            )sql"
        },
        public_id, user_id, resource_type
    );
}

std::optional<MediaUploadRow> MediaRepository::FindByPublicId(
    const std::string& public_id
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kSecondary,
        userver::storages::mysql::Query{
            std::string{kSelectMediaUpload} + " WHERE public_id = ? LIMIT 1"
        },
        public_id
    ).AsOptionalSingleRow<MediaUploadRow>();
}

bool MediaRepository::MarkInUse(
    const std::string& public_id, const std::string& user_id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE media_uploads
                SET status = 'in_use', attached_at = NOW(6)
                WHERE public_id = ? AND user_id = ? AND status = 'orphan'
            )sql"
        },
        public_id, user_id
    ).AsExecutionResult();

    return result.rows_affected > 0;
}

bool MediaRepository::DetachIfUnreferenced(const std::string& public_id) const {
    // Hanya dikembalikan ke orphan kalau tidak ada lagi project yang
    // mereferensikan aset ini (satu aset bisa dipakai beberapa project).
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE media_uploads mu
                SET mu.status = 'orphan', mu.attached_at = NULL
                WHERE mu.public_id = ?
                  AND mu.status = 'in_use'
                  AND NOT EXISTS (
                      SELECT 1 FROM project_media pm
                      WHERE pm.cloudinary_public_id = mu.public_id
                  )
            )sql"
        },
        public_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

std::vector<ExpiredOrphan> MediaRepository::ListExpiredOrphans(
    std::int64_t ttl_seconds, std::int64_t limit
) const {
    return _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                SELECT public_id, resource_type
                FROM media_uploads
                WHERE status = 'orphan'
                  AND created_at < NOW(6) - INTERVAL ? SECOND
                LIMIT ?
            )sql"
        },
        ttl_seconds, limit
    ).AsVector<ExpiredOrphan>();
}

void MediaRepository::DeleteByPublicId(const std::string& public_id) const {
    _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            "DELETE FROM media_uploads WHERE public_id = ?"
        },
        public_id
    );
}

bool MediaRepository::MakeOrphan(
    const std::string& public_id, const std::string& user_id
) const {
    auto result = _mysql_cluster->Execute(
        userver::storages::mysql::ClusterHostType::kPrimary,
        userver::storages::mysql::Query{
            R"sql(
                UPDATE media_uploads mu
                SET mu.status = 'orphan', mu.attached_at = NULL
                WHERE mu.public_id = ?
                AND mu.status = 'in_use'
                AND mu.user_id = ?
            )sql"
        },
        public_id,
        user_id
    ).AsExecutionResult();
    return result.rows_affected > 0;
}

}  // namespace priemman::database
