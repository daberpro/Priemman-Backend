"""
Client test HTTP + Protobuf untuk Priemman backend.

Fitur:
  - Token disimpan otomatis di `.session` supaya tidak perlu OTP setiap run.
  - Kalau token invalid (401), auto-clear file dan minta login ulang sekali.
  - Logout otomatis menghapus `.session`.
  - Menu interaktif untuk test semua endpoint.
  - Upload media ke Cloudinary via /v1/media/upload
  - Uji keamanan: suspend email (3x OTP salah) & blokir IP (>=5 gagal per IP)
  - Upgrade ke creator (request -> review admin -> invoice -> konfirmasi bayar)
  - Admin: list users & kelola upgrade requests
  - Uji throttle global per-IP (60 req/menit -> 429)

Jalankan:  python test_py/main.py
"""

import os
import sys
import time
import hashlib
import requests
import mimetypes
from pathlib import Path

import auth_pb2
import user_pb2
import common_pb2
import project_pb2
import media_pb2

# ============================================================
# KONFIGURASI
# ============================================================
BASE_URL = "http://localhost:8080"
SEND_OTP_URL = f"{BASE_URL}/v1/auth/send-otp"
VERIFY_OTP_URL = f"{BASE_URL}/v1/auth/verify-otp"
LOGOUT_URL = f"{BASE_URL}/v1/auth/logout"

GET_PROFILE_URL = f"{BASE_URL}/v1/users/me"
UPDATE_PROFILE_URL = f"{BASE_URL}/v1/users/me"
LIST_WORK_EXP_URL = f"{BASE_URL}/v1/users/me/work-experiences"
UPSERT_WORK_EXP_URL = f"{BASE_URL}/v1/users/me/work-experiences"
DELETE_WORK_EXP_URL = f"{BASE_URL}/v1/users/me/work-experiences"
LIST_CONNECTED_URL = f"{BASE_URL}/v1/users/me/connected-accounts"
DELETE_CONNECTED_URL = f"{BASE_URL}/v1/users/me/connected-accounts"

CREATE_PROJECT_URL = f"{BASE_URL}/v1/projects"
LIST_PROJECTS_URL = f"{BASE_URL}/v1/projects/list"
PROJECT_DETAIL_URL = f"{BASE_URL}/v1/projects/{{}}"  # format dengan .format(id)
COLLECTIONS_URL = f"{BASE_URL}/v1/collections"
UPLOAD_MEDIA_URL = f"{BASE_URL}/v1/media/upload"

UPGRADE_URL = f"{BASE_URL}/v1/users/me/upgrade"
ADMIN_USERS_URL = f"{BASE_URL}/v1/admin/users"
ADMIN_UPGRADES_URL = f"{BASE_URL}/v1/admin/upgrades"
ADMIN_UPGRADES_REVIEW_URL = f"{BASE_URL}/v1/admin/upgrades/review"
ADMIN_UPGRADES_CONFIRM_URL = f"{BASE_URL}/v1/admin/upgrades/confirm-payment"

SESSION_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".session")

PROTO_HEADERS = {
    "Content-Type": "application/x-protobuf",
    "Accept": "application/x-protobuf",
}

http = requests.Session()


# ============================================================
# SESSION PERSISTENCE
# ============================================================

def save_token(token: str) -> None:
    with open(SESSION_FILE, "w") as f:
        f.write(token)
    print(f"  (token disimpan ke {SESSION_FILE})")


def load_token() -> str | None:
    if not os.path.exists(SESSION_FILE):
        return None
    with open(SESSION_FILE, "r") as f:
        token = f.read().strip()
    return token or None


def clear_token() -> None:
    if os.path.exists(SESSION_FILE):
        os.remove(SESSION_FILE)
        print(f"  (token dihapus dari {SESSION_FILE})")


# ============================================================
# HELPERS
# ============================================================

def show(status: int, text: str) -> None:
    print(f"-> HTTP {status}")
    print(text)
    print("-" * 60)


def is_proto(resp: requests.Response) -> bool:
    return "protobuf" in resp.headers.get("Content-Type", "")


def show_error(resp: requests.Response) -> None:
    if is_proto(resp):
        try:
            err = common_pb2.Result()
            err.ParseFromString(resp.content)
            show(
                resp.status_code,
                f"error code : {err.error_detail.code}\n"
                f"message    : {err.error_detail.message or err.message}",
            )
            return
        except Exception:
            pass
    show(resp.status_code, resp.text)


def auth_headers(token: str) -> dict:
    return {
        **PROTO_HEADERS,
        "Authorization": f"Bearer {token}",
    }


ROLE_NAMES = {
    user_pb2.USER_ROLE_UNSPECIFIED: "unspecified",
    user_pb2.USER_ROLE_USER: "user",
    user_pb2.USER_ROLE_CREATOR: "creator",
    user_pb2.USER_ROLE_ADMIN: "admin",
}


def fmt_ts(ts) -> str:
    """Format google.protobuf.Timestamp jadi string yang mudah dibaca."""
    if ts is None or (ts.seconds == 0 and ts.nanos == 0):
        return "-"
    from datetime import datetime, timezone
    return datetime.fromtimestamp(ts.seconds, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")


# ============================================================
# AUTH FUNCTIONS
# ============================================================

def send_otp(email: str) -> bool:
    req = auth_pb2.SendOtpRequest()
    req.email = email

    resp = http.post(SEND_OTP_URL, data=req.SerializeToString(), headers=PROTO_HEADERS)

    if resp.status_code == 200 and is_proto(resp):
        out = auth_pb2.SendOtpResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code,
             f"success        : {out.success}\n"
             f"message        : {out.message}\n"
             f"cooldown_seconds: {out.cooldown_seconds}")
        return out.success

    show_error(resp)
    return False


