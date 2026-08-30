---
sidebar_position: 7
---

# Referensi protobuf

Source of truth skema request/response berada di direktori [`proto`](https://github.com/daberpro/Priemman-Backend/tree/main/proto) repository.

Untuk nama message yang dipakai per endpoint, lihat [Peta request dan response protobuf](./protobuf-api-map).

## Content type

| Endpoint | Request | Response |
| --- | --- | --- |
| OTP, profil, project, collection, upgrade, admin | `application/x-protobuf` | `application/x-protobuf` |
| Upload media | `multipart/form-data` | `application/x-protobuf` |
| `/ping` | Tidak ada | `text/plain` |
| `/v1/info` | Tidak ada | `application/json` |

## Enum penting

| Enum | Nilai |
| --- | --- |
| `MediaType` | `MEDIA_TYPE_IMAGE`, `MEDIA_TYPE_VIDEO` |
| `ProjectVisibility` | `PROJECT_VISIBILITY_PUBLIC`, `PROJECT_VISIBILITY_UNLISTED`, `PROJECT_VISIBILITY_DRAFT` |
| `ProjectStatus` | `PROJECT_STATUS_DRAFT`, `PROJECT_STATUS_PUBLISHED`, `PROJECT_STATUS_ARCHIVED` |
| `CollectionVisibility` | `COLLECTION_VISIBILITY_PUBLIC`, `COLLECTION_VISIBILITY_PRIVATE` |

## HTTP OPTIONS

Endpoint API mendukung `OPTIONS` untuk preflight CORS. Origin yang diizinkan dikonfigurasi pada server; header yang didukung mencakup `Content-Type` dan `Authorization`.
