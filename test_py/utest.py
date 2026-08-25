"""
Integration test suite untuk Priemman backend (userver + Protobuf).

Menjalankan test terhadap server YANG SEDANG BERJALAN + database MariaDB.
OTP di-crack dari hash di tabel otp_challenges (SHA-256, 6 digit).

Environment variables (opsional, ada default):
    PRIEMMAN_BASE_URL      default http://localhost:8080
    PRIEMMAN_DB_HOST       default 127.0.0.1
    PRIEMMAN_DB_PORT       default 3306
    PRIEMMAN_DB_USER       default root
    PRIEMMAN_DB_PASSWORD   default ""
    PRIEMMAN_DB_NAME       default db_priemman
    PRIEMMAN_CORS_ORIGIN   default http://localhost:3000

Setup:
    pip install pytest requests pymysql

Jalankan:
    python -m pytest test_py/test_api.py -v
"""

import hashlib
import os
import time
import uuid

import pytest
import requests
from google.protobuf import timestamp_pb2

import auth_pb2
import common_pb2
import media_pb2
import project_pb2
import user_pb2

# ============================================================
# Konstanta & helper
# ============================================================

BASE_URL = os.environ.get("PRIEMMAN_BASE_URL", "http://localhost:8080")

PROTO_HEADERS = {
    "Content-Type": "application/x-protobuf",
    "Accept": "application/x-protobuf",
}


def auth_hdr(token: str) -> dict:
    return {**PROTO_HEADERS, "Authorization": f"Bearer {token}"}


def unique_email() -> str:
    return f"test-{uuid.uuid4().hex[:10]}@priemman.my.id"


def parse(cls, resp):
    msg = cls()
    msg.ParseFromString(resp.content)
    return msg


def assert_proto_ok(resp, cls):
    assert resp.status_code == 200, f"expected 200, got {resp.status_code}: {resp.text[:200]}"
    assert "protobuf" in resp.headers.get("Content-Type", ""), "response bukan protobuf"
    return parse(cls, resp)


def crack_otp(db, email: str) -> str:
    """Brute-force OTP 6 digit dari hash SHA-256 di DB."""
    with db.cursor() as cur:
        cur.execute(
            "SELECT otp_hash FROM otp_challenges "
            "WHERE email = %s AND consumed_at IS NULL "
            "ORDER BY created_at DESC LIMIT 1",
            (email,),
        )
        row = cur.fetchone()
    assert row is not None, f"tidak ada otp challenge aktif untuk {email}"
    target = row[0]
    for cand in range(100000, 1000000):
        if hashlib.sha256(str(cand).encode()).hexdigest() == target:
            return str(cand)
    raise AssertionError("OTP tidak berhasil di-crack")


def login(base_url: str, db, email: str | None = None) -> tuple[str, str]:
    """Full OTP login. Return (email, session_token)."""
    email = email or unique_email()
    req = auth_pb2.SendOtpRequest(email=email)
    r = requests.post(f"{base_url}/v1/auth/send-otp",
                      data=req.SerializeToString(), headers=PROTO_HEADERS)
    assert r.status_code == 200, f"send-otp gagal: {r.status_code} {r.text[:200]}"

    otp = crack_otp(db, email)

    vr = auth_pb2.VerifyOtpRequest(email=email, otp=otp)
    r2 = requests.post(f"{base_url}/v1/auth/verify-otp",
                       data=vr.SerializeToString(), headers=PROTO_HEADERS)
    assert r2.status_code == 200, f"verify-otp gagal: {r2.status_code} {r2.text[:200]}"
    out = parse(auth_pb2.VerifyOtpResponse, r2)
    assert out.session_token, "session_token kosong"
    return email, out.session_token


# ============================================================
# Fixtures
# ============================================================

@pytest.fixture(scope="session")
def base_url() -> str:
    return BASE_URL


@pytest.fixture(scope="session")
def db():
    pymysql = pytest.importorskip("pymysql")
    conn = pymysql.connect(
        host=os.environ.get("PRIEMMAN_DB_HOST", "127.0.0.1"),
        port=int(os.environ.get("PRIEMMAN_DB_PORT", "3306")),
        user=os.environ.get("PRIEMMAN_DB_USER", "root"),
        password=os.environ.get("PRIEMMAN_DB_PASSWORD", ""),
        database=os.environ.get("PRIEMMAN_DB_NAME", "db_priemman"),
        autocommit=True,
    )
    yield conn
    conn.close()


