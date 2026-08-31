---
sidebar_position: 2
---

# Peta request dan response protobuf

Referensi cepat message protobuf tiap endpoint. Semua nama berada di package `priemman.v1`; source-of-truth ada di direktori `proto/`. Kecuali baris yang tertulis JSON, teks, redirect, atau multipart, request dan response memakai `application/x-protobuf`.

## Error response

Response error protobuf menggunakan **`Result`** dari `common.proto`. Kode error berada pada `Result.error_detail.code`.

## Utility dan autentikasi

| Method | Endpoint | Request | Response |
| --- | --- | --- | --- |
| `GET` | `/ping` | — | Teks `pong` (`text/plain`) |
| `GET` | `/v1/info` | — | JSON metadata API |
| `POST` | `/v1/auth/send-otp` | `SendOtpRequest` | `SendOtpResponse` |
| `POST` | `/v1/auth/verify-otp` | `VerifyOtpRequest` | `VerifyOtpResponse` |
| `POST` | `/v1/auth/logout` | `LogoutRequest` | `LogoutResponse` |
| `GET` | `/v1/auth/google` | — | Redirect OAuth (`302`) |
| `GET` | `/v1/auth/callback/google` | Query `code`, `state` | Redirect dashboard (`302`) |
| `GET` | `/v1/auth/github` | — | Redirect OAuth (`302`) |
| `GET` | `/v1/auth/callback/github` | Query `code`, `state` | Redirect dashboard (`302`) |

## User dan media

| Method | Endpoint | Request | Response |
| --- | --- | --- | --- |
| `GET` | `/v1/users/me` | — | `User` |
| `PUT` / `PATCH` | `/v1/users/me` | `UpdateBasicInfoRequest` | `User` |
| `GET` | `/v1/users/me/work-experiences` | — | `ListWorkExperienceResponse` |
| `POST` | `/v1/users/me/work-experiences` | `UpsertWorkExperienceRequest` | `WorkExperience` |
| `DELETE` | `/v1/users/me/work-experiences` | `DeleteWorkExperienceRequest` | `DeleteResponse` |
| `GET` | `/v1/users/me/connected-accounts` | — | `ListConnectedAccountsResponse` |
| `POST` | `/v1/users/me/connected-accounts` | `UpsertConnectedAccountRequest` | `UpsertConnectedAccountRequest` |
| `DELETE` | `/v1/users/me/connected-accounts` | `DeleteConnectedAccountRequest` | `DeleteResponse` |
| `POST` | `/v1/media/upload` | `multipart/form-data` field `file` | `UploadMediaBatchResponse` |

## Project dan collection

| Method | Endpoint | Request | Response |
| --- | --- | --- | --- |
| `POST` | `/v1/projects` | `CreateProjectRequest` | `ProjectResponse` |
| `GET` | `/v1/projects/list` | Query `page_size`, `page_token`, `status` | `ListProjectsResponse` |
| `GET` | `/v1/projects/{id}` | Path `id` | `ProjectResponse` |
| `PUT` | `/v1/projects/{id}` | `UpdateProjectRequest` | `ProjectResponse` |
| `DELETE` | `/v1/projects/{id}` | Path `id` | `DeleteResponse` |
| `GET` | `/v1/collections` | Query `id` opsional | `ListCollectionsResponse` / `CollectionResponse` |
| `POST` | `/v1/collections` | `CreateCollectionRequest` | `CollectionResponse` |
| `PUT` | `/v1/collections` | `UpdateCollectionRequest` | `CollectionResponse` |
| `DELETE` | `/v1/collections` | `DeleteCollectionRequest` | `DeleteResponse` |

## Upgrade dan admin

| Method | Endpoint | Request | Response |
| --- | --- | --- | --- |
| `GET` | `/v1/users/me/upgrade` | — | `UpgradeStatus` |
| `POST` | `/v1/users/me/upgrade` | Body kosong | `CreateUpgradeRequestResponse` |
| `GET` | `/v1/admin/users` | Query `limit`, `offset`, `role` | `AdminListUsersResponse` |
| `GET` | `/v1/admin/upgrades` | Query `status` | `AdminListUpgradeRequestsResponse` |
| `POST` | `/v1/admin/upgrades/review` | `AdminReviewUpgradeRequest` | `AdminReviewUpgradeResponse` |
| `POST` | `/v1/admin/upgrades/confirm-payment` | `AdminConfirmPaymentRequest` | `AdminConfirmPaymentResponse` |

## File protobuf

| File | Message utama |
| --- | --- |
| `common.proto` | `ObjectId`, `Location`, `Result`, `DeleteResponse` |
| `auth.proto` | OTP, session, logout |
| `user.proto` | Profil, akun terhubung, upgrade, admin |
| `media.proto` | `Media`, `UploadMediaBatchResponse` |
| `project.proto` | Project, collection, CRUD request/response |
