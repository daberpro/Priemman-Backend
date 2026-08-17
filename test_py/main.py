"""
Client test HTTP + Protobuf untuk Priemman backend.

Fitur:
  - Token disimpan otomatis di `.session` supaya tidak perlu OTP setiap run.
  - Kalau token invalid (401), auto-clear file dan minta login ulang sekali.
  - Logout otomatis menghapus `.session`.
  - Menu interaktif untuk test semua endpoint.

Jalankan:  python test_py/main.py
"""

import os
import sys
import requests

import auth_pb2
import user_pb2
import common_pb2
import project_pb2

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
        result = common_pb2.DeleteResponse()   # <-- dari common.proto
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
        result = common_pb2.DeleteResponse()   # <-- dari common.proto
        result.ParseFromString(resp.content)
        show(resp.status_code, f"success: {result.success}")
        return result.success

    show_error(resp)
    return False


# ============================================================
# PROJECT & COLLECTION API FUNCTIONS
# ============================================================

def test_create_project(token: str) -> str | None:
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
    media.url = "https://images.unsplash.com/photo-1555066931-4365d14bab8c"
    media.type = project_pb2.MEDIA_TYPE_IMAGE
    media.order = 1

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
             f"visibility  : {p.visibility}")
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


def test_update_project(token: str, project_id: str) -> bool:
    print(f"\n== [J] UPDATE Project (ID: {project_id}) ==")
    req = project_pb2.UpdateProjectRequest()
    req.id.value = project_id
    req.input.title = "Priemman Portfolio Backend (Updated)"
    req.input.description = "Updated description with more features."
    req.input.tools.extend(["C++20", "Userver", "Docker"])
    req.input.visibility = project_pb2.PROJECT_VISIBILITY_PUBLIC
    req.input.status = project_pb2.PROJECT_STATUS_PUBLISHED

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
    # DELETE project tidak butuh body (ID ada di path parameter)
    resp = http.delete(url, headers=auth_headers(token))

    if resp.status_code == 200 and is_proto(resp):
        out = common_pb2.DeleteResponse()   # <-- dari common.proto
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
        out = common_pb2.DeleteResponse()   # <-- dari common.proto
        out.ParseFromString(resp.content)
        show(resp.status_code, f"success: {out.success}")
        return out.success

    show_error(resp)
    return False


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

    while True:
        print("\nApa yang ingin kamu test?")
        print("  1. User API (Profile, Work Experience, Connected Accounts)")
        print("  2. Projects & Collections API")
        print("  3. Logout & Exit")
        print("  0. Exit (tanpa logout)")

        choice = input("\nPilih [0-3]: ").strip()

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

            sub_choice = input("\nTest Create Project? [y/N]: ").strip().lower()
            new_project_id = None
            if sub_choice == "y":
                new_project_id = test_create_project(token)

            test_list_projects(token)

            if new_project_id:
                sub_choice = input(f"\nUpdate the new project ({new_project_id})? [y/N]: ").strip().lower()
                if sub_choice == "y":
                    test_update_project(token, new_project_id)

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