def verify_otp(email: str, otp: str) -> auth_pb2.VerifyOtpResponse | None:
    req = auth_pb2.VerifyOtpRequest()
    req.email = email
    req.otp = otp

    resp = http.post(VERIFY_OTP_URL, data=req.SerializeToString(), headers=PROTO_HEADERS)

    if resp.status_code == 200 and is_proto(resp):
        out = auth_pb2.VerifyOtpResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code,
             f"session_token : {out.session_token}\n"
             f"is_new_user   : {out.is_new_user}")
        return out

    show_error(resp)
    return None


def logout(token: str) -> bool:
    req = auth_pb2.LogoutRequest()
    req.session_token = token

    resp = http.post(LOGOUT_URL, data=req.SerializeToString(), headers=PROTO_HEADERS)

    if resp.status_code == 200 and is_proto(resp):
        out = auth_pb2.LogoutResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code, f"success: {out.success}")
        clear_token()
        return out.success

    show_error(resp)
    return False


def interactive_login() -> str | None:
    email = input("Email untuk login: ").strip().lower()
    if not email:
        print("Email kosong.")
        return None

    print("\n== [1] Kirim OTP ==")
    if not send_otp(email):
        print("Gagal mengirim OTP.")
        return None

    otp = input("Masukkan kode OTP dari email: ").strip()

    print("\n== [2] Verifikasi OTP ==")
    result = verify_otp(email, otp)
    if result is None:
        print("OTP tidak valid / expired.")
        return None

    print("\nLOGIN BERHASIL")
    print(f"  token     : {result.session_token}")
    print(f"  user baru : {result.is_new_user}")
    return result.session_token


# ============================================================
# USER API FUNCTIONS
# ============================================================

