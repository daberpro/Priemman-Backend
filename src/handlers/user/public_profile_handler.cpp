#include "public_profile_handler.hpp"

namespace priemman::handlers::user {

    PublicProfileHandler::PublicProfileHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    ): AuthenticatedHandlerBase(config,context) {}

    std::string PublicProfileHandler::HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&
    ) const {

        auto& res = request.GetHttpResponse();
        res.SetContentType("application/x-protobuf");

        if (request.GetMethod() != userver::server::http::HttpMethod::kGet) {
            res.SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
            return utils::ErrorResult("METHOD_NOT_ALLOWED", "Unsupported method");
        }

        auto user_id{request.GetArg("user_id")};
        database::UserProfileAggregate user_profile;

        auto public_info_profile{_users.GetPublicProfile(user_id)};
        if(public_info_profile){
            user_profile.profile = *public_info_profile;
            auto experiences{_accounts.ListWorkExperiences(user_id)};
            if(experiences.empty()){
                LOG_ERROR() << '\n' << "Cannot get user public profile" << '\n';
                return ErrorResult("ERROR_GET_PROFILE","Cannot get user public profile");
            }
            user_profile.work_experiences = std::move(experiences);
        }else{
            LOG_ERROR() << '\n' << public_info_profile.error() << '\n';
            return ErrorResult("ERROR_GET_PROFILE","Cannot get user public profile");
        }

        auto result{mapper::ToProto(user_profile)};
        return result.SerializeAsString();

    };

}