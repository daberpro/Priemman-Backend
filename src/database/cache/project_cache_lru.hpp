#pragma once

#include <userver/cache/lru_cache_component_base.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/component_config.hpp>
#include <userver/storages/mysql/component.hpp>
#include <proto/project.pb.h>
#include <src/handlers/projects/project_proto_convert.hpp>
#include <src/database/project_repository.hpp>

namespace priemman::cache::project {
    
    using ProjectId = std::string;
    using ProjectData = priemman::v1::ProjectResponse;

    class ProjectCache final : public userver::cache::LruCacheComponent<ProjectId,ProjectData> {
    public:

        static constexpr std::string_view kName{"project-cache"};
        
        ProjectCache(
            [[maybe_unused]] const userver::components::ComponentConfig& config,
            [[maybe_unused]] const userver::components::ComponentContext& context
        ) : userver::cache::LruCacheComponent<ProjectId, ProjectData>(config,context), 
        _mysql_cluster{
            context.FindComponent<userver::storages::mysql::Component>("database").GetCluster()
        },
        _projects(&_mysql_cluster){}

    private:

        ProjectData DoGetByKey([[maybe_unused]] const ProjectId& project_id) override;
        std::shared_ptr<userver::storages::mysql::Cluster> _mysql_cluster;
        database::ProjectRepository _projects;

    };

}