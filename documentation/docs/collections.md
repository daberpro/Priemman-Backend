---
sidebar_position: 5
---

# Collections

Semua endpoint collection membutuhkan session token.

| Method | Path | Keterangan |
| --- | --- | --- |
| `GET` | `/v1/collections` | List semua collection milik user |
| `GET` | `/v1/collections?id=<id>` | Detail satu collection |
| `POST` | `/v1/collections` | Membuat collection |
| `PUT` | `/v1/collections` | Mengganti collection dan daftar project |
| `DELETE` | `/v1/collections` | Menghapus collection; ID dikirim melalui body protobuf |

```json title="CollectionInput"
{
  "title":"Karya Terbaik 2026",
  "description":"Kumpulan project pilihan.",
  "visibility":"COLLECTION_VISIBILITY_PUBLIC",
  "project_ids":[{"value":"project-uuid"}]
}
```

`project_ids` harus project milik sendiri. Pada update, daftar `project_ids` menggantikan seluruh isi collection, bukan menambahkan ke daftar sebelumnya.