@pytest.fixture(scope="session")
def user(base_url, db) -> dict:
    email, token = login(base_url, db)
    return {"email": email, "token": token}


@pytest.fixture(scope="session")
def second_user(base_url, db) -> dict:
    email, token = login(base_url, db)
    return {"email": email, "token": token}


# ============================================================
# AUTH
# ============================================================

class TestAuth:
    def test_send_otp_invalid_body(self, base_url):
        r = requests.post(f"{base_url}/v1/auth/send-otp",
                          data=b"garbage-bytes", headers=PROTO_HEADERS)
        assert r.status_code == 400

    def test_send_otp_empty_email(self, base_url):
        r = requests.post(f"{base_url}/v1/auth/send-otp",
                          data=auth_pb2.SendOtpRequest().SerializeToString(),
                          headers=PROTO_HEADERS)
        assert r.status_code == 400

    def test_send_otp_success_then_cooldown(self, base_url):
        email = unique_email()
        body = auth_pb2.SendOtpRequest(email=email).SerializeToString()

        r1 = requests.post(f"{base_url}/v1/auth/send-otp", data=body, headers=PROTO_HEADERS)
        out1 = assert_proto_ok(r1, auth_pb2.SendOtpResponse)
        assert out1.success is True
        assert out1.cooldown_seconds == 30

        # langsung request lagi -> kena cooldown 429
        r2 = requests.post(f"{base_url}/v1/auth/send-otp", data=body, headers=PROTO_HEADERS)
        assert r2.status_code == 429
        out2 = parse(auth_pb2.SendOtpResponse, r2)
        assert out2.success is False

    def test_verify_otp_invalid_body(self, base_url):
        r = requests.post(f"{base_url}/v1/auth/verify-otp",
                          data=b"\x00\x01\x02", headers=PROTO_HEADERS)
        assert r.status_code == 400

    def test_verify_otp_wrong_code(self, base_url):
        email = unique_email()
        requests.post(f"{base_url}/v1/auth/send-otp",
                      data=auth_pb2.SendOtpRequest(email=email).SerializeToString(),
                      headers=PROTO_HEADERS)
        r = requests.post(f"{base_url}/v1/auth/verify-otp",
                          data=auth_pb2.VerifyOtpRequest(email=email, otp="000000").SerializeToString(),
                          headers=PROTO_HEADERS)
        assert r.status_code == 401

    def test_verify_otp_correct_and_single_use(self, base_url, db):
        email, token = login(base_url, db)  # login pertama sukses
        assert token

        # OTP yang sama tidak bisa dipakai lagi (sudah consumed)
        otp = crack_otp.__wrapped__(db, email) if hasattr(crack_otp, "__wrapped__") else None
        with db.cursor() as cur:
            cur.execute(
                "SELECT COUNT(*) FROM otp_challenges WHERE email = %s AND consumed_at IS NULL",
                (email,))
            assert cur.fetchone()[0] == 0, "challenge seharusnya sudah consumed"

    def test_me_requires_auth(self, base_url):
        r = requests.get(f"{base_url}/v1/users/me", headers=PROTO_HEADERS)
        assert r.status_code == 401

        r2 = requests.get(f"{base_url}/v1/users/me",
                          headers=auth_hdr("token-palsu-123"))
        assert r2.status_code == 401

    def test_logout_invalidates_session(self, base_url, db):
        email, token = login(base_url, db)

        r = requests.get(f"{base_url}/v1/users/me", headers=auth_hdr(token))
        assert r.status_code == 200

        lr = requests.post(f"{base_url}/v1/auth/logout",
                           data=auth_pb2.LogoutRequest(session_token=token).SerializeToString(),
                           headers=PROTO_HEADERS)
        out = assert_proto_ok(lr, auth_pb2.LogoutResponse)
        assert out.success is True

        r2 = requests.get(f"{base_url}/v1/users/me", headers=auth_hdr(token))
        assert r2.status_code == 401, "token harusnya tidak valid setelah logout"


# ============================================================
# USER API
# ============================================================

class TestUserApi:
    def test_get_me(self, base_url, user):
        r = requests.get(f"{base_url}/v1/users/me", headers=auth_hdr(user["token"]))
        me = assert_proto_ok(r, user_pb2.User)
        assert me.email == user["email"]
        assert me.id.value

    def test_update_basic_info(self, base_url, user):
        req = user_pb2.UpdateBasicInfoRequest(
            first_name="Test",
            last_name="User",
            headline="QA Engineer",
            company="Priemman",
            location=common_pb2.Location(country="Indonesia", city="Bandung"),
            website_url="https://test.priemman.my.id",
        )
        r = requests.put(f"{base_url}/v1/users/me",
                         data=req.SerializeToString(), headers=auth_hdr(user["token"]))
        me = assert_proto_ok(r, user_pb2.User)
        assert me.first_name == "Test"
        assert me.location.city == "Bandung"

        # pastikan persist
        r2 = requests.get(f"{base_url}/v1/users/me", headers=auth_hdr(user["token"]))
        me2 = assert_proto_ok(r2, user_pb2.User)
        assert me2.first_name == "Test"

    def test_work_experience_crud_and_authz(self, base_url, user, second_user):
        h = auth_hdr(user["token"])

        # create
        req = user_pb2.UpsertWorkExperienceRequest()
        req.entry.title = "Backend Dev"
        req.entry.company = "PT Test"
        req.entry.is_current = True
        req.entry.start_date.seconds = 1672531200
        req.entry.description = "testing"
        r = requests.post(f"{base_url}/v1/users/me/work-experiences",
                          data=req.SerializeToString(), headers=h)
        wx = assert_proto_ok(r, user_pb2.WorkExperience)
        assert wx.id.value
        assert wx.title == "Backend Dev"

        # list contains it
        r = requests.get(f"{base_url}/v1/users/me/work-experiences", headers=h)
        lst = assert_proto_ok(r, user_pb2.ListWorkExperienceResponse)
        assert any(w.id.value == wx.id.value for w in lst.entries)

        # update
        req2 = user_pb2.UpsertWorkExperienceRequest()
        req2.entry.id.value = wx.id.value
        req2.entry.title = "Senior Backend Dev"
        req2.entry.company = "PT Test"
        req2.entry.is_current = False
        req2.entry.start_date.seconds = 1672531200
        req2.entry.end_date.seconds = 1700000000
        req2.entry.description = "updated"
        r = requests.post(f"{base_url}/v1/users/me/work-experiences",
                          data=req2.SerializeToString(), headers=h)
        wx2 = assert_proto_ok(r, user_pb2.WorkExperience)
        assert wx2.title == "Senior Backend Dev"
        assert wx2.is_current is False

        # user lain tidak boleh delete
        r = requests.delete(f"{base_url}/v1/users/me/work-experiences",
                            data=auth_pb2_wx_delete(wx.id.value),
                            headers=auth_hdr(second_user["token"]))
        assert r.status_code == 404

        # owner delete
        r = requests.delete(f"{base_url}/v1/users/me/work-experiences",
                            data=auth_pb2_wx_delete(wx.id.value), headers=h)
        out = assert_proto_ok(r, common_pb2.DeleteResponse)
        assert out.success is True

        # delete lagi -> 404
        r = requests.delete(f"{base_url}/v1/users/me/work-experiences",
                            data=auth_pb2_wx_delete(wx.id.value), headers=h)
        assert r.status_code == 404

    def test_connected_accounts(self, base_url, user):
        h = auth_hdr(user["token"])
        r = requests.get(f"{base_url}/v1/users/me/connected-accounts", headers=h)
        assert_proto_ok(r, user_pb2.ListConnectedAccountsResponse)

        # user baru belum punya connected account -> delete = 404
        req = user_pb2.DeleteConnectedAccountRequest(
            platform=user_pb2.CONNECTED_PLATFORM_GITHUB)
        r = requests.delete(f"{base_url}/v1/users/me/connected-accounts",
                            data=req.SerializeToString(), headers=h)
        assert r.status_code == 404

    def test_user_endpoints_require_auth(self, base_url):
        assert requests.get(f"{base_url}/v1/users/me/work-experiences").status_code == 401
        assert requests.post(f"{base_url}/v1/users/me/work-experiences").status_code == 401
        assert requests.get(f"{base_url}/v1/users/me/connected-accounts").status_code == 401
        assert requests.put(f"{base_url}/v1/users/me").status_code == 401


