import common_pb2
import auth_pb2


async def test_send_otp_success(service_client):
    request = auth_pb2.SendOtpRequest(email="ari.susanto@priemman.my.id")

    response = await service_client.post(
        '/v1/auth/send-otp',
        data=request.SerializeToString(),
        headers={'Content-Type': 'application/x-protobuf'},
    )

    assert response.status == 200

    proto_response = auth_pb2.SendOtpResponse()
    proto_response.ParseFromString(response.content)

    assert proto_response.success is True
    assert proto_response.cooldown_seconds == 30


async def test_send_otp_empty_email(service_client):
    request = auth_pb2.SendOtpRequest(email="")

    response = await service_client.post(
        '/v1/auth/send-otp',
        data=request.SerializeToString(),
        headers={'Content-Type': 'application/x-protobuf'},
    )

    assert response.status == 400


async def test_send_otp_malformed_protobuf(service_client):
    response = await service_client.post(
        '/v1/auth/send-otp',
        data=b'\x00\x01\x02garbage',
        headers={'Content-Type': 'application/x-protobuf'},
    )

    assert response.status == 400
