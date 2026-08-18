#!/usr/bin/env python3
"""
Manual login test - untuk test dari domain production
tanpa akses DB langsung.
"""
import os
import sys
import requests
import auth_pb2      # protobuf binding
import project_pb2   # kalau mau test endpoint lain juga

BASE_URL = os.environ.get("PRIEMMAN_BASE_URL", "https://api.priemman.my.id")
PROTO_CONTENT_TYPE = "application/x-protobuf"


def send_otp(email: str):
    """Kirim OTP ke email."""
    req = auth_pb2.SendOtpRequest()
    req.email = email

    r = requests.post(
        f"{BASE_URL}/v1/auth/send-otp",
        data=req.SerializeToString(),
        headers={"Content-Type": PROTO_CONTENT_TYPE},
    )

    print(f"\n[Send OTP] Status: {r.status_code}")

    if r.status_code == 200:
        resp = auth_pb2.SendOtpResponse()
        resp.ParseFromString(r.content)
        print(f"✅ Success: {resp.message}")
        print(f"⏱️  Cooldown: {resp.cooldown_seconds}s")
        return True
    elif r.status_code == 429:
        resp = auth_pb2.SendOtpResponse()
        resp.ParseFromString(r.content)
        print(f"⏳ Cooldown aktif: {resp.message}")
        return False
    else:
        print(f"❌ Error: {r.text[:200]}")
        return False


def verify_otp(email: str, otp_code: str):
    """Verifikasi OTP dan dapatkan session token."""
    req = auth_pb2.VerifyOtpRequest()
    req.email = email
    req.otp = otp_code

    r = requests.post(
        f"{BASE_URL}/v1/auth/verify-otp",
        data=req.SerializeToString(),
        headers={"Content-Type": PROTO_CONTENT_TYPE},
    )

    print(f"\n[Verify OTP] Status: {r.status_code}")

    if r.status_code == 200:
        resp = auth_pb2.VerifyOtpResponse()
        resp.ParseFromString(r.content)
        print(f"✅ Login berhasil!")
        print(f"🆕 New user: {resp.is_new_user}")
        print(f"🔑 Session token: {resp.session_token}")
        return resp.session_token
    else:
        print(f"❌ Error: {r.text[:200]}")
        return None


def get_me(token: str):
    """Test endpoint /v1/users/me."""
    r = requests.get(
        f"{BASE_URL}/v1/users/me",
        headers={"Authorization": f"Bearer {token}"},
    )

    print(f"\n[GET /v1/users/me] Status: {r.status_code}")
    print(f"Content-Type: {r.headers.get('Content-Type', 'unknown')}")

    if r.status_code == 200:
        print("✅ Authenticated! Response received.")
        print(f"Body length: {len(r.content)} bytes")
        # Kalau response-nya protobuf, parse sesuai tipe yang diharapkan
        return True
    else:
        print(f"❌ Error: {r.text[:200]}")
        return False


def main():
    print("=" * 60)
    print("🚀 Manual Login Test - Priemman Backend")
    print(f"🌐 Target: {BASE_URL}")
    print("=" * 60)

    # 1. Health check dulu
    print("\n[Health Check] Testing /ping ...")
    try:
        r = requests.get(f"{BASE_URL}/ping", timeout=5)
        if r.status_code == 200:
            print(f"✅ Server OK: {r.text}")
        else:
            print(f"⚠️  Server returned {r.status_code}")
    except Exception as e:
        print(f"❌ Cannot reach server: {e}")
        return 1

    # 2. Input email (default: ari.susanto@priemman.my.id)
    default_email = "ari.susanto@priemman.my.id"
    email = input(f"\n📧 Email [{default_email}]: ").strip() or default_email

    # 3. Kirim OTP
    print(f"\n📤 Mengirim OTP ke {email} ...")
    if not send_otp(email):
        print("\n💡 Coba lagi setelah cooldown, atau cek email.")
        return 1

    # 4. Minta user cek email
    print("\n" + "=" * 60)
    print("📬 Cek email kamu sekarang!")
    print("   Cari email dengan subject: 'Kode Verifikasi Priemman Studio'")
    print("   Copy kode 6 digit-nya.")
    print("=" * 60)

    # 5. Input OTP manual
    otp_code = input("\n🔢 Masukkan OTP (6 digit): ").strip()
    if not otp_code or len(otp_code) != 6 or not otp_code.isdigit():
        print("❌ OTP harus 6 digit angka")
        return 1

    # 6. Verify OTP
    token = verify_otp(email, otp_code)
    if not token:
        return 1

    # 7. Test endpoint authenticated
    print("\n" + "=" * 60)
    print("🧪 Testing authenticated endpoint ...")
    print("=" * 60)

    get_me(token)

    # 8. Simpan token untuk dipakai test manual lain
    print("\n" + "=" * 60)
    print("💾 Token siap dipakai untuk request lain:")
    print(f"\n   curl -H 'Authorization: Bearer {token}' \\")
    print(f"        {BASE_URL}/v1/users/me\n")

    # Export ke environment (kalau user mau pakai di shell lain)
    with open("/tmp/priemman_token.txt", "w") as f:
        f.write(token)
    print(f"📁 Token disimpan di: /tmp/priemman_token.txt")
    print(f"   Pakai: export TOKEN=$(cat /tmp/priemman_token.txt)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
