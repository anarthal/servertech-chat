#!/usr/bin/env python

import json
from websockets.sync.client import connect

def hello():
    with connect("ws://localhost:8080") as websocket:
        message = websocket.recv()
        print(json.loads(message))

hello()