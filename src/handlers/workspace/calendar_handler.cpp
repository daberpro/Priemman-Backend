#include "calendar_handler.hpp"

#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <userver/http/url.hpp>
#include <utility>
#include <vector>
#include <chrono>

#include <libical/ical.h>

#include <userver/clients/http/response.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/yaml_config/yaml_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {

std::string FormatIcalTime(const icaltimetype& time) {
    char buffer[64];

    if (icaltime_is_date(time)) {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%04d-%02d-%02d",
            time.year,
            time.month,
            time.day
        );
        return buffer;
    }

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%04d-%02d-%02dT%02d:%02d:%02d%s",
        time.year,
        time.month,
        time.day,
        time.hour,
        time.minute,
        time.second,
        icaltime_is_utc(time) ? "Z" : ""
    );

    return buffer;
}

std::string GetPropertyString(
    icalcomponent* component,
    icalproperty_kind kind
) {
    auto* property = icalcomponent_get_first_property(
        component,
        kind
    );

    if (!property) {
        return {};
    }

    const char* value = nullptr;

    switch (kind) {
        case ICAL_SUMMARY_PROPERTY:
            value = icalproperty_get_summary(property);
            break;

        case ICAL_DESCRIPTION_PROPERTY:
            value = icalproperty_get_description(property);
            break;

        case ICAL_LOCATION_PROPERTY:
            value = icalproperty_get_location(property);
            break;

        default:
            break;
    }

    return value ? value : "";
}

} // namespace

namespace priemman::handlers::workspace {

userver::yaml_config::Schema CalendarHandler::GetStaticConfigSchema() {
    auto schema = userver::server::handlers::HttpHandlerBase::GetStaticConfigSchema();
    if (!schema.properties.has_value()) {
        schema.properties.emplace();
    }
    static const std::pair<std::string_view, std::string_view> kProps[] = {
        {"workspace_host", "Workspace host url"},
        {"username", "Workspace username"},
        {"password", "Workspace password"},
    };
    for (const auto& [name, description] : kProps) {
        schema.properties->emplace(
            std::string{name},
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: " + std::string{description} + "\n")
            )
        );
    }
    return schema;
}

std::vector<CalendarEvent> CalendarHandler::ParseIcsEvents(
    std::string_view ics_data
) const {
    std::vector<CalendarEvent> events;
    std::string ics(ics_data);

    icalcomponent* calendar =
        icalcomponent_new_from_string(ics.c_str());

    if (!calendar) {
        return events;
    }

    for (
        auto* component = icalcomponent_get_first_component(
            calendar,
            ICAL_VEVENT_COMPONENT
        );
        component != nullptr;
        component = icalcomponent_get_next_component(
            calendar,
            ICAL_VEVENT_COMPONENT
        )
    ) {
        CalendarEvent event;

        event.title = GetPropertyString(
            component,
            ICAL_SUMMARY_PROPERTY
        );

        event.description = GetPropertyString(
            component,
            ICAL_DESCRIPTION_PROPERTY
        );

        event.location = GetPropertyString(
            component,
            ICAL_LOCATION_PROPERTY
        );

        if (auto* property = icalcomponent_get_first_property(
                component,
                ICAL_DTSTART_PROPERTY
            );
            property != nullptr
        ) {
            event.start = FormatIcalTime(
                icalproperty_get_dtstart(property)
            );
        }

        if (auto* property = icalcomponent_get_first_property(
                component,
                ICAL_DTEND_PROPERTY
            );
            property != nullptr
        ) {
            event.end = FormatIcalTime(
                icalproperty_get_dtend(property)
            );
        }

        events.emplace_back(std::move(event));
    }

    icalcomponent_free(calendar);
    return events;
}

