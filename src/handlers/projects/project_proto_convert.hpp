#pragma once

#include <proto/project.pb.h>
#include <src/database/collection_repository.hpp>
#include <src/database/project_repository.hpp>
#include <src/handlers/user/proto_convert.hpp>

namespace priemman::handlers::mapper {

inline std::string VisibilityToString(const priemman::v1::ProjectVisibility v) {
    switch (v) {
        case priemman::v1::PROJECT_VISIBILITY_PUBLIC: return "PUBLIC";
        case priemman::v1::PROJECT_VISIBILITY_UNLISTED: return "UNLISTED";
        default: return "DRAFT";
    }
}

inline priemman::v1::ProjectVisibility StringToVisibility(const std::string& s) {
    if (s == "PUBLIC") return priemman::v1::PROJECT_VISIBILITY_PUBLIC;
    if (s == "UNLISTED") return priemman::v1::PROJECT_VISIBILITY_UNLISTED;
    return priemman::v1::PROJECT_VISIBILITY_DRAFT;
}

inline std::string StatusToString(const priemman::v1::ProjectStatus s) {
    switch (s) {
        case priemman::v1::PROJECT_STATUS_PUBLISHED: return "PUBLISHED";
        case priemman::v1::PROJECT_STATUS_ARCHIVED: return "ARCHIVED";
        default: return "DRAFT";
    }
}

inline priemman::v1::ProjectStatus StringToStatus(const std::string& s) {
    if (s == "PUBLISHED") return priemman::v1::PROJECT_STATUS_PUBLISHED;
    if (s == "ARCHIVED") return priemman::v1::PROJECT_STATUS_ARCHIVED;
    return priemman::v1::PROJECT_STATUS_DRAFT;
}

inline priemman::v1::Project ToProto(
    const database::ProjectRowPopulated& row,
    const std::vector<std::string>& tags,
    const std::vector<database::ProjectMediaRow>& media,
    const std::vector<database::ProjectCollaboratorRow>& collabs
) {
    priemman::v1::Project p;
    p.mutable_id()->set_value(row.project.id);
    p.mutable_owner_id()->set_value(row.project.owner_id);
    p.set_title(row.project.title);
    p.set_slug(row.project.slug);
    p.set_content(row.project.content);
    p.set_cover_media_id(row.project.cover_media_id.value_or(""));
    p.set_visibility(StringToVisibility(row.project.visibility));
    p.set_status(StringToStatus(row.project.status));
    p.mutable_metrics()->set_views(row.project.views);
    p.mutable_metrics()->set_likes(row.project.likes);
    p.mutable_metrics()->set_saves(row.project.saves);

    for (const auto& t : tags) p.add_tags(t);

    for (const auto& m : media) {
        auto* pm = p.add_media();
        pm->mutable_id()->set_value(m.id);
        pm->set_url(m.url);
        pm->set_type(m.media_type == "VIDEO"
            ? priemman::v1::MEDIA_TYPE_VIDEO
            : priemman::v1::MEDIA_TYPE_IMAGE);
        pm->set_order(static_cast<std::int32_t>(m.sort_order));
        pm->set_public_id(m.cloudinary_public_id);
    }

    for (const auto& c : collabs) {
        auto* pc = p.add_collaborators();
        pc->mutable_user_id()->set_value(c.user_id);
        pc->set_role(c.role);
    }

    SqlToTimestamp(row.project.created_at, p.mutable_created_at());
    SqlToTimestamp(row.project.updated_at, p.mutable_updated_at());
    SqlToTimestamp(row.project.published_at, p.mutable_published_at());
    p.mutable_author()->mutable_id()->set_value(row.author.id);
    p.mutable_author()->set_first_name(row.author.first_name);
    p.mutable_author()->set_first_name(row.author.last_name);
    p.mutable_author()->set_avatar_url(row.author.avatar_url);

    return p;
}

inline priemman::v1::Collection ToProto(
    const database::CollectionRow& row,
    const std::vector<std::string>& project_ids
) {
    priemman::v1::Collection c;
    c.mutable_id()->set_value(row.id);
    c.mutable_owner_id()->set_value(row.owner_id);
    c.set_title(row.title);
    c.set_description(row.description);
    c.set_visibility(row.visibility == "PUBLIC"
        ? priemman::v1::COLLECTION_VISIBILITY_PUBLIC
        : priemman::v1::COLLECTION_VISIBILITY_PRIVATE);
    for (const auto& id : project_ids) c.add_project_ids()->set_value(id);
    SqlToTimestamp(row.created_at, c.mutable_created_at());
    SqlToTimestamp(row.updated_at, c.mutable_updated_at());
    return c;
}

}  // namespace priemman::handlers::mapper