# ============================================================
# PROJECTS
# ============================================================

def make_project(token: str, title: str,
                 status=project_pb2.PROJECT_STATUS_DRAFT,
                 visibility=project_pb2.PROJECT_VISIBILITY_PUBLIC):
    req = project_pb2.CreateProjectRequest()
    req.input.title = title
    req.input.description = "desc"
    req.input.tools.extend(["C++"])
    req.input.status = status
    req.input.visibility = visibility
    m = req.input.media.add()
    m.url = "https://example.com/img.png"
    m.type = project_pb2.MEDIA_TYPE_IMAGE
    m.order = 1
    r = requests.post(f"{BASE_URL}/v1/projects",
                      data=req.SerializeToString(), headers=auth_hdr(token))
    return r


class TestProjects:
    def test_create_requires_auth(self, base_url):
        r = requests.post(f"{base_url}/v1/projects",
                          data=project_pb2.CreateProjectRequest().SerializeToString(),
                          headers=PROTO_HEADERS)
        assert r.status_code == 401

    def test_create_empty_title(self, base_url, user):
        r = requests.post(f"{base_url}/v1/projects",
                          data=project_pb2.CreateProjectRequest().SerializeToString(),
                          headers=auth_hdr(user["token"]))
        assert r.status_code == 400

    def test_create_and_get_detail(self, base_url, user):
        r = make_project(user["token"], "Project Alpha")
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project
        assert proj.slug
        assert proj.status == project_pb2.PROJECT_STATUS_DRAFT

        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}",
                         headers=auth_hdr(user["token"]))
        got = assert_proto_ok(r, project_pb2.ProjectResponse).project
        assert got.title == "Project Alpha"
        assert len(got.media) == 1

    def test_slug_unique(self, base_url, user):
        r1 = make_project(user["token"], "Same Title")
        r2 = make_project(user["token"], "Same Title")
        p1 = assert_proto_ok(r1, project_pb2.ProjectResponse).project
        p2 = assert_proto_ok(r2, project_pb2.ProjectResponse).project
        assert p1.slug != p2.slug

    def test_draft_hidden_from_others_and_public(self, base_url, user, second_user):
        r = make_project(user["token"], "Secret Draft")
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

        # user lain -> 404
        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}",
                         headers=auth_hdr(second_user["token"]))
        assert r.status_code == 404

        # publik (tanpa token) -> 404
        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}")
        assert r.status_code == 404

    def test_published_visible_to_public_and_views(self, base_url, user, second_user):
        r = make_project(user["token"], "Public Show",
                         status=project_pb2.PROJECT_STATUS_PUBLISHED,
                         visibility=project_pb2.PROJECT_VISIBILITY_PUBLIC)
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

        # publik bisa lihat
        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}")
        got = assert_proto_ok(r, project_pb2.ProjectResponse).project
        assert got.title == "Public Show"

        # view bertambah setelah dilihat publik (bukan owner)
        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}")
        got2 = assert_proto_ok(r, project_pb2.ProjectResponse).project
        assert got2.metrics.views == got.metrics.views + 1

        # muncul di feed publik
        r = requests.get(f"{base_url}/v1/projects/list")
        feed = assert_proto_ok(r, project_pb2.ListProjectsResponse)
        assert any(p.id.value == proj.id.value for p in feed.projects)

    def test_update_project_and_authz(self, base_url, user, second_user):
        r = make_project(user["token"], "Before Update")
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

        # user lain tidak bisa update
        req = project_pb2.UpdateProjectRequest()
        req.id.value = proj.id.value
        req.input.title = "Hacked"
        r = requests.put(f"{base_url}/v1/projects/{proj.id.value}",
                         data=req.SerializeToString(),
                         headers=auth_hdr(second_user["token"]))
        assert r.status_code == 404

        # owner update
        req.input.title = "After Update"
        r = requests.put(f"{base_url}/v1/projects/{proj.id.value}",
                         data=req.SerializeToString(), headers=auth_hdr(user["token"]))
        got = assert_proto_ok(r, project_pb2.ProjectResponse).project
        assert got.title == "After Update"

    def test_list_own_with_status_filter(self, base_url, user):
        make_project(user["token"], "Filter Draft")
        make_project(user["token"], "Filter Pub",
                     status=project_pb2.PROJECT_STATUS_PUBLISHED)

        r = requests.get(f"{base_url}/v1/projects/list?status=PUBLISHED",
                         headers=auth_hdr(user["token"]))
        lst = assert_proto_ok(r, project_pb2.ListProjectsResponse)
        assert all(p.status == project_pb2.PROJECT_STATUS_PUBLISHED for p in lst.projects)

        r = requests.get(f"{base_url}/v1/projects/list?status=DRAFT",
                         headers=auth_hdr(user["token"]))
        lst2 = assert_proto_ok(r, project_pb2.ListProjectsResponse)
        assert all(p.status == project_pb2.PROJECT_STATUS_DRAFT for p in lst2.projects)

    def test_delete_project_and_authz(self, base_url, user, second_user):
        r = make_project(user["token"], "To Delete")
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

        r = requests.delete(f"{base_url}/v1/projects/{proj.id.value}",
                            headers=auth_hdr(second_user["token"]))
        assert r.status_code == 404

        r = requests.delete(f"{base_url}/v1/projects/{proj.id.value}",
                            headers=auth_hdr(user["token"]))
        out = assert_proto_ok(r, common_pb2.DeleteResponse)
        assert out.success is True

        r = requests.get(f"{base_url}/v1/projects/{proj.id.value}",
                         headers=auth_hdr(user["token"]))
        assert r.status_code == 404

    def test_get_unknown_project(self, base_url, user):
        r = requests.get(f"{base_url}/v1/projects/{uuid.uuid4().hex}",
                         headers=auth_hdr(user["token"]))
        assert r.status_code == 404


# ============================================================
# MEDIA (CLOUDINARY)
# ============================================================

MEDIA_TEST_IMAGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test.png")


def cloudinary_creds() -> tuple | None:
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


def cloudinary_destroy(public_id: str) -> str | None:
    """Signed destroy langsung ke Cloudinary (sama seperti Client::Delete backend).

    Return nilai `result` dari Cloudinary ("ok" / "not found"), atau None kalau gagal.
    """
    creds = cloudinary_creds()
    if creds is None:
        return None
    cloud_name, api_key, api_secret = creds
    ts = str(int(time.time()))
    to_sign = f"public_id={public_id}&timestamp={ts}" + api_secret
    signature = hashlib.sha1(to_sign.encode()).hexdigest()
    url = f"https://api.cloudinary.com/v1_1/{cloud_name}/image/destroy"
    resp = requests.post(url, data={
        "public_id": public_id,
        "api_key": api_key,
        "timestamp": ts,
        "signature": signature,
    }, timeout=30)
    if resp.status_code != 200:
        return None
    return resp.json().get("result")


def cloudinary_exists(public_id: str) -> bool | None:
    """Cek aset masih ada di Cloudinary via Admin API (read-only, tidak menghapus).

    Return True/False, atau None kalau kredensial tidak ada / error jaringan.
    """
    creds = cloudinary_creds()
    if creds is None:
        return None
    cloud_name, api_key, api_secret = creds
    url = (f"https://api.cloudinary.com/v1_1/{cloud_name}"
           f"/resources/image/upload/{public_id}")
    try:
        resp = requests.get(url, auth=(api_key, api_secret), timeout=15)
    except requests.RequestException:
        return None
    if resp.status_code == 200:
        return True
    if resp.status_code == 404:
        return False
    return None


