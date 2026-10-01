#include "project_cache_lru.hpp"

namespace priemman::cache::project {

    ProjectData ProjectCache::DoGetByKey([[maybe_unused]] const ProjectId& project_id){
        auto row = _projects.FindById(project_id);
        if (!row.has_value()) {
            throw std::runtime_error("NOT_FOUND");
        }

        priemman::v1::ProjectResponse response;
        *response.mutable_project() = handlers::mapper::ToProto(
            *row,
            _projects.ListStrings(project_id, "tags"),
            _projects.ListMedia(project_id),
            _projects.ListCollaborators(project_id)
        );
        return response;
    }

}