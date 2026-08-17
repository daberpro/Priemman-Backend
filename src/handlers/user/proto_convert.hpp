#pragma once

#include <optional>
#include <string>

#include <google/protobuf/util/time_util.h>
#include <proto/user.pb.h>

#include <src/database/account_repository.hpp>
#include <src/database/user_repository.hpp>

namespace priemman::handlers::mapper {

// Timestamp (UTC) -> string SQL "YYYY-MM-DD HH:MM:SS[.ffffff]"
inline std::optional<std::string> TimestampToSql(
    const google::protobuf::Timestamp& ts
) {
    if (ts.seconds() == 0 && ts.nanos() == 0) return std::nullopt;
    std::string s = google::protobuf::util::TimeUtil::ToString(ts);
    for (auto& c : s) {
        if (c == 'T') c = ' ';
    }
    if (!s.empty() && s.back() == 'Z') s.pop_back();
    return s;
}

// "YYYY-MM-DDTHH:MM:SSZ" (hasil DATE_FORMAT) -> Timestamp
inline void SqlToTimestamp(
    const std::optional<std::string>& sql,
    google::protobuf::Timestamp* out
) {
    if (!sql.has_value()) return;
    google::protobuf::util::TimeUtil::FromString(*sql, out);
}

inline priemman::v1::WorkExperience ToProto(const database::WorkExperienceRow& r) {
    priemman::v1::WorkExperience w;
    w.mutable_id()->set_value(r.id);
    w.set_title(r.title);
    w.set_company(r.company);
    w.set_is_current(r.is_current != 0);
    SqlToTimestamp(r.start_date, w.mutable_start_date());
    SqlToTimestamp(r.end_date, w.mutable_end_date());
    w.set_description(r.description);
    return w;
}

inline database::WorkExperienceRow ToRow(const priemman::v1::WorkExperience& w) {
    database::WorkExperienceRow r;
    r.id = w.id().value();
    r.title = w.title();
    r.company = w.company();
    r.is_current = w.is_current() ? 1 : 0;
    r.start_date = TimestampToSql(w.start_date());
    r.end_date = w.is_current() ? std::nullopt : TimestampToSql(w.end_date());
    r.description = w.description();
    return r;
}

inline priemman::v1::ConnectedAccount ToProto(const database::ConnectedAccountRow& r) {
    priemman::v1::ConnectedAccount c;
    if (r.platform == "INSTAGRAM") c.set_platform(priemman::v1::CONNECTED_PLATFORM_INSTAGRAM);
    else if (r.platform == "LINKEDIN") c.set_platform(priemman::v1::CONNECTED_PLATFORM_LINKEDIN);
    else if (r.platform == "GITHUB") c.set_platform(priemman::v1::CONNECTED_PLATFORM_GITHUB);
    else c.set_platform(priemman::v1::CONNECTED_PLATFORM_UNSPECIFIED);

    c.set_handle_or_url(r.handle_or_url);
    c.set_verified(r.verified != 0);

    // r.connected_at sekarang std::optional<std::string>, langsung pass saja
    SqlToTimestamp(r.connected_at, c.mutable_connected_at());

    return c;
}
inline priemman::v1::User ToUserProto(
    const database::User& u,
    const std::optional<database::AboutInfo>& about,
    const std::vector<database::WorkExperienceRow>& wx,
    const std::vector<database::ConnectedAccountRow>& ca
) {
    priemman::v1::User p;
    p.mutable_id()->set_value(u.id);
    p.set_email(u.email);
    p.set_first_name(u.first_name);
    p.set_last_name(u.last_name);
    p.set_headline(u.headline);
    p.set_company(u.company);
    p.mutable_location()->set_country(u.country);
    p.mutable_location()->set_city(u.city);
    p.set_website_url(u.website_url);
    p.set_avatar_url(u.avatar_url);
    p.set_is_onboarded(u.is_onboarded != 0);
    if (about.has_value()) {
        p.mutable_about_me()->set_title(about->title);
        p.mutable_about_me()->set_description(about->description);
    }
    for (const auto& w : wx) *p.add_work_experience() = ToProto(w);
    for (const auto& a : ca) *p.add_connected_accounts() = ToProto(a);
    return p;
}

}  // namespace priemman::handlers::mapper
