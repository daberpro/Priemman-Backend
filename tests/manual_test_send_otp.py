#!/usr/bin/env python3
"""
Script manual untuk test endpoint /v1/auth/send-otp
TIDAK butuh pip install apapun untuk HTTP-nya (pakai urllib bawaan Python).
Untuk protobuf, jalankan pakai python dari venv testsuite userver, karena
di situ package `protobuf` sudah otomatis terinstall:

    ./build/venv-userver-default/bin/python3 tests/manual_test_send_otp.py --email your-email@example.com

Kalau python3 sistem kamu juga sudah punya `protobuf` (cek: python3 -c "import google.protobuf"),
bisa juga langsung:

    python3 tests/manual_test_send_otp.py --email your-email@example.com
"""

import argparse
import sys
import pathlib
import urllib.request
import urllib.error

# Arahkan ke folder tempat auth_pb2.py & common_pb2.py hasil generate protoc berada.
PROTO_DIR = pathlib.Path(__file__).parent / "proto"
sys.path.insert(0, str(PROTO_DIR))

try:
    import auth_pb2
except ImportError as e:
    print(f"[ERROR] Tidak bisa import auth_pb2 dari {PROTO_DIR}")
    print(f"        Detail: {e}")
    print("Pastikan sudah generate: protoc --proto_path=proto --python_out=tests/proto proto/auth.proto proto/common.proto")
    sys.exit(1)


def send_otp(base_url: str, email: str) -> None:
    request = auth_pb2.SendOtpRequest(email=email)
    payload = request.SerializeToString()

    print(f"[INFO] Mengirim request ke {base_url}/v1/auth/send-otp")
    print(f"[INFO] Email target: {email}")
    print(f"[INFO] Payload size: {len(payload)} bytes")

    req = urllib.request.Request(
        f"{base_url}/v1/auth/send-otp",
        data=payload,
        headers={"Content-Type": "application/x-protobuf"},
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=15) as response:
            status_code = response.status
            body = response.read()
    except urllib.error.HTTPError as e:
        status_code = e.code
        body = e.read()
    except urllib.error.URLError as e:
        print(f"[ERROR] Tidak bisa connect ke server. Pastikan server sudah jalan.")
        print(f"        Detail: {e}")
        sys.exit(1)

    print(f"\n[RESPONSE] HTTP Status: {status_code}")

    if status_code == 200:
        proto_response = auth_pb2.SendOtpResponse()
        try:
            proto_response.ParseFromString(body)
            print(f"[RESPONSE] success: {proto_response.success}")
            print(f"[RESPONSE] message: {proto_response.message}")
            print(f"[RESPONSE] cooldown_seconds: {proto_response.cooldown_seconds}")
        except Exception as e:
            print(f"[ERROR] Gagal parse response sebagai protobuf: {e}")
            print(f"[RAW BODY] {body}")
    else:
        print(f"[RAW BODY] {body}")

    print(f"\n[INFO] Cek terminal server untuk log detail SMTP (grep 'smtp\\|email\\|Failed to send').")


def main():
    parser = argparse.ArgumentParser(description="Manual test untuk endpoint send-otp")
    parser.add_argument("--url", default="http://localhost:8080", help="Base URL server")
    parser.add_argument("--email", required=True, help="Email tujuan untuk dikirimi OTP")
    args = parser.parse_args()

    send_otp(args.url, args.email)


if __name__ == "__main__":
    main()
