---
sidebar_position: 6
---

# Creator upgrade dan admin

## Creator upgrade

| Method | Path | Keterangan |
| --- | --- | --- |
| `GET` | `/v1/users/me/upgrade` | Status request upgrade terbaru |
| `POST` | `/v1/users/me/upgrade` | Ajukan upgrade; body kosong |

Status request: `none`, `pending`, `approved`, `rejected`, atau `paid`. Request pending/approved tidak dapat diajukan ulang. Admin yang menyetujui request akan menerbitkan invoice 100.000 IDR; setelah pembayaran dikonfirmasi, role user menjadi creator.

## Admin

Semua endpoint berikut membutuhkan user dengan role admin.

| Method | Path | Keterangan |
| --- | --- | --- |
| `GET` | `/v1/admin/users` | List user; query: `limit`, `offset`, `role` |
| `GET` | `/v1/admin/upgrades` | List request upgrade; query `status` |
| `POST` | `/v1/admin/upgrades/review` | Approve/reject request pending |
| `POST` | `/v1/admin/upgrades/confirm-payment` | Konfirmasi pembayaran request approved |

```json title="AdminReviewUpgradeRequest"
{
  "id":{"value":"upgrade-request-uuid"},
  "approve":true,
  "rejection_reason":""
}
```

Kesalahan akses menggunakan `401 UNAUTHORIZED` bila token tidak ada dan `403 FORBIDDEN` bila role bukan admin. Error state upgrade: `UPGRADE_ALREADY_REQUESTED`, `ALREADY_CREATOR`, `REVIEW_FAILED`, atau `CONFIRM_FAILED`.