std::string CalendarHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&
) const {
    userver::formats::json::ValueBuilder builder;
    builder["status"] = "error";
    builder["message"] = "";
    builder["username_nextcloud"] = "";
    builder["events"] = userver::formats::json::ValueBuilder(
        userver::formats::common::Type::kArray
    ).ExtractValue();

    const auto user_id = TryAuth(request);

    if (!user_id.has_value()) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kUnauthorized
        );
        builder["message"] = "Unauthorized";

        return userver::formats::json::ToString(
            builder.ExtractValue()
        );
    }

    auto user = _users.FindById(user_id.value());
    if (!user.has_value()) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kNotFound
        );
        return ErrorResult("NOT_FOUND", "User not found");
    }
    const auto& target_user = user.value().email;

    const std::string auth_base = userver::crypto::base64::Base64Encode(
            std::format("{}:{}", _username, _password)
    );

    userver::clients::http::Headers headers;
    headers.InsertOrAppend("OCS-APIRequest", "true");
    headers.InsertOrAppend("Accept", "application/json");
    headers.InsertOrAppend(
        "Authorization",
        std::format("Basic {}", auth_base)
    );

    const std::string user_search_url = std::format(
        "{}/ocs/v1.php/cloud/users?search={}",
        _workspace_host,
        target_user
    );

    auto user_result = _client
        ->CreateRequest()
        .headers(headers)
        .get(user_search_url)
        .timeout(std::chrono::seconds{10})
        .perform();

    if (user_result->IsError()) {
        LOG_ERROR() << "Cannot access Nextcloud user API";

        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadGateway
        );
        builder["message"] =
            "Gagal menghubungi server Nextcloud";

        return userver::formats::json::ToString(
            builder.ExtractValue()
        );
    }

    const auto user_response =
        userver::formats::json::FromString(
            user_result->body()
        );

    const auto users =
        user_response["ocs"]["data"]["users"];

    if (!users.IsArray() || users.GetSize() == 0) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kNotFound
        );
        builder["message"] =
            "User tidak terdaftar di Nextcloud";

        return userver::formats::json::ToString(
            builder.ExtractValue()
        );
    }

    const std::string username =
        users[0].As<std::string>();

    builder["username_nextcloud"] = username;

    const auto encoded_username = userver::http::UrlEncode(username);
    const std::string calendar_url = std::format(
        "{}/remote.php/dav/calendars/{}/personal/?export",
        _workspace_host,
        encoded_username
    );

    userver::clients::http::Headers calendar_headers;
    calendar_headers.InsertOrAppend(
        "Accept",
        "text/calendar"
    );
    calendar_headers.InsertOrAppend(
        "Authorization",
        std::format("Basic {}", auth_base)
    );

    auto calendar_result = _client
        ->CreateRequest()
        .headers(calendar_headers)
        .get(calendar_url)
        .timeout(std::chrono::seconds{10})
        .perform();

    if (calendar_result->IsError()) {
        LOG_ERROR() << "Cannot access Nextcloud calendar API";

        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadGateway
        );
        builder["message"] =
            "Gagal mengambil kalender Nextcloud";

        return userver::formats::json::ToString(
            builder.ExtractValue()
        );
    }

    const auto events = ParseIcsEvents(
        calendar_result->body()
    );

    userver::formats::json::ValueBuilder events_builder(
        userver::formats::common::Type::kArray
    );

    for (const auto& event : events) {
        userver::formats::json::ValueBuilder event_builder(
            userver::formats::common::Type::kObject
        );

        event_builder["title"] = event.title;
        event_builder["description"] = event.description;
        event_builder["start"] = event.start;
        event_builder["end"] = event.end;
        event_builder["location"] = event.location;

        events_builder.PushBack(
            event_builder.ExtractValue()
        );
    }

    builder["status"] = "success";
    builder["username_nextcloud"] = username;
    builder["events"] = events_builder.ExtractValue();

    return userver::formats::json::ToString(
        builder.ExtractValue()
    );
}

} // namespace priemman::handlers::workspace
