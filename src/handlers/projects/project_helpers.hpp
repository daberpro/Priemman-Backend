#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include <proto/project.pb.h>
#include <userver/utils/uuid4.hpp>

#include <src/database/project_repository.hpp>

namespace priemman::handlers::projects::helpers {

inline std::string Slugify(const std::string& s) {
    std::string out;
    bool dashed = false;
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            dashed = false;
        } else if (!dashed && !out.empty()) {
            out += '-';
            dashed = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "project" : out;
}

inline std::vector<database::ProjectMediaRow> BuildMediaRows(
    const google::protobuf::RepeatedPtrField<priemman::v1::Media>& media
) {
    std::vector<database::ProjectMediaRow> rows;
    for (const auto& m : media) {
        database::ProjectMediaRow row;
        row.id = m.id().value().empty()
            ? userver::utils::generators::GenerateUuid()
            : m.id().value();
        row.url = m.url();
        row.media_type = m.type() == priemman::v1::MEDIA_TYPE_VIDEO ? "VIDEO" : "IMAGE";
        row.sort_order = m.order();
        rows.push_back(std::move(row));
    }
    return rows;
}

inline std::vector<database::ProjectCollaboratorRow> BuildCollabRows(
    const google::protobuf::RepeatedPtrField<priemman::v1::Collaborator>& collabs
) {
    std::vector<database::ProjectCollaboratorRow> rows;
    for (const auto& c : collabs) {
        if (!c.user_id().value().empty()) {
            rows.push_back({c.user_id().value(), c.role()});
        }
    }
    return rows;
}

inline void SaveChildren(
    const database::ProjectRepository& repo,
    const std::string& project_id,
    const priemman::v1::ProjectInput& input
) {
    repo.ReplaceStrings(project_id, "tools",
        {input.tools().begin(), input.tools().end()});
    repo.ReplaceStrings(project_id, "disciplines",
        {input.disciplines().begin(), input.disciplines().end()});
    repo.ReplaceStrings(project_id, "tags",
        {input.tags().begin(), input.tags().end()});

    const auto media = BuildMediaRows(input.media());
    repo.ReplaceMedia(project_id, media);

    std::optional<std::string> cover;
    if (!input.cover_media_id().empty()) {
        cover = input.cover_media_id();
    } else if (!media.empty()) {
        cover = media.front().id;
    }
    repo.SetCover(project_id, cover);

    repo.ReplaceCollaborators(project_id, BuildCollabRows(input.collaborators()));
}

}  // namespace priemman::handlers::projects::helpers
