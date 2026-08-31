---
sidebar_position: 4
---

# Projects

## Bentuk `ProjectInput`

```json
{
  "title":"Redesign Aplikasi Kasir UMKM",
  "content":"Studi kasus dan proses desain.",
  "tags":["fintech","mobile"],
  "media":[{
    "url":"https://res.cloudinary.com/.../hero.avif",
    "type":"MEDIA_TYPE_IMAGE",
    "order":0,
    "id":{"value":"media-uuid"},
    "public_id":"projects/user/hero"
  }],
  "collaborators":[{"user_id":{"value":"user-uuid"},"role":"Designer"}],
  "visibility":"PROJECT_VISIBILITY_PUBLIC",
  "status":"PROJECT_STATUS_PUBLISHED",
  "cover_media_id":"media-uuid"
}
```

`title` dan `content` wajib diisi; content maksimal 1 MiB. Setiap media memerlukan URL, public ID, tipe IMAGE/VIDEO, serta `order` tidak negatif. `cover_media_id` harus merujuk ke media yang ada pada input. Collaborator harus user yang tersedia dan tidak boleh duplikat.

## `POST /v1/projects`

Membuat project milik pengguna terautentikasi. Semua media harus milik pengguna dan masih orphan. Project dengan status `PROJECT_STATUS_PUBLISHED` memperoleh `published_at` otomatis.

```json title="CreateProjectRequest"
{"input":{"title":"...","content":"...","media":[]}}
```

## `GET /v1/projects/list`

Tanpa token, mengembalikan feed project `PUBLISHED` dan `PUBLIC`. Dengan token, mengembalikan project milik viewer.

| Query | Keterangan |
| --- | --- |
| `page_size` | Default 20, maksimum 50 |
| `page_token` | Offset numerik dari `next_page_token` |
| `status` | Untuk viewer: `DRAFT`, `PUBLISHED`, atau `ARCHIVED` |

## `GET /v1/projects/{id}`

Project publik dapat dibaca semua orang. Project non-publik hanya terlihat bagi owner dan request lain menerima `404`. View project publik bertambah saat dibaca non-owner.

## `PUT /v1/projects/{id}`

Mengganti seluruh project input. Kirim ulang media lama yang tetap ingin dipakai. Media milik user dapat dipakai ulang pada project lain; media yang dilepas akan kembali orphan hanya jika sudah tidak direferensikan project mana pun.

`UpdateProjectRequest.id`, bila diisi, harus sama dengan ID pada URL.

## `DELETE /v1/projects/{id}`

Menghapus project milik owner. Media yang kemudian tidak direferensikan project lain menjadi orphan dan dibersihkan oleh sweeper.
