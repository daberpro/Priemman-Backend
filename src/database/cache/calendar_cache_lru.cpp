#include "calendar_cache_lru.hpp"

#include <chrono>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <string_view>

#include <libical/ical.h>

#include <userver/clients/http/response.hpp>
#include <userver/crypto/base64.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/url.hpp>
#include <userver/yaml_config/yaml_config.hpp>

namespace {

std::string FormatIcalTime(const icaltimetype& time) {
    char buffer[64];
    if (icaltime_is_date(time)) {
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", time.year,
                      time.month, time.day);
        return buffer;
    }

    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d%s",
                  time.year, time.month, time.day, time.hour, time.minute,
                  time.second, icaltime_is_utc(time) ? "Z" : "");
    return buffer;
}

std::string GetPropertyString(icalcomponent* component, icalproperty_kind kind) {
    auto* property = icalcomponent_get_first_property(component, kind);
    if (!property) return {};

    const char* value = nullptr;
    switch (kind) {
        case ICAL_SUMMARY_PROPERTY: value = icalproperty_get_summary(property); break;
        case ICAL_DESCRIPTION_PROPERTY: value = icalproperty_get_description(property); break;
        case ICAL_LOCATION_PROPERTY: value = icalproperty_get_location(property); break;
        default: break;
    }
    return value ? value : "";
}

userver::formats::json::Value ParseIcsEvents(std::string_view ics_data) {
    userver::formats::json::ValueBuilder events(
        userver::formats::common::Type::kArray);
    std::string ics{ics_data};
    icalcomponent* calendar = icalcomponent_new_from_string(ics.c_str());
    if (!calendar) return events.ExtractValue();

    for (auto* component = icalcomponent_get_first_component(
             calendar, ICAL_VEVENT_COMPONENT);
         component != nullptr;
         component = icalcomponent_get_next_component(
             calendar, ICAL_VEVENT_COMPONENT)) {
        userver::formats::json::ValueBuilder event(
            userver::formats::common::Type::kObject);
        event["title"] = GetPropertyString(component, ICAL_SUMMARY_PROPERTY);
        event["description"] = GetPropertyString(component, ICAL_DESCRIPTION_PROPERTY);
        event["location"] = GetPropertyString(component, ICAL_LOCATION_PROPERTY);

        if (auto* property = icalcomponent_get_first_property(
                component, ICAL_DTSTART_PROPERTY)) {
            event["start"] = FormatIcalTime(icalproperty_get_dtstart(property));
        } else {
            event["start"] = "";
        }
        if (auto* property = icalcomponent_get_first_property(
                component, ICAL_DTEND_PROPERTY)) {
            event["end"] = FormatIcalTime(icalproperty_get_dtend(property));
        } else {
            event["end"] = "";
        }
        events.PushBack(event.ExtractValue());
    }

    icalcomponent_free(calendar);
    return events.ExtractValue();
}

}  // namespace

namespace priemman::cache::calendar {

userver::yaml_config::Schema CalendarCache::GetStaticConfigSchema() {
    auto schema = userver::cache::LruCacheComponent<
        UserId, CalendarData>::GetStaticConfigSchema();
    if (!schema.properties.has_value()) schema.properties.emplace();

    for (const auto name : {"workspace_host", "username", "password"}) {
        schema.properties->emplace(
            name,
            userver::yaml_config::SchemaPtr(
                userver::yaml_config::impl::SchemaFromString(
                    "type: string\ndescription: Calendar service credential\n")));
    }
    return schema;
}

CalendarCache::CalendarCache(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : userver::cache::LruCacheComponent<UserId, CalendarData>(config, context),
      _mysql_cluster(context.FindComponent<userver::storages::mysql::Component>(
          "database").GetCluster()),
      _users(&_mysql_cluster),
      _client(&context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
      _workspace_host(config["workspace_host"].As<std::string>()),
      _username(config["username"].As<std::string>()),
      _password(config["password"].As<std::string>()) {}

CalendarData CalendarCache::DoGetByKey(const UserId& user_id) {
    const auto user = _users.FindById(user_id);
    if (!user.has_value()) throw std::runtime_error("USER_NOT_FOUND");

    const std::string auth_base = userver::crypto::base64::Base64Encode(
        std::format("{}:{}", _username, _password));
    userver::clients::http::Headers headers;
    headers.InsertOrAppend("OCS-APIRequest", "true");
    headers.InsertOrAppend("Accept", "application/json");
    headers.InsertOrAppend("Authorization", std::format("Basic {}", auth_base));

    const std::string user_url = std::format(
        "{}/ocs/v1.php/cloud/users?search={}", _workspace_host,
        user->email);
    auto user_result = _client->CreateRequest().headers(headers).get(user_url)
                           .timeout(std::chrono::seconds{10}).perform();
    if (user_result->IsError()) throw std::runtime_error("PLATFORM_UNAVAILABLE");

    const auto users = userver::formats::json::FromString(user_result->body())
                           ["ocs"]["data"]["users"];
    if (!users.IsArray() || users.GetSize() == 0) {
        throw std::runtime_error("PLATFORM_USER_NOT_FOUND");
    }
    const std::string username = users[0].As<std::string>();

    headers.clear();
    headers.InsertOrAppend("Accept", "text/calendar");
    headers.InsertOrAppend("Authorization", std::format("Basic {}", auth_base));
    const std::string calendar_url = std::format(
        "{}/remote.php/dav/calendars/{}/personal/?export", _workspace_host,
        userver::http::UrlEncode(username));
    auto calendar_result = _client->CreateRequest().headers(headers).get(calendar_url)
                              .timeout(std::chrono::seconds{10}).perform();
    if (calendar_result->IsError()) throw std::runtime_error("CALENDAR_UNAVAILABLE");

    userver::formats::json::ValueBuilder response;
    response["status"] = "success";
    response["message"] = "";
    response["username"] = username;
    response["events"] = ParseIcsEvents(calendar_result->body());
    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace priemman::cache::calendar
