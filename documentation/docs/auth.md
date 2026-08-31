---
sidebar_position: 2
---

# Autentikasi

Semua request body OTP menggunakan protobuf. Header autentikasi baru diperlukan setelah token diperoleh.

## `POST /v1/auth/send-otp`

Mengirim OTP enam digit ke email. OTP berlaku lima menit dan pengiriman berikutnya memiliki cooldown 30 detik.

```json title="SendOtpRequest"
{"email":"budi@example.com"}
```

Error penting: `EMAIL_REQUIRED`, `INVALID_REQUEST_BODY`, `EMAIL_SUSPENDED`, `IP_SUSPENDED`, dan `RATE_LIMITED`.

## `POST /v1/auth/verify-otp`

Memverifikasi OTP, membuat session, serta memasang cookie `session` HTTP-only.

```json title="VerifyOtpRequest"
{"email":"budi@example.com","otp":"482913"}
```

```json title="VerifyOtpResponse"
{"session_token":"sess_...","is_new_user":false}
```

Gunakan `session_token` sebagai bearer token. Bila `is_new_user` bernilai `true`, arahkan pengguna ke onboarding.

## `POST /v1/auth/logout`

Menghapus session dan cookie browser.

```json title="LogoutRequest"
{"session_token":"sess_..."}
```

## OAuth Google dan GitHub

| Endpoint | Keterangan |
| --- | --- |
| `GET /v1/auth/google` | Mulai redirect OAuth Google |
| `GET /v1/auth/callback/google?code=&state=` | Callback Google |
| `GET /v1/auth/github` | Mulai redirect OAuth GitHub |
| `GET /v1/auth/callback/github?code=&state=` | Callback GitHub |

Mulai OAuth dengan navigasi browser, bukan `fetch`:

```js
window.location.href = `${API_URL}/v1/auth/google`;
```

Server memakai cookie `oauth_state` untuk proteksi CSRF, lalu mengarahkan pengguna ke dashboard berdasarkan perannya. Callback dapat menghasilkan `INVALID_OR_EXPIRED_STATE` atau `OAUTH_PROFILE_INCOMPLETE`.
