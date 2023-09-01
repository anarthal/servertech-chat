from websockets.sync.client import connect
from contextlib import contextmanager
from .models import HelloEvent, MessageWithoutId, ReceivedMessagesEvent, SentMessagesEvent, SentMessagesEventPayload, User

# These tests verify that the websocket API works as expected, with a real
# server and a real DB running. These tests should be run with exclusive
# access to the server (no other tests or human users running in parallel).
# They don't access or modify the DB directly, so it's safe to run them
# several times or with existent data.


# Read any received message before closing the socket.
# There seems to be a race condition that hangs close if we don't do this.
@contextmanager
def _connect_websocket():
    ws = connect("ws://localhost:8080", close_timeout=0.1)
    try:
        yield ws
    finally:
        try:
            while True:
                ws.recv(timeout=0)
        except:
            pass
        ws.close()


def test_hello():
    '''
    When we connect to the server, we receive a hello as the first event,
    containing the rooms the user is subscribed to.
    '''

    with connect("ws://localhost:8080") as websocket:
        # Receive and parse the hello event
        message = HelloEvent.model_validate_json(websocket.recv(timeout=3))

        # We know which rooms we have, but not the messages they've got
        expected_rooms = [
            ("beast", "Boost.Beast"),
            ("async", "Boost.Async"),
            ("db", "Database connectors"),
            ("wasm", "Web assembly"),
        ]

        # Validate
        for actual, (expected_id, expected_name) in zip(message.payload.rooms, expected_rooms):
            assert actual.id == expected_id
            assert actual.name == expected_name


def test_send_receive_messages():
    '''
    We can broadcast messages to other clients. When we send a message to the
    server, we receive it back with its ID and timestamp, and other clients
    receive it, too.
    '''
    with _connect_websocket() as ws1:
        with _connect_websocket() as ws2:
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

            # Send a message through ws2
            sent_msg = SentMessagesEvent(
                type='messages',
                payload=SentMessagesEventPayload(
                    roomId="beast",
                    messages=[MessageWithoutId(
                        user=user,
                        content="Test message 2"
                    )]
                )
            )
            ws2.send(sent_msg.model_dump_json())

            # Both ws1 and ws2 should receive it
            ws1_msg = ReceivedMessagesEvent.model_validate_json(ws1.recv())
            ws2_msg = ReceivedMessagesEvent.model_validate_json(ws2.recv())

            # Sanity checks
            assert ws1_msg.model_dump_json() == ws2_msg.model_dump_json()
            assert ws1_msg.payload.roomId == 'beast'


def test_new_messages_appear_in_history():
    '''
    After we send a message, it appears in message history, and other clients
    get it in the hello event.
    '''

    with _connect_websocket() as ws1:
        # Read hello message
        hello_1 = HelloEvent.model_validate_json(ws1.recv(timeout=1))

        # Send a couple of messages
        user = User(id="user1", username="username1")
        msg_1 = SentMessagesEvent(
            type='messages',
            payload=SentMessagesEventPayload(
                roomId="wasm",
                messages=[MessageWithoutId(
                    user=user,
                    content="Test message 1"
                )]
            )
        )
        msg_2 = SentMessagesEvent(
            type='messages',
            payload=SentMessagesEventPayload(
                roomId="wasm",
                messages=[MessageWithoutId(
                    user=user,
                    content="Test message 2"
                )]
            )
        )
        ws1.send(msg_1.model_dump_json())
        ws1.send(msg_2.model_dump_json())

        # Receive the sent messages. This guarantees that the server has received and processed them
        ReceivedMessagesEvent.model_validate_json(ws1.recv())
        ReceivedMessagesEvent.model_validate_json(ws1.recv())

        # Connect another websocket and read the hello
        with _connect_websocket() as ws2:
            hello_2 = HelloEvent.model_validate_json(ws2.recv(timeout=1))
        
    # Validate that two new messages were added to the room's history
    wasm_room_2 = next(r for r in hello_2.payload.rooms if r.id == 'wasm')

    # Last message goes first
    assert wasm_room_2.messages[0].content == 'Test message 2'
    assert wasm_room_2.messages[1].content == 'Test message 1'
    assert wasm_room_2.messages[0].id > wasm_room_2.messages[1].id

