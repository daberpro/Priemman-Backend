#include "basic_info_handler.hpp"

#include <proto/user.pb.h>
#include <userver/server/http/http_method.hpp>
#include <src/handlers/user/proto_convert.hpp>
#include <src/handlers/media/media_helper.hpp>

namespace priemman::handlers::user {

namespace {

using namespace userver::server::http;  // NOLINT

database::BasicInfoPatch ExistingUserToPatch(
    const database::User& user,
    const std::optional<database::AboutInfo>& about
) {
    database::BasicInfoPatch patch;

    patch.first_name = user.first_name;
    patch.last_name = user.last_name;
    patch.headline = user.headline;
    patch.company = user.company;
    patch.city = user.city;
    patch.country = user.country;
    patch.website_url = user.website_url;
    patch.avatar_url = user.avatar_url;
    if(about.has_value()){
        patch.about_me = about.value();
    }
    return patch;
}

std::string BuildMe(
    const AuthenticatedHandlerBase* /*tag*/,
    const database::UserRepository& users,
    const database::AccountRepository& accounts,
    const std::string& user_id
) {
    auto user = users.FindById(user_id);
    if (!user.has_value()) return "";

    const auto about = users.FindAbout(user_id);
    const auto wx = accounts.ListWorkExperiences(user_id);
    const auto ca = accounts.ListConnectedAccounts(user_id);

    return mapper::ToUserProto(*user, about, wx, ca).SerializeAsString();
}

}  // namespace

std::string BasicInfoHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    auto& res = request.GetHttpResponse();
    res.SetContentType("application/x-protobuf");

    auto user_id = RequireAuth(request);
    if (!user_id.has_value()) {
        return ErrorResult("UNAUTHORIZED", "Missing or invalid session token");
    }

    const auto method = request.GetMethod();

    if (method == HttpMethod::kGet) {
        const auto body = BuildMe(nullptr, _users, _accounts, *user_id);
        if (body.empty()) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "User not found");
        }
        return body;
    }

    if (method == HttpMethod::kPut || method == HttpMethod::kPatch) {
        priemman::v1::UpdateBasicInfoRequest req;
        if (!req.ParseFromString(request.RequestBody())) {
            res.SetStatus(HttpStatus::kBadRequest);
            return ErrorResult("INVALID_BODY", "Invalid request body");
        }

        const auto user = _users.FindById(*user_id);
        if (!user.has_value()) {
            res.SetStatus(HttpStatus::kNotFound);
            return ErrorResult("NOT_FOUND", "User not found");
        }

        const bool replace_avatar = req.has_avatar_replace_media();
        std::vector<std::string> avatar_public_ids;
        if (replace_avatar) {
            const auto& avatar = req.avatar_replace_media();
            if (avatar.type() != priemman::v1::MEDIA_TYPE_IMAGE ||
                avatar.public_id().empty() || avatar.url().empty()) {
                res.SetStatus(HttpStatus::kBadRequest);
                return ErrorResult("INVALID_MEDIA", "Avatar must be an uploaded image with URL and public ID");
            }

            avatar_public_ids.emplace_back(avatar.public_id());
            if (const auto validation = media::ValidateMediaForCreate(
                    _media, avatar_public_ids, *user_id);
                !validation.has_value()) {
                res.SetStatus(HttpStatus::kBadRequest);
                return ErrorResult("INVALID_MEDIA", validation.error());
            }
        }

        auto about_info = _users.FindAbout(user->id.c_str());
        auto patch = ExistingUserToPatch(*user, about_info);

        const auto& about_me = req.about_me();
        if(req.has_first_name()){
            patch.first_name = req.first_name();
        }

        if(req.has_last_name()){
            patch.last_name = req.last_name();
        }

        if(req.has_headline()){
            patch.headline = req.headline();
        }

        if(req.has_company()){
            patch.company = req.company();
        }

        if(req.has_location()){
            patch.country = req.location().country();
            patch.city = req.location().city();
        }

        if(req.has_website_url()){
            patch.website_url = req.website_url();
        }

        if(req.has_about_me()){
            patch.about_me = {
            .title = about_me.title(),
            .description =  about_me.description()
            };
        }

        patch.avatar_url = replace_avatar
            ? req.avatar_replace_media().url()
            : user->avatar_url;

        _users.UpdateBasicInfo(*user_id, patch);
        if (replace_avatar) {
            media::AttachMedia(_media, avatar_public_ids, *user_id);
        }

        const auto body = BuildMe(nullptr, _users, _accounts, *user_id);
        return body;
    }

    res.SetStatus(HttpStatus::kMethodNotAllowed);
    return ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
}

}  // namespace priemman::handlers::user
