#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct ProjectRow {
    std::string id;
    std::string owner_id;
    std::string title;
    std::string slug;
    std::string description;
    std::optional<std::string> cover_media_id;
    std::string visibility;   // 'PUBLIC' | 'UNLISTED' | 'DRAFT'
    std::string status;       // 'DRAFT' | 'PUBLISHED' | 'ARCHIVED'
    // views/likes/saves dibaca lewat CAST(... AS SIGNED) di query SELECT,
    // jadi bind di sini harus signed.
    std::int64_t views;
    std::int64_t likes;
    std::int64_t saves;
    std::optional<std::string> created_at;
    std::optional<std::string> updated_at;
    std::optional<std::string> published_at;
};

struct ProjectMediaRow {
    std::string id;
    std::string url;
    std::string media_type;   // 'IMAGE' | 'VIDEO'
    std::int64_t sort_order; // dibaca lewat CAST(sort_order AS SIGNED)
    std::string cloudinary_public_id; // <-- tambahan
};

struct ProjectCollaboratorRow {
    std::string user_id;
    std::string role;
};

class ProjectRepository {
public:
    explicit ProjectRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    std::optional<ProjectRow> FindById(const std::string& id) const;
    bool ExistsByOwnerSlug(const std::string& owner_id, const std::string& slug) const;
    std::string Create(const ProjectRow& row) const;
    bool Update(const ProjectRow& row) const;
    bool Delete(const std::string& id, const std::string& owner_id) const;
    void SetCover(const std::string& id, const std::optional<std::string>& media_id) const;
    void IncrementViews(const std::string& id) const;

    void ReplaceStrings(const std::string& project_id, const std::string& kind,
                        const std::vector<std::string>& values) const;
    void ReplaceMedia(const std::string& project_id,
                      const std::vector<ProjectMediaRow>& media) const;
    void ReplaceCollaborators(const std::string& project_id,
                              const std::vector<ProjectCollaboratorRow>& collabs) const;

    std::vector<std::string> ListStrings(const std::string& project_id,
                                         const std::string& kind) const;
    std::vector<ProjectMediaRow> ListMedia(const std::string& project_id) const;

    // Apakah public_id ini masih direferensikan oleh project tertentu?
    // Dipakai saat update project: media in_use boleh dikirim ulang selama
    // memang sudah terpasang di project ini.
    bool IsMediaReferenced(const std::string& project_id,
                           const std::string& cloudinary_public_id) const;
    std::vector<ProjectCollaboratorRow> ListCollaborators(const std::string& project_id) const;

    std::vector<ProjectRow> ListByOwner(const std::string& owner_id,
                                        const std::string& status_filter,
                                        std::int64_t limit, std::int64_t offset) const;
    std::vector<ProjectRow> ListPublic(std::int64_t limit, std::int64_t offset) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
