---
sidebar_position: 1
slug: /
---

# Priemman API

Backend Priemman memakai C++23 dan userver. Sebagian besar endpoint memakai **Protocol Buffers** (`application/x-protobuf`); contoh payload di dokumen ini ditampilkan sebagai JSON agar mudah dibaca. Client harus melakukan serialisasi/deserialisasi memakai file `.proto` pada repository.

## Base URL dan konvensi

| Item | Nilai |
| --- | --- |
| Base URL | URL deployment API yang aktif |
| Rate limit | 60 request/menit/IP |
| Auth | `Authorization: Bearer <session_token>` atau cookie `session` |
| Error | Payload protobuf dengan `code` dan `message` |

`GET /ping` mengembalikan teks `pong`. `GET /v1/info` adalah endpoint JSON yang menyediakan metadata server.

## Alur cepat

1. Kirim OTP melalui `POST /v1/auth/send-otp`.
2. Verifikasi OTP melalui `POST /v1/auth/verify-otp`, lalu simpan `session_token`.
3. Untuk media project atau avatar, upload dahulu lewat `POST /v1/media/upload`.
4. Kirim `Media.url`, `Media.id`, dan `Media.public_id` hasil upload ketika membuat atau memperbarui project/profil.

:::important Media lifecycle

Media upload awalnya berstatus `orphan`. Media yang dipakai project atau avatar harus di-attach lewat endpoint pemiliknya. Orphan dibersihkan secara otomatis setelah 24 jam.

:::

## Status HTTP umum

| Status | Arti |
| --- | --- |
| `400` | Body, parameter, atau relasi data tidak valid |
| `401` | Sesi tidak ada atau tidak valid |
| `403` | Peran pengguna tidak memiliki akses |
| `404` | Resource tidak ditemukan atau tidak dapat dilihat |
| `409` | State resource tidak mendukung operasi yang diminta |
| `429` | Rate limit atau cooldown OTP |

Lanjutkan ke [Autentikasi](./auth) untuk mendapatkan session token.
