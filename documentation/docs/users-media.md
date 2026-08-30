---
sidebar_position: 3
---

# User dan media

Semua endpoint pada halaman ini membutuhkan session token, kecuali dinyatakan lain.

## Profil: `GET`, `PUT`, atau `PATCH /v1/users/me`

`GET` mengembalikan profil lengkap user yang login. `PUT` dan `PATCH` menyimpan informasi dasar dan mengembalikan profil terbaru.

```json title="UpdateBasicInfoRequest"
{
  "first_name":"Budi",
  "last_name":"Santoso",
  "headline":"Product Designer",
  "company":"Studio Delapan",
  "location":{"country":"Indonesia","city":"Bandung"},
  "website_url":"https://budi.design",
  "about_me":{"title":"Halo!","description":"Desainer produk."},
  "avatar_replace_media": {
    "url":"https://res.cloudinary.com/.../avatar.avif",
    "type":"MEDIA_TYPE_IMAGE",
    "id":{"value":"media-uuid"},
    "public_id":"projects/user/avatar"
  }
}
```

`avatar_replace_media` opsional. Jika dikirim, harus berupa image yang sebelumnya diupload oleh user dan masih orphan. Avatar yang tidak dikirim akan dipertahankan.

## Work experience

| Method | Path | Keterangan |
| --- | --- | --- |
| `GET` | `/v1/users/me/work-experiences` | Semua entri milik user |
| `POST` | `/v1/users/me/work-experiences` | Upsert; `entry.id` kosong untuk membuat data baru |
| `DELETE` | `/v1/users/me/work-experiences` | Hapus, dengan `DeleteWorkExperienceRequest` di body |

## Connected accounts

| Method | Path | Keterangan |
| --- | --- | --- |
| `GET` | `/v1/users/me/connected-accounts` | Semua akun yang terhubung |
| `POST` | `/v1/users/me/connected-accounts` | Tambah atau perbarui akun terhubung |
| `DELETE` | `/v1/users/me/connected-accounts` | Hapus berdasarkan platform di body |

Platform yang tersedia: `CONNECTED_PLATFORM_INSTAGRAM`, `CONNECTED_PLATFORM_LINKEDIN`, dan `CONNECTED_PLATFORM_GITHUB`.

## `POST /v1/media/upload`

Mengunggah maksimal 10 file gambar/video melalui `multipart/form-data`. Nama field wajib `file`; tiap file maksimal 10 MB.

```bash
curl -X POST "$API_URL/v1/media/upload" \
  -H "Authorization: Bearer $SESSION_TOKEN" \
  -F "file=@hero.avif"
```

Response berupa `UploadMediaBatchResponse.items`; simpan `url`, `type`, `id`, dan `public_id` setiap item. Error umum: `INVALID_CONTENT_TYPE`, `MISSING_FILE`, `EMPTY_FILE`, `FILE_TOO_LARGE`, `CLOUDINARY_ERROR`, dan `DB_ERROR`.
