from pydantic import BaseModel, Field
from typing import List, Literal

# pydantic models describing our API structures. These classes perform
# automatic validation of JSON data

class User(BaseModel):
    id: str
    username: str


class MessageWithoutId(BaseModel):
    user: User
    content: str


class Message(MessageWithoutId):
    id: str = Field(pattern=r'[0-9]+\-[0-9]+') # Redis stream ID
    timestamp: int


class Room(BaseModel):
    id: str
    name: str
    messages: List[Message]
    hasMoreMessages: bool


class HelloEventPayload(BaseModel):
    rooms: List[Room]


class HelloEvent(BaseModel):
    type: Literal['hello']
    payload: HelloEventPayload


class SentMessagesEventPayload(BaseModel):
    roomId: str
    messages: List[MessageWithoutId]


class SentMessagesEvent(BaseModel):
    type: Literal['messages']
    payload: SentMessagesEventPayload


class ReceivedMessagesEventPayload(BaseModel):
    roomId: str
    messages: List[Message]


class ReceivedMessagesEvent(BaseModel):
    type: Literal['messages']
    payload: ReceivedMessagesEventPayload