def upload_test_image(base_url: str, token: str):
    """Upload test.png ke backend, return item pertama UploadMediaBatchResponse."""
    if not os.path.exists(MEDIA_TEST_IMAGE):
        pytest.skip("test.png tidak ada di test_py/")
    with open(MEDIA_TEST_IMAGE, "rb") as f:
        files = {"file": ("test.png", f.read(), "image/png")}
    # multipart: JANGAN pakai Content-Type protobuf, biarkan requests isi boundary
    r = requests.post(f"{base_url}/v1/media/upload", files=files,
                      headers={"Authorization": f"Bearer {token}"})
    assert r.status_code == 200, f"upload gagal: {r.status_code} {r.text[:300]}"
    batch = parse(media_pb2.UploadMediaBatchResponse, r)
    assert batch.items, "batch upload kosong"
    return batch.items[0]


class TestMedia:
    def test_upload_requires_auth(self, base_url):
        r = requests.post(f"{base_url}/v1/media/upload")
        assert r.status_code == 401

    def test_upload_media(self, base_url, user):
        out = upload_test_image(base_url, user["token"])
        assert out.url.startswith("https://res.cloudinary.com/")
        assert out.public_id
        assert out.id.value
        assert out.type == project_pb2.MEDIA_TYPE_IMAGE
        assert out.resource_type == "image"

        # cleanup supaya tidak menumpuk di Cloudinary
        if cloudinary_creds() is not None:
            cloudinary_destroy(out.public_id)

    def test_project_delete_returns_media_to_orphan(self, base_url, db, user):
        """Lifecycle: upload -> pakai di project -> delete project.

        Perilaku baru (grace period): delete project TIDAK langsung menghapus
        aset dari Cloudinary — media dikembalikan ke status orphan dan
        penghapusan diserahkan ke sweeper (TTL 24 jam). Di sini kita tidak
        menunggu 24 jam, jadi diverifikasi lewat status di DB + aset yang
        masih ada di Cloudinary.
        """
        if cloudinary_creds() is None:
            pytest.skip("kredensial Cloudinary tidak ada di config_vars.yaml")

        media = upload_test_image(base_url, user["token"])

        def db_media_status() -> str | None:
            with db.cursor() as cur:
                cur.execute(
                    "SELECT status FROM media_uploads WHERE public_id = %s",
                    (media.public_id,),
                )
                row = cur.fetchone()
            return row[0] if row else None

        try:
            # setelah upload, media tercatat sebagai orphan
            assert db_media_status() == "orphan", \
                "media seharusnya tercatat orphan setelah upload"

            # buat project yang memakai media tersebut
            req = project_pb2.CreateProjectRequest()
            req.input.title = "Media Lifecycle"
            req.input.description = "d"
            req.input.status = project_pb2.PROJECT_STATUS_DRAFT
            req.input.visibility = project_pb2.PROJECT_VISIBILITY_PUBLIC
            m = req.input.media.add()
            m.url = media.url
            m.type = project_pb2.MEDIA_TYPE_IMAGE
            m.order = 1
            m.public_id = media.public_id
            r = requests.post(f"{base_url}/v1/projects",
                              data=req.SerializeToString(), headers=auth_hdr(user["token"]))
            proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

            # setelah di-attach, status berubah jadi in_use
            assert db_media_status() == "in_use", \
                "media seharusnya in_use setelah dipakai project"

            # hapus project -> media kembali ke orphan, BUKAN langsung dihapus
            r = requests.delete(f"{base_url}/v1/projects/{proj.id.value}",
                                headers=auth_hdr(user["token"]))
            assert_proto_ok(r, common_pb2.DeleteResponse)
            assert db_media_status() == "orphan", \
                "media seharusnya kembali orphan setelah delete project (grace period)"

            # aset masih ada di Cloudinary — penghapusannya menunggu sweeper
            assert cloudinary_exists(media.public_id) is True, \
                "aset seharusnya masih ada di Cloudinary menunggu sweeper"
        finally:
            # cleanup manual supaya tidak menunggu sweeper 24 jam
            cloudinary_destroy(media.public_id)


# ============================================================
# COLLECTIONS
# ============================================================