def get_profile(token: str) -> user_pb2.User | None:
    print("\n== [A] GET Profile ==")
    resp = http.get(GET_PROFILE_URL, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        user = user_pb2.User()
        user.ParseFromString(resp.content)
        show(resp.status_code,
             f"id            : {user.id.value}\n"
             f"email         : {user.email}\n"
             f"first_name    : {user.first_name}\n"
             f"last_name     : {user.last_name}\n"
             f"headline      : {user.headline}\n"
             f"company       : {user.company}\n"
             f"city          : {user.location.city}\n"
             f"country       : {user.location.country}\n"
             f"website_url   : {user.website_url}\n"
             f"avatar_url    : {user.avatar_url}\n"
             f"is_onboarded  : {user.is_onboarded}\n"
             f"role          : {ROLE_NAMES.get(user.role, user.role)}\n"
             f"work_exp_count: {len(user.work_experience)}\n"
             f"connected_count: {len(user.connected_accounts)}")
        return user

    show_error(resp)
    return None


def update_profile(token: str) -> bool:
    print("\n== [B] UPDATE Profile ==")
    req = user_pb2.UpdateBasicInfoRequest()
    req.first_name = "John"
    req.last_name = "Doe"
    req.headline = "Software Engineer"
    req.company = "Tech Corp"
    req.location.country = "Indonesia"
    req.location.city = "Jakarta"
    req.website_url = "https://johndoe.dev"

    resp = http.put(UPDATE_PROFILE_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        user = user_pb2.User()
        user.ParseFromString(resp.content)
        show(resp.status_code,
             f"first_name    : {user.first_name}\n"
             f"last_name     : {user.last_name}\n"
             f"headline      : {user.headline}\n"
             f"company       : {user.company}\n"
             f"city          : {user.location.city}\n"
             f"country       : {user.location.country}\n"
             f"website_url   : {user.website_url}")
        return True

    show_error(resp)
    return False


def list_work_experiences(token: str) -> list[str]:
    print("\n== [C] LIST Work Experiences ==")
    resp = http.get(LIST_WORK_EXP_URL, headers=auth_headers(token))
    work_ids = []

    if resp.status_code == 200 and is_proto(resp):
        response = user_pb2.ListWorkExperienceResponse()
        response.ParseFromString(resp.content)
        show(resp.status_code, f"Total work experiences: {len(response.entries)}")
        for i, wx in enumerate(response.entries, 1):
            print(f"  [{i}] {wx.id.value}")
            print(f"      Title: {wx.title}")
            print(f"      Company: {wx.company}")
            print(f"      Current: {wx.is_current}")
            print(f"      Description: {wx.description[:50]}...")
            work_ids.append(wx.id.value)
    else:
        show_error(resp)

    return work_ids


def upsert_work_experience(token: str) -> str | None:
    print("\n== [D] UPSERT Work Experience (Create) ==")
    req = user_pb2.UpsertWorkExperienceRequest()
    wx = req.entry
    wx.id.value = ""
    wx.title = "Senior Backend Engineer"
    wx.company = "Priemman Studio"
    wx.is_current = True
    wx.start_date.seconds = 1672531200
    wx.description = "Building high-performance C++ microservices with userver framework."

    resp = http.post(UPSERT_WORK_EXP_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        result = user_pb2.WorkExperience()
        result.ParseFromString(resp.content)
        show(resp.status_code,
             f"id          : {result.id.value}\n"
             f"title       : {result.title}\n"
             f"company     : {result.company}\n"
             f"is_current  : {result.is_current}")
        return result.id.value

    show_error(resp)
    return None


def delete_work_experience(token: str, work_id: str) -> bool:
    print(f"\n== [E] DELETE Work Experience (ID: {work_id}) ==")
    req = user_pb2.DeleteWorkExperienceRequest()
    req.id.value = work_id

    resp = http.delete(DELETE_WORK_EXP_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        result = common_pb2.DeleteResponse()
        result.ParseFromString(resp.content)
        show(resp.status_code, f"success: {result.success}")
        return result.success

    show_error(resp)
    return False


def list_connected_accounts(token: str) -> None:
    print("\n== [F] LIST Connected Accounts ==")
    resp = http.get(LIST_CONNECTED_URL, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        response = user_pb2.ListConnectedAccountsResponse()
        response.ParseFromString(resp.content)
        show(resp.status_code, f"Total connected accounts: {len(response.accounts)}")
        for i, acc in enumerate(response.accounts, 1):
            platform_name = {
                user_pb2.CONNECTED_PLATFORM_INSTAGRAM: "Instagram",
                user_pb2.CONNECTED_PLATFORM_LINKEDIN: "LinkedIn",
                user_pb2.CONNECTED_PLATFORM_GITHUB: "GitHub",
            }.get(acc.platform, "Unknown")
            print(f"  [{i}] {platform_name}")
            print(f"      Handle/URL: {acc.handle_or_url}")
            print(f"      Verified: {acc.verified}")
    else:
        show_error(resp)


def delete_connected_account(token: str) -> bool:
    print("\n== [G] DELETE Connected Account (GitHub) ==")
    req = user_pb2.DeleteConnectedAccountRequest()
    req.platform = user_pb2.CONNECTED_PLATFORM_GITHUB

    resp = http.delete(DELETE_CONNECTED_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        result = common_pb2.DeleteResponse()
        result.ParseFromString(resp.content)
        show(resp.status_code, f"success: {result.success}")
        return result.success

    show_error(resp)
    return False


# ============================================================
# MEDIA UPLOAD FUNCTIONS
# ============================================================

def upload_media(token: str, file_path: str) -> media_pb2.UploadMediaResponse | None:
    print(f"\n== [UPLOAD] Upload Media: {file_path} ==")

    if not os.path.exists(file_path):
        print(f"File tidak ditemukan: {file_path}")
        return None

    # Baca file
    with open(file_path, "rb") as f:
        file_data = f.read()

    # Tentukan content-type dari ekstensi file
    content_type, _ = mimetypes.guess_type(file_path)
    if not content_type:
        content_type = "application/octet-stream"

    # Siapkan multipart form data
    files = {
        "file": (os.path.basename(file_path), file_data, content_type)
    }

    headers = {
        "Authorization": f"Bearer {token}",
        # Jangan set Content-Type, biarkan requests yang set multipart boundary
    }

    print(f"  Uploading {len(file_data)} bytes...")
    resp = http.post(UPLOAD_MEDIA_URL, files=files, headers=headers)

    if resp.status_code == 200 and is_proto(resp):
        batch = media_pb2.UploadMediaBatchResponse()
        batch.ParseFromString(resp.content)

        if not batch.items:
            show_error(resp)
            return None

        # Enum MediaType didefinisikan di project.proto, jadi ada di project_pb2
        for idx, out in enumerate(batch.items, start=1):
            media_type_name = {
                project_pb2.MEDIA_TYPE_IMAGE: "IMAGE",
                project_pb2.MEDIA_TYPE_VIDEO: "VIDEO",
                project_pb2.MEDIA_TYPE_UNSPECIFIED: "UNSPECIFIED",
            }.get(out.type, "UNKNOWN")

            header = f"file {idx}/{len(batch.items)}" if len(batch.items) > 1 else None
            show(resp.status_code,
                 (f"[{header}]\n" if header else "") +
                 f"id            : {out.id.value}\n"
                 f"url           : {out.url}\n"
                 f"public_id     : {out.public_id}\n"
                 f"type          : {media_type_name}\n"
                 f"resource_type : {out.resource_type}")
        return batch.items[0]

    show_error(resp)
    return None


# ============================================================
# CLOUDINARY DELETE (signed destroy, sama seperti Client::Delete backend)
# ============================================================

def load_cloudinary_creds() -> tuple | None:
    """Baca kredensial Cloudinary dari config/config_vars.yaml (parse sederhana)."""
    cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "..", "config", "config_vars.yaml")
    if not os.path.exists(cfg_path):
        return None
    keys = ("cloudinary-cloud-name", "cloudinary-api-key", "cloudinary-api-secret")
    vals: dict[str, str] = {}
    with open(cfg_path, "r") as f:
        for line in f:
            for k in keys:
                if line.startswith(k + ":"):
                    vals[k] = line.split(":", 1)[1].strip().strip("'\"")
    if len(vals) == 3:
        return vals["cloudinary-cloud-name"], vals["cloudinary-api-key"], vals["cloudinary-api-secret"]
    return None


def delete_cloudinary_media(public_id: str) -> str | None:
    """Hapus media langsung dari Cloudinary.

    Return nilai `result` dari Cloudinary ("ok" / "not found"), atau None kalau gagal.
    """
    creds = load_cloudinary_creds()
    if creds is None:
        print("  Kredensial Cloudinary tidak ditemukan di config/config_vars.yaml")
        return None
    cloud_name, api_key, api_secret = creds

    ts = str(int(time.time()))
    # Signature: parameter urut alfabetis (public_id, timestamp) + api_secret, SHA-1
    to_sign = f"public_id={public_id}&timestamp={ts}" + api_secret
    signature = hashlib.sha1(to_sign.encode()).hexdigest()

    url = f"https://api.cloudinary.com/v1_1/{cloud_name}/image/destroy"
    try:
        resp = http.post(url, data={
            "public_id": public_id,
            "api_key": api_key,
            "timestamp": ts,
            "signature": signature,
        }, timeout=30)
    except requests.RequestException as e:
        print(f"  Request ke Cloudinary gagal: {e}")
        return None

    if resp.status_code != 200:
        print(f"  Cloudinary destroy gagal: HTTP {resp.status_code} {resp.text[:200]}")
        return None
    return resp.json().get("result")


def delete_uploaded_media(public_id: str) -> bool:
    if not public_id:
        print("Tidak ada public_id yang bisa dihapus.")
        return False
    print(f"\n== [DELETE] Menghapus media dari Cloudinary: {public_id} ==")
    result = delete_cloudinary_media(public_id)
    if result == "ok":
        print("  Media berhasil dihapus dari Cloudinary.")
        return True
    if result == "not found":
        print("  Media tidak ditemukan di Cloudinary (mungkin sudah terhapus).")
        return True
    print("  Gagal menghapus media.")
    return False


# ============================================================
# PROJECT & COLLECTION API FUNCTIONS
# ============================================================

def test_create_project(token: str, media_public_id: str = "", media_url: str = "") -> str | None:
    print("\n== [H] CREATE Project Showcase ==")
    req = project_pb2.CreateProjectRequest()
    req.input.title = "Priemman Portfolio Backend"
    req.input.description = "High-performance C++ microservice menggunakan userver."
    req.input.tools.extend(["C++", "Userver", "MariaDB", "Protobuf"])
    req.input.disciplines.extend(["Backend Engineering", "System Design"])
    req.input.tags.extend(["cpp", "async", "showcase"])
    req.input.visibility = project_pb2.PROJECT_VISIBILITY_PUBLIC
    req.input.status = project_pb2.PROJECT_STATUS_PUBLISHED

    media = req.input.media.add()
    if media_url:
        media.url = media_url
    else:
        media.url = "https://images.unsplash.com/photo-1555066931-4365d14bab8c"
    media.type = project_pb2.MEDIA_TYPE_IMAGE
    media.order = 1
    if media_public_id:
        media.public_id = media_public_id

    resp = http.post(CREATE_PROJECT_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = project_pb2.ProjectResponse()
        out.ParseFromString(resp.content)
        p = out.project
        show(resp.status_code,
             f"id          : {p.id.value}\n"
             f"slug        : {p.slug}\n"
             f"title       : {p.title}\n"
             f"status      : {p.status}\n"
             f"visibility  : {p.visibility}\n"
             f"media_count : {len(p.media)}")
        if len(p.media) > 0:
            print(f"  Media public_id: {p.media[0].public_id}")
        return p.id.value

    show_error(resp)
    return None


def test_list_projects(token: str) -> list[str]:
    print("\n== [I] LIST Projects (My Showcases) ==")
    resp = http.get(LIST_PROJECTS_URL, headers=auth_headers(token))
    project_ids = []

    if resp.status_code == 200 and is_proto(resp):
        out = project_pb2.ListProjectsResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code, f"Total projects: {len(out.projects)}")
        for i, p in enumerate(out.projects, 1):
            print(f"  [{i}] {p.id.value} | {p.title} ({p.status})")
            project_ids.append(p.id.value)
    else:
        show_error(resp)

    return project_ids


def test_update_project(token: str, project_id: str, media_public_id: str = "", media_url: str = "") -> bool:
    print(f"\n== [J] UPDATE Project (ID: {project_id}) ==")
    req = project_pb2.UpdateProjectRequest()
    req.id.value = project_id
    req.input.title = "Priemman Portfolio Backend (Updated)"
    req.input.description = "Updated description with more features."
    req.input.tools.extend(["C++20", "Userver", "Docker"])
    req.input.visibility = project_pb2.PROJECT_VISIBILITY_PUBLIC
    req.input.status = project_pb2.PROJECT_STATUS_PUBLISHED

    if media_url or media_public_id:
        media = req.input.media.add()
        media.url = media_url or "https://images.unsplash.com/photo-1555066931-4365d14bab8c"
        media.type = project_pb2.MEDIA_TYPE_IMAGE
        media.order = 1
        if media_public_id:
            media.public_id = media_public_id

    url = PROJECT_DETAIL_URL.format(project_id)
    print(f"  URL: {url}")
    resp = http.put(url, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = project_pb2.ProjectResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code, f"New Title: {out.project.title}")
        return True

    show_error(resp)
    return False


def test_delete_project(token: str, project_id: str) -> bool:
    print(f"\n== [K] DELETE Project (ID: {project_id}) ==")
    url = PROJECT_DETAIL_URL.format(project_id)
    print(f"  URL: {url}")
    resp = http.delete(url, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = common_pb2.DeleteResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code, f"success: {out.success}")
        return out.success

    show_error(resp)
    return False


def test_create_collection(token: str, project_id: str) -> str | None:
    print("\n== [L] CREATE Collection ==")
    req = project_pb2.CreateCollectionRequest()
    req.input.title = "My Best C++ Works"
    req.input.description = "Kumpulan project backend terbaik."
    req.input.visibility = project_pb2.COLLECTION_VISIBILITY_PUBLIC
    if project_id:
        req.input.project_ids.add().value = project_id

    resp = http.post(COLLECTIONS_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = project_pb2.CollectionResponse()
        out.ParseFromString(resp.content)
        c = out.collection
        show(resp.status_code, f"id: {c.id.value} | title: {c.title}")
        return c.id.value

    show_error(resp)
    return None


def test_delete_collection(token: str, collection_id: str) -> bool:
    print(f"\n== [M] DELETE Collection (ID: {collection_id}) ==")
    req = project_pb2.DeleteCollectionRequest()
    req.id.value = collection_id

    resp = http.delete(COLLECTIONS_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = common_pb2.DeleteResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code, f"success: {out.success}")
        return out.success

    show_error(resp)
    return False


# ============================================================
# CREATOR UPGRADE & ADMIN FUNCTIONS
# ============================================================

def get_upgrade_status(token: str) -> user_pb2.UpgradeStatus | None:
    print("\n== GET /v1/users/me/upgrade (status) ==")
    resp = http.get(UPGRADE_URL, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        st = user_pb2.UpgradeStatus()
        st.ParseFromString(resp.content)
        show(resp.status_code,
             f"status          : {st.status}\n"
             f"request_id      : {st.request_id.value or '-'}\n"
             f"invoice_id      : {st.invoice_id or '-'}\n"
             f"invoice_amount  : {st.invoice_amount or '-'} {st.currency}\n"
             f"rejection_reason: {st.rejection_reason or '-'}\n"
             f"requested_at    : {fmt_ts(st.requested_at)}\n"
             f"reviewed_at     : {fmt_ts(st.reviewed_at)}\n"
             f"paid_at         : {fmt_ts(st.paid_at)}")
        return st

    show_error(resp)
    return None


def request_upgrade(token: str) -> bool:
    print("\n== POST /v1/users/me/upgrade (ajukan upgrade) ==")
    resp = http.post(UPGRADE_URL, data=b"", headers=auth_headers(token))

    if resp.status_code == 201 and is_proto(resp):
        out = user_pb2.CreateUpgradeRequestResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code,
             f"success   : {out.success}\n"
             f"status    : {out.status.status}\n"
             f"request_id: {out.status.request_id.value}")
        return out.success

    show_error(resp)
    return False


def admin_list_users(token: str, limit: int = 20, offset: int = 0, role: str = "") -> user_pb2.AdminListUsersResponse | None:
    print(f"\n== GET /v1/admin/users (limit={limit}, offset={offset}, role={role or 'semua'}) ==")
    params = {"limit": limit, "offset": offset}
    if role:
        params["role"] = role
    resp = http.get(ADMIN_USERS_URL, params=params, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = user_pb2.AdminListUsersResponse()
        out.ParseFromString(resp.content)
        lines = [f"total : {out.total} | limit: {out.limit} | offset: {out.offset}"]
        for u in out.users:
            lines.append(
                f"  {u.id.value} | {u.email} | {u.first_name} {u.last_name}".rstrip()
                + f" | role={ROLE_NAMES.get(u.role, u.role)} | created={fmt_ts(u.created_at)}")
        show(resp.status_code, "\n".join(lines))
        return out

    show_error(resp)
    return None


def admin_list_upgrades(token: str, status: str = "pending") -> list:
    print(f"\n== GET /v1/admin/upgrades?status={status} ==")
    resp = http.get(ADMIN_UPGRADES_URL, params={"status": status}, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = user_pb2.AdminListUpgradeRequestsResponse()
        out.ParseFromString(resp.content)
        lines = [f"jumlah request: {len(out.requests)}"]
        for r in out.requests:
            lines.append(
                f"  id={r.id.value} | user={r.user_id.value} | {r.email}\n"
                f"    status={r.status} | invoice={r.invoice_id or '-'} "
                f"{r.invoice_amount or ''} {r.currency} | requested={fmt_ts(r.requested_at)}")
        show(resp.status_code, "\n".join(lines))
        return list(out.requests)

    show_error(resp)
    return []


def admin_review_upgrade(token: str, request_id: str, approve: bool, reason: str = "") -> bool:
    print(f"\n== POST /v1/admin/upgrades/review ({'APPROVE' if approve else 'REJECT'}) ==")
    req = user_pb2.AdminReviewUpgradeRequest()
    req.id.value = request_id
    req.approve = approve
    req.rejection_reason = reason

    resp = http.post(ADMIN_UPGRADES_REVIEW_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = user_pb2.AdminReviewUpgradeResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code,
             f"success   : {out.success}\n"
             f"status    : {out.request.status}\n"
             f"invoice_id: {out.request.invoice_id or '-'}\n"
             f"amount    : {out.request.invoice_amount or '-'} {out.request.currency}")
        return out.success

    show_error(resp)
    return False


def admin_confirm_payment(token: str, request_id: str) -> bool:
    print("\n== POST /v1/admin/upgrades/confirm-payment ==")
    req = user_pb2.AdminConfirmPaymentRequest()
    req.id.value = request_id

    resp = http.post(ADMIN_UPGRADES_CONFIRM_URL, data=req.SerializeToString(), headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = user_pb2.AdminConfirmPaymentResponse()
        out.ParseFromString(resp.content)
        show(resp.status_code,
             f"success: {out.success}\n"
             f"user_id: {out.user_id.value}")
        return out.success

    show_error(resp)
    return False


def test_throttle(token: str, count: int = 80) -> None:
    print(f"\n== UJI THROTTLE: {count}x GET /v1/users/me secara cepat ==")
    print("(limit server: 60 request/IP/menit; sisanya harus kena 429)")

    ok = 0
    limited = 0
    other = 0
    first_retry_after = None

    for _ in range(count):
        resp = http.get(GET_PROFILE_URL, headers=auth_headers(token))
        if resp.status_code == 200:
            ok += 1
        elif resp.status_code == 429:
            limited += 1
            if first_retry_after is None:
                first_retry_after = resp.headers.get("Retry-After")
        else:
            other += 1

    print(f"-> hasil: {ok} OK (200) | {limited} kena 429 | {other} status lain")
    if first_retry_after is not None:
        print(f"-> header Retry-After pertama: {first_retry_after}")
    print("-" * 60)


# ============================================================
# SECURITY TEST: EMAIL SUSPEND & IP BLOCK
# ============================================================

def send_otp_status(email: str) -> tuple[int, str]:
    """Kirim OTP, return (http_status, message) tanpa asumsi sukses."""
    req = auth_pb2.SendOtpRequest()
    req.email = email

    resp = http.post(SEND_OTP_URL, data=req.SerializeToString(), headers=PROTO_HEADERS)

    if is_proto(resp):
        out = auth_pb2.SendOtpResponse()
        out.ParseFromString(resp.content)
        return resp.status_code, out.message
    return resp.status_code, resp.text.strip()


def verify_otp_error(email: str, otp: str) -> tuple[int, str]:
    """Verifikasi OTP, return (http_status, error_code). error_code kosong = sukses."""
    req = auth_pb2.VerifyOtpRequest()
    req.email = email
    req.otp = otp

    resp = http.post(VERIFY_OTP_URL, data=req.SerializeToString(), headers=PROTO_HEADERS)

    if resp.status_code == 200 and is_proto(resp):
        return 200, ""
    try:
        return resp.status_code, resp.json().get("error", resp.text)
    except ValueError:
        return resp.status_code, resp.text.strip()


def check_result(step: str, expected, actual) -> bool:
    ok = actual == expected
    label = "OK  " if ok else "FAIL"
    print(f"  [{label}] {step} | expected={expected!r} actual={actual!r}")
    return ok


def check_verify(step: str, expected_err: str, status: int, err: str) -> bool:
    """Sama seperti check_result, tapi selalu tampilkan HTTP status supaya
    mudah membedakan 500 (body kosong) dari 401 (JSON error)."""
    ok = err == expected_err
    label = "OK  " if ok else "FAIL"
    detail = f"http={status}" + (f" error={err!r}" if err else "")
    print(f"  [{label}] {step} | expected={expected_err!r} actual={detail}")
    return ok


def test_email_suspend_and_ip_block() -> None:
    print("\n" + "=" * 60)
    print("UJI KEAMANAN: EMAIL SUSPEND & BLOKIR IP")
    print("=" * 60)
    print("""
Aturan backend:
  - 3x OTP salah untuk satu email  -> email dikunci 5 menit (email_suspended).
  - Total >=5 kegagalan dari IP yang sama dalam 15 menit
                                    -> IP diblokir (ip_suspended).
  - Selama ada kunci (email/IP), send-otp ditolak dengan HTTP 429.

Catatan:
  - Butuh 2 email berbeda, dan OTP asli tidak perlu diketahui
    (semua verifikasi sengaja memakai kode salah).
  - Jangan jalankan ulang dalam 15 menit: IP masih terblokir
    dan hasil test bisa melenceng.
""")

    email1 = input("Email #1 (target suspend email): ").strip().lower()
    email2 = input("Email #2 (target akumulasi blokir IP): ").strip().lower()
    if not email1 or not email2:
        print("Kedua email harus diisi.")
        return
    if email1 == email2:
        print("Email #1 dan #2 harus berbeda.")
        return

    wrong_otps = ["000000", "000001", "000002"]
    results: list[bool] = []

    print("\n-- Step 1: send-otp email #1 (harus 200) --")
    status, msg = send_otp_status(email1)
    results.append(check_result("send-otp email#1", 200, status))
    print(f"         message: {msg}")

    print("\n-- Step 2: verify email #1 dengan OTP salah 2x (invalid_otp_code) --")
    for code in wrong_otps[:2]:
        status, err = verify_otp_error(email1, code)
        results.append(check_verify(f"verify email#1 otp={code}", "invalid_otp_code", status, err))

    print("\n-- Step 3: send-otp email #2 (harus 200; wajib sebelum email #1 terkunci,")
    print("           karena lock send-otp dicek per email ATAU per IP) --")
    status, msg = send_otp_status(email2)
    results.append(check_result("send-otp email#2", 200, status))
    print(f"         message: {msg}")

    print("\n-- Step 4: verify email #2 dengan OTP salah 2x (invalid_otp_code) --")
    for code in wrong_otps[:2]:
        status, err = verify_otp_error(email2, code)
        results.append(check_verify(f"verify email#2 otp={code}", "invalid_otp_code", status, err))

    print("\n-- Step 5: OTP salah ke-3 untuk email #1 -> email #1 suspend 5 menit --")
    status, err = verify_otp_error(email1, wrong_otps[2])
    results.append(check_verify("verify email#1 (percobaan ke-3)", "email_suspended", status, err))

    print("\n-- Step 6: verify email #2 lagi -> IP diblokir (total gagal = 5 >= 5),")
    print("           cek IP dilakukan sebelum cek challenge --")
    status, err = verify_otp_error(email2, wrong_otps[2])
    results.append(check_verify("verify email#2 (blokir IP)", "ip_suspended", status, err))

    # Tunggu > cooldown 30 detik supaya 429 di bawah terbukti karena LOCK,
    # bukan karena cooldown pengiriman OTP.
    print("\n-- Menunggu 31 detik agar cooldown send-otp habis,")
    print("   sehingga 429 berikutnya murni karena kunci suspend --")
    time.sleep(31)

    print("\n-- Step 7: send-otp ulang email #1 (harus 429, email terkunci) --")
    status, msg = send_otp_status(email1)
    results.append(check_result("send-otp email#1 saat locked", 429, status))
    print(f"         message: {msg}")

    print("\n-- Step 8: send-otp email #2 (harus 429, IP punya record terkunci) --")
    status, msg = send_otp_status(email2)
    results.append(check_result("send-otp email#2 saat IP locked", 429, status))
    print(f"         message: {msg}")

    print("\n" + "-" * 60)
    print(f"Hasil: {sum(results)}/{len(results)} step sesuai ekspektasi")
    print("Suspend email berlaku 5 menit; blokir IP berbasis kegagalan")
    print("15 menit terakhir. Tunggu masa itu habis sebelum test ulang.")


# ============================================================
# MAIN FLOW
# ============================================================

def main() -> None:
    print("=" * 60)
    print("PRIEMMAN API TEST CLIENT")
    print("=" * 60)

    # 1. Cek session yang tersimpan
    token = load_token()
    if token:
        print(f"\nSesi tersimpan ditemukan: {token[:12]}...{token[-6:]}")
        choice = input("Pakai sesi ini? [Y/n]: ").strip().lower()
        if choice == "n":
            clear_token()
            token = None

    # 2. Kalau belum ada token, minta login
    if token is None:
        token = interactive_login()
        if token is None:
            sys.exit(1)
        save_token(token)

    # 3. Verifikasi token masih valid
    user = get_profile(token)
    if user is None:
        print("\nToken sudah invalid / expired. Login ulang...")
        clear_token()
        token = interactive_login()
        if token is None:
            sys.exit(1)
        save_token(token)
        user = get_profile(token)
        if user is None:
            print("Login gagal, berhenti.")
            sys.exit(1)

    # 4. Menu test
    print("\n" + "=" * 60)
    print("MENU TEST")
    print("=" * 60)

    # Store uploaded media info
    uploaded_media = {
        "public_id": "",
        "url": "",
    }

    while True:
        print("\nApa yang ingin kamu test?")
        print("  1. User API (Profile, Work Experience, Connected Accounts)")
        print("  2. Projects & Collections API")
        print("  3. Media Upload (Cloudinary)")
        print("  4. Logout & Exit")
        print("  5. Delete Media (Cloudinary, manual public_id)")
        print("  6. Uji Keamanan: Email Suspend & Blokir IP")
        print("  7. Upgrade ke Creator")
        print("  8. Admin: List Users & Upgrade Requests")
        print("  9. Uji Throttle (Rate Limiter)")
        print("  0. Exit (tanpa logout)")

        choice = input("\nPilih [0-9]: ").strip()

        if choice == "1":
            print("\n" + "=" * 60)
            print("TESTING USER API ENDPOINTS")
            print("=" * 60)

            sub_choice = input("\nUpdate profile? [y/N]: ").strip().lower()
            if sub_choice == "y":
                if update_profile(token):
                    get_profile(token)

            work_ids = list_work_experiences(token)

            sub_choice = input("\nCreate new work experience? [y/N]: ").strip().lower()
            new_work_id = None
            if sub_choice == "y":
                new_work_id = upsert_work_experience(token)

            if new_work_id:
                sub_choice = input(f"\nDelete the new work experience ({new_work_id})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    delete_work_experience(token, new_work_id)
                    list_work_experiences(token)
            elif work_ids:
                sub_choice = input(f"\nDelete first work experience ({work_ids[0]})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    delete_work_experience(token, work_ids[0])
                    list_work_experiences(token)

            list_connected_accounts(token)

            sub_choice = input("\nDelete GitHub connected account? [y/N]: ").strip().lower()
            if sub_choice == "y":
                delete_connected_account(token)
                list_connected_accounts(token)

        elif choice == "2":
            print("\n" + "=" * 60)
            print("TESTING PROJECTS & COLLECTIONS API")
            print("=" * 60)

            # Gunakan media dari upload sebelumnya jika ada
            media_public_id = uploaded_media.get("public_id", "")
            media_url = uploaded_media.get("url", "")

            if media_public_id:
                print(f"\n  Menggunakan media yang diupload sebelumnya:")
                print(f"    public_id: {media_public_id}")
                print(f"    url: {media_url}")

            sub_choice = input("\nTest Create Project? [y/N]: ").strip().lower()
            new_project_id = None
            if sub_choice == "y":
                new_project_id = test_create_project(token, media_public_id, media_url)

            test_list_projects(token)

            if new_project_id:
                sub_choice = input(f"\nUpdate the new project ({new_project_id})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    test_update_project(token, new_project_id, media_public_id, media_url)

            sub_choice = input("\nTest Create Collection? [y/N]: ").strip().lower()
            new_collection_id = None
            if sub_choice == "y":
                new_collection_id = test_create_collection(token, new_project_id or "")

            if new_collection_id:
                sub_choice = input(f"\nDelete the new collection ({new_collection_id})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    test_delete_collection(token, new_collection_id)

            if new_project_id:
                sub_choice = input(f"\nDelete the new project ({new_project_id})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    test_delete_project(token, new_project_id)

        elif choice == "3":
            print("\n" + "=" * 60)
            print("MEDIA UPLOAD (CLOUDINARY)")
            print("=" * 60)

            print("\nContoh path file:")
            print("  ~/Pictures/photo.jpg")
            print("  ~/Downloads/video.mp4")
            print("  ./test_image.png")

            file_path = input("\nMasukkan path file: ").strip()

            # Expand user home directory
            file_path = os.path.expanduser(file_path)

            if not file_path:
                print("Path kosong.")
            elif not os.path.exists(file_path):
                print(f"File tidak ditemukan: {file_path}")
            else:
                result = upload_media(token, file_path)
                if result:
                    uploaded_media["public_id"] = result.public_id
                    uploaded_media["url"] = result.url
                    print(f"\n  Media tersimpan untuk digunakan di project:")
                    print(f"    public_id: {uploaded_media['public_id']}")
                    print(f"    url: {uploaded_media['url']}")

                    sub_choice = input("\nLangsung hapus media ini dari Cloudinary? [y/N]: ").strip().lower()
                    if sub_choice == "y":
                        if delete_uploaded_media(result.public_id):
                            uploaded_media["public_id"] = ""
                            uploaded_media["url"] = ""

        elif choice == "5":
            print("\n" + "=" * 60)
            print("DELETE MEDIA (CLOUDINARY)")
            print("=" * 60)

            if uploaded_media["public_id"]:
                print(f"\n  Media terakhir yang diupload: {uploaded_media['public_id']}")

            pid = input("Masukkan public_id (kosong = pakai media terakhir): ").strip()
            if not pid:
                pid = uploaded_media["public_id"]

            if delete_uploaded_media(pid) and pid == uploaded_media["public_id"]:
                uploaded_media["public_id"] = ""
                uploaded_media["url"] = ""
                print("  (media tersimpan untuk project ikut dibersihkan)")

        elif choice == "6":
            test_email_suspend_and_ip_block()

        elif choice == "7":
            print("\n" + "=" * 60)
            print("UPGRADE KE CREATOR")
            print("=" * 60)

            get_upgrade_status(token)

            sub_choice = input("\nAjukan upgrade (POST)? [y/N]: ").strip().lower()
            if sub_choice == "y":
                request_upgrade(token)
                get_upgrade_status(token)

        elif choice == "8":
            print("\n" + "=" * 60)
            print("ADMIN: LIST USERS & UPGRADE REQUESTS")
            print("=" * 60)
            print("(harus login sebagai admin; user biasa akan kena 403)")

            admin_list_users(token)

            sub_choice = input("\nFilter role (user/creator/admin, kosong = lewati): ").strip().lower()
            if sub_choice:
                admin_list_users(token, role=sub_choice)

            entries = admin_list_upgrades(token, "pending")

            if entries:
                sub_choice = input("\nReview salah satu request? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    rid = input("Masukkan request_id: ").strip()
                    verdict = input("Approve? [Y/n]: ").strip().lower()
                    approve = verdict != "n"
                    reason = ""
                    if not approve:
                        reason = input("Alasan reject: ").strip()
                    if rid and admin_review_upgrade(token, rid, approve, reason):
                        if approve:
                            pay = input("Konfirmasi pembayaran (naikkan role ke creator)? [y/N]: ").strip().lower()
                            if pay == "y":
                                admin_confirm_payment(token, rid)

            sub_choice = input("\nKonfirmasi pembayaran request lain (masukkan request_id, kosong = lewati): ").strip()
            if sub_choice:
                admin_confirm_payment(token, sub_choice)

        elif choice == "9":
            print("\n" + "=" * 60)
            print("UJI THROTTLE (RATE LIMITER)")
            print("=" * 60)

            n_raw = input("Jumlah request (default 80): ").strip()
            try:
                count = int(n_raw) if n_raw else 80
            except ValueError:
                count = 80
            test_throttle(token, count)

        elif choice == "4":
            print("\n== Logout ==")
            logout(token)
            break

        elif choice == "0":
            print("\nKeluar tanpa logout.")
            break

        else:
            print("Pilihan tidak valid.")


if __name__ == "__main__":
    main()
