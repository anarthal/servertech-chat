import json
from websockets.sync.client import connect
from contextlib import contextmanager
from .models import HelloEvent, MessageWithoutId, ReceivedMessagesEvent, SentMessagesEvent, SentMessagesEventPayload, User


def test_hello():
    with connect("ws://localhost:8080") as websocket:
        message = HelloEvent.model_validate_json(websocket.recv(timeout=3))

        # We know which rooms we have, but not the messages they've got
        expected_rooms = [
            ("beast", "Boost.Beast"),
            ("async", "Boost.Async"),
            ("db", "Database connectors"),
            ("wasm", "Web assembly"),
        ]

        for actual, (expected_id, expected_name) in zip(message.payload.rooms, expected_rooms):
            assert actual.id == expected_id
            assert actual.name == expected_name


# Clean up any received message before closing the socket.
# There seems to be a race condition that hangs close if we don't do this.
@contextmanager
def connect_websocket():
    try:
        ws = connect("ws://localhost:8080", close_timeout=1)
        yield ws
    finally:
        try:
            while True:
                ws.recv(timeout=0)
        except:
            pass
        ws.close()


def test_sending_messages():
    with connect_websocket() as ws1:
        with connect_websocket() as ws2:
            # Read hello messages
            HelloEvent.model_validate_json(ws1.recv(timeout=1))
            HelloEvent.model_validate_json(ws2.recv(timeout=1))

            # Send a message through ws1
            user = User(id="user1", username="username1")
            sent_msg = SentMessagesEvent(
                type='messages',
                payload=SentMessagesEventPayload(
                    roomId="wasm",
                    messages=[MessageWithoutId(
                        user=user,
                        content="Test message 1"
                    )]
                )
            )
            ws1.send(sent_msg.model_dump_json())

            # Both ws1 and ws2 should receive it
            ws1_msg = ReceivedMessagesEvent.model_validate_json(ws1.recv())
            ws2_msg = ReceivedMessagesEvent.model_validate_json(ws2.recv())

            # Validate one of them
            assert len(ws1_msg.payload.messages) == 1
            recv_msg = ws1_msg.payload.messages[0]
            assert recv_msg.user == user
            assert recv_msg.content == "Test message 1"

            # The other one should be identical
            assert ws1_msg.model_dump_json() == ws2_msg.model_dump_json()

