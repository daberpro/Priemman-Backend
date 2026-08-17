#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <userver/storages/mysql/cluster.hpp>

namespace priemman::database {

struct CollectionRow {
    std::string id;
    std::string owner_id;
    std::string title;
    std::string description;
    std::string visibility;   // 'PUBLIC' | 'PRIVATE'
    std::optional<std::string> created_at;
    std::optional<std::string> updated_at;
};

class CollectionRepository {
public:
    explicit CollectionRepository(
        std::shared_ptr<userver::storages::mysql::Cluster>* mysql_cluster
    );

    std::optional<CollectionRow> FindById(const std::string& id) const;
    std::string Create(const CollectionRow& row) const;
    bool Update(const CollectionRow& row) const;
    bool Delete(const std::string& id, const std::string& owner_id) const;
    std::vector<CollectionRow> ListByOwner(const std::string& owner_id) const;

    void ReplaceProjects(const std::string& collection_id,
                         const std::vector<std::string>& project_ids) const;
    std::vector<std::string> ListProjectIds(const std::string& collection_id) const;

private:
    std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster{nullptr};
};

}  // namespace priemman::database
