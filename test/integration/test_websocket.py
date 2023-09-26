from websockets.sync.client import connect, ClientConnection
from websockets.exceptions import ConnectionClosedError
from contextlib import contextmanager
from .api_types import (
    HelloEvent,
    ClientMessage,
    ServerMessagesEvent,
    ClientMessagesEvent,
    ClientMessagesEventPayload,
)
from typing import Generator
from .conftest import ws_endpoint, GeneratedSession
import pytest


# These tests verify that the websocket API works as expected, with a real
# server and a real DB running. These tests should be run with exclusive
# access to the server (no other tests or human users running in parallel).
# They don't access or modify the DB directly, so it's safe to run them
# several times or with existent data.


@contextmanager
def _connect_websocket(sid: str) -> Generator[ClientConnection, None, None]:
    ws = connect(ws_endpoint(), close_timeout=0.1, additional_headers={
        'Cookie': f'sid={sid}'
    })
    try:
        yield ws
    finally:
        # Read any received message before closing the socket.
        # There seems to be a race condition that hangs close if we don't do this.
        try:
            while True:
                ws.recv(timeout=0)
        except:
            pass
        ws.close()


def test_hello(session: GeneratedSession):
    '''
    When we connect to the server, we receive a hello as the first event,
    containing the rooms the user is subscribed to.
    '''

    with _connect_websocket(sid=session.sid) as websocket:
        # Receive and parse the hello event
        message = HelloEvent.model_validate_json(websocket.recv(timeout=3))

        # We know which rooms we have, but not the messages they've got
        expected_rooms = [
            ("beast", "Boost.Beast"),
            ("async", "Boost.Async"),
            ("db", "Database connectors"),
            ("wasm", "Web assembly"),
        ]

        # Validate user
        assert message.payload.me.username == session.username

        # Validate rooms
        for actual, (expected_id, expected_name) in zip(message.payload.rooms, expected_rooms):
            assert actual.id == expected_id
            assert actual.name == expected_name


def test_send_receive_messages(session: GeneratedSession, session2: GeneratedSession):
    '''
    We can broadcast messages to other clients. When we send a message to the
    server, we receive it back with its ID and timestamp, and other clients
    receive it, too.
    '''
    with _connect_websocket(sid=session.sid) as ws1:
        with _connect_websocket(sid=session2.sid) as ws2:
            # Read hello messages
            h1 = HelloEvent.model_validate_json(ws1.recv(timeout=1))
            h2 = HelloEvent.model_validate_json(ws2.recv(timeout=1))
            assert h1.payload.me.username == session.username
            assert h2.payload.me.username == session2.username

            # Send a message through ws1
            sent_msg = ClientMessagesEvent(
                type='clientMessages',
                payload=ClientMessagesEventPayload(
                    roomId="wasm",
                    messages=[ClientMessage(
                        content="Test message 1"
                    )]
                )
            )
            ws1.send(sent_msg.model_dump_json())

            # Both ws1 and ws2 should receive it
            ws1_msg = ServerMessagesEvent.model_validate_json(ws1.recv())
            ws2_msg = ServerMessagesEvent.model_validate_json(ws2.recv())

            # Validate one of them
            assert len(ws1_msg.payload.messages) == 1
            recv_msg = ws1_msg.payload.messages[0]
            assert recv_msg.user == h1.payload.me
            assert recv_msg.content == "Test message 1"

            # The other one should be identical
            assert ws1_msg.model_dump_json() == ws2_msg.model_dump_json()

            # Send a message through ws2
            sent_msg = ClientMessagesEvent(
                type='clientMessages',
                payload=ClientMessagesEventPayload(
                    roomId="beast",
                    messages=[ClientMessage(
                        content="Test message 2"
                    )]
                )
            )
            ws2.send(sent_msg.model_dump_json())

            # Both ws1 and ws2 should receive it
            ws1_msg = ServerMessagesEvent.model_validate_json(ws1.recv())
            ws2_msg = ServerMessagesEvent.model_validate_json(ws2.recv())

            # Sanity checks
            assert ws1_msg.payload.messages[0].user == h2.payload.me
            assert ws1_msg.model_dump_json() == ws2_msg.model_dump_json()
            assert ws1_msg.payload.roomId == 'beast'


def test_new_messages_appear_in_history(session: GeneratedSession, session2: GeneratedSession):
    '''
    After we send a message, it appears in message history, and other clients
    get it in the hello event.
    '''

    with _connect_websocket(session.sid) as ws1:
        # Read hello message
        h1 = HelloEvent.model_validate_json(ws1.recv(timeout=1))

        # Send a couple of messages
        msg_1 = ClientMessagesEvent(
            type='clientMessages',
            payload=ClientMessagesEventPayload(
                roomId="wasm",
                messages=[ClientMessage(
                    content="Test message 1"
                )]
            )
        )
        msg_2 = ClientMessagesEvent(
            type='clientMessages',
            payload=ClientMessagesEventPayload(
                roomId="wasm",
                messages=[ClientMessage(
                    content="Test message 2"
                )]
            )
        )
        ws1.send(msg_1.model_dump_json())
        ws1.send(msg_2.model_dump_json())

        # Receive the sent messages. This guarantees that the server has received and processed them
        ServerMessagesEvent.model_validate_json(ws1.recv())
        ServerMessagesEvent.model_validate_json(ws1.recv())

        # Connect another websocket and read the hello
        with _connect_websocket(sid=session2.sid) as ws2:
            h2 = HelloEvent.model_validate_json(ws2.recv(timeout=1))

    # Validate that two new messages were added to the room's history
    wasm_room_2 = next(r for r in h2.payload.rooms if r.id == 'wasm')

    # Last message goes first
    assert wasm_room_2.messages[0].content == 'Test message 2'
    assert wasm_room_2.messages[0].user == h1.payload.me
    assert wasm_room_2.messages[1].content == 'Test message 1'
    assert wasm_room_2.messages[1].user == h1.payload.me
    assert wasm_room_2.messages[0].id > wasm_room_2.messages[1].id


def test_not_authenticated():
    '''
    If we're not authenticated, the websocket is closed with a policy violation code.
    '''
    with pytest.raises(ConnectionClosedError) as err:
        with _connect_websocket('bad_session_id') as ws:
            # The websocket is opened and then closed. Reading triggers the error
            ws.recv(timeout=1)
    assert err.value.rcvd is not None
    assert err.value.rcvd.code == 1008
