#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

// TTL untuk media orphan: media yang diupload tapi tidak pernah
// terikat ke project akan dihapus dari Cloudinary setelah lewat waktu ini.
inline constexpr std::int64_t kMediaOrphanTtlSeconds = 24 * 60 * 60;  // 24 jam

struct MediaUploadRow {
    std::string public_id;
    std::string user_id;
    std::string resource_type;  // 'image' | 'video'
    std::string status;         // 'orphan' | 'in_use'
    std::optional<std::string> created_at;
    std::optional<std::string> attached_at;
};

struct ExpiredOrphan {
    std::string public_id;
    std::string resource_type;
};

class MediaRepository {
public:
    explicit MediaRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    // Catat aset baru sebagai orphan (dipanggil setelah upload ke Cloudinary).
    void InsertOrphan(const std::string& public_id,
                      const std::string& user_id,
                      const std::string& resource_type) const;

    std::optional<MediaUploadRow> FindByPublicId(const std::string& public_id) const;

    // Flip orphan -> in_use hanya jika masih orphan dan milik user ini.
    // Return true kalau flip terjadi, false kalau tidak (sudah in_use atau
    // bukan miliknya).
    bool MarkInUse(const std::string& public_id,
                   const std::string& user_id) const;

    // Kembalikan ke orphan hanya jika sudah TIDAK ADA lagi baris project_media
    // yang mereferensikan public_id ini (aman untuk media yang dipakai
    // lebih dari satu project).
    bool DetachIfUnreferenced(const std::string& public_id) const;

    // Mengembalikan media ke orphan, (berarti media tidak dipakai)
    bool MakeOrphan(const std::string& public_id, const std::string& user_id) const;

    // Ambil orphan yang sudah melewati TTL (untuk sweeper).
    std::vector<ExpiredOrphan> ListExpiredOrphans(std::int64_t ttl_seconds,
                                                  std::int64_t limit) const;

    // Hapus baris tracking (dipanggil setelah aset terhapus dari Cloudinary).
    void DeleteByPublicId(const std::string& public_id) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
