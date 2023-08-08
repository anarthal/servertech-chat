#!/usr/bin/env python

import json
from websockets.sync.client import connect
import pytest
import redis
from contextlib import contextmanager
import re

@pytest.fixture(scope="session", autouse=True)
def prepare_db(request):
    redis.Redis().client().flushdb()


def test_hello():
    with connect("ws://localhost:8080") as websocket:
        message = websocket.recv(timeout=3)
        expected = {
            "type": "hello",
            "payload": {
                "rooms": [
                    {
                        "id": "beast",
                        "name": "Boost.Beast",
                        "messages": [],
                        "hasMoreMessages": False,
                    },
                    {
                        "id": "async",
                        "name": "Boost.Async",
                        "messages": [],
                        "hasMoreMessages": False,
                    },
                    {
                        "id": "db",
                        "name": "Database connectors",
                        "messages": [],
                        "hasMoreMessages": False,
                    },
                    {
                        "id": "wasm",
                        "name": "Web assembly",
                        "messages": [],
                        "hasMoreMessages": False,
                    },
                ]
            }
        }
        actual = json.loads(message)

        assert actual == expected

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
                ws1.recv(timeout=1)
                ws2.recv(timeout=1)

                # Send a message through ws1
                msg = {
                    "roomId": "wasm",
                    "messages": [{
                        "user": {
                            "id": "user1",
                            "username": "username1"
                        },
                        "content": "Test message 1"
                    }]
                }
                ws1.send(json.dumps({ "type": "messages", "payload": msg }))

                # Both ws1 and ws2 should receive it
                ws1_msg = json.loads(ws1.recv())
                ws2_msg = json.loads(ws2.recv())

                # Validate one of them
                assert ws1_msg["type"] == "messages"
                payload = ws1_msg["payload"]
                assert payload["roomId"] == "wasm"
                assert len(payload["messages"]) == 1
                recv_msg = payload["messages"][0]
                assert recv_msg["user"] == msg["messages"][0]["user"]
                assert recv_msg["content"] == "Test message 1"
                assert re.match(r'[0-9]+\-[0-9]+', recv_msg["id"]) is not None

                # The other one should be identical
                assert ws1_msg == ws2_msg

