#pragma once

#include <cctype>
#include <expected>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <proto/project.pb.h>
#include <userver/utils/uuid4.hpp>

#include <src/database/media_repository.hpp>
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
        row.cloudinary_public_id = m.public_id(); // <-- ambil
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

// Validasi dilakukan sebelum ReplaceMedia/ReplaceCollaborators agar input
// malformed tidak menghapus child records project yang sudah ada.
inline std::expected<bool, std::string> ValidateProjectRelations(
    const priemman::v1::ProjectInput& input
) {
    std::unordered_set<std::string> media_ids;
    std::unordered_set<std::string> collaborator_ids;
    bool cover_found = input.cover_media_id().empty();

    for (const auto& media : input.media()) {
        if (media.public_id().empty() || media.url().empty()) {
            return std::unexpected{"Every media item must have a URL and public ID"};
        }
        if (media.type() != priemman::v1::MEDIA_TYPE_IMAGE &&
            media.type() != priemman::v1::MEDIA_TYPE_VIDEO) {
            return std::unexpected{"Every media item must be an image or video"};
        }
        if (media.order() < 0) {
            return std::unexpected{"Media order cannot be negative"};
        }

        const auto& media_id = media.id().value();
        if (!media_id.empty()) {
            if (!media_ids.insert(media_id).second) {
                return std::unexpected{"Media IDs must be unique"};
            }
            if (media_id == input.cover_media_id()) cover_found = true;
        }
    }

    if (!cover_found) {
        return std::unexpected{"cover_media_id must reference media in this project"};
    }

    for (const auto& collaborator : input.collaborators()) {
        const auto& collaborator_id = collaborator.user_id().value();
        if (collaborator_id.empty()) {
            return std::unexpected{"Collaborator user ID is required"};
        }
        if (!collaborator_ids.insert(collaborator_id).second) {
            return std::unexpected{"Collaborators must be unique"};
        }
    }

    return true;
}

inline void SaveChildren(
    const database::ProjectRepository& repo,
    const std::string& project_id,
    const priemman::v1::ProjectInput& input
) {
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

inline bool isExceeding1MB(const std::string& content) {
    const size_t max_size = 1024 * 1024; // 1 MB dalam satuan byte
    return content.size() > max_size;
}

}  // namespace priemman::handlers::projects::helpers