class TestCollections:
    def test_create_requires_auth(self, base_url):
        r = requests.post(f"{base_url}/v1/collections",
                          data=project_pb2.CreateCollectionRequest().SerializeToString(),
                          headers=PROTO_HEADERS)
        assert r.status_code == 401

    def test_crud_and_authz(self, base_url, user, second_user):
        h = auth_hdr(user["token"])

        # buat project dulu untuk diisi ke collection
        r = make_project(user["token"], "Col Project")
        proj = assert_proto_ok(r, project_pb2.ProjectResponse).project

        # create
        req = project_pb2.CreateCollectionRequest()
        req.input.title = "My Collection"
        req.input.description = "d"
        req.input.visibility = project_pb2.COLLECTION_VISIBILITY_PRIVATE
        req.input.project_ids.add().value = proj.id.value
        r = requests.post(f"{base_url}/v1/collections",
                          data=req.SerializeToString(), headers=h)
        col = assert_proto_ok(r, project_pb2.CollectionResponse).collection
        assert col.id.value
        assert col.project_ids[0].value == proj.id.value

        # detail via ?id=
        r = requests.get(f"{base_url}/v1/collections?id={col.id.value}", headers=h)
        got = assert_proto_ok(r, project_pb2.CollectionResponse).collection
        assert got.title == "My Collection"

        # list contains
        r = requests.get(f"{base_url}/v1/collections", headers=h)
        lst = assert_proto_ok(r, project_pb2.ListCollectionsResponse)
        assert any(c.id.value == col.id.value for c in lst.collections)

        # update
        req2 = project_pb2.UpdateCollectionRequest()
        req2.id.value = col.id.value
        req2.input.title = "Renamed Collection"
        req2.input.visibility = project_pb2.COLLECTION_VISIBILITY_PRIVATE
        r = requests.put(f"{base_url}/v1/collections",
                         data=req2.SerializeToString(), headers=h)
        got2 = assert_proto_ok(r, project_pb2.CollectionResponse).collection
        assert got2.title == "Renamed Collection"

        # user lain tidak bisa delete
        req3 = project_pb2.DeleteCollectionRequest()
        req3.id.value = col.id.value
        r = requests.delete(f"{base_url}/v1/collections",
                            data=req3.SerializeToString(),
                            headers=auth_hdr(second_user["token"]))
        assert r.status_code == 404

        # owner delete
        r = requests.delete(f"{base_url}/v1/collections",
                            data=req3.SerializeToString(), headers=h)
        out = assert_proto_ok(r, common_pb2.DeleteResponse)
        assert out.success is True

        r = requests.get(f"{base_url}/v1/collections?id={col.id.value}", headers=h)
        assert r.status_code == 404


# ============================================================
# CORS & SECURITY
# ============================================================

class TestCorsAndSecurity:
    def test_preflight_allowed_origin(self, base_url):
        origin = os.environ.get("PRIEMMAN_CORS_ORIGIN", "http://localhost:3000")
        r = requests.options(
            f"{base_url}/v1/users/me",
            headers={
                "Origin": origin,
                "Access-Control-Request-Method": "POST",
                "Access-Control-Request-Headers": "content-type, authorization",
            },
        )
        if r.status_code == 405:
            pytest.skip("Method OPTIONS belum didaftarkan di config.yaml handler")
        if r.status_code == 403:
            pytest.skip(f"origin {origin} tidak terdaftar di cors allowed-origins")
        assert r.status_code == 204

    def test_forbidden_origin_rejected(self, base_url, user):
        r = requests.get(
            f"{base_url}/v1/users/me",
            headers={**auth_hdr(user["token"]), "Origin": "http://evil.example.com"},
        )
        assert r.status_code in (401, 403)

    def test_unknown_route_404(self, base_url):
        r = requests.get(f"{base_url}/definitely-not-a-route")
        assert r.status_code == 404

    def test_method_not_allowed(self, base_url, user):
        r = requests.delete(f"{base_url}/v1/users/me", headers=auth_hdr(user["token"]))
        assert r.status_code == 405


# ============================================================
# helper kecil
# ============================================================

def auth_pb2_wx_delete(work_id: str) -> bytes:
    req = user_pb2.DeleteWorkExperienceRequest()
    req.id.value = work_id
    return req.SerializeToString()
