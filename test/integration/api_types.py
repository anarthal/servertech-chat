from pydantic import BaseModel, Field
from typing import List, Literal
from enum import Enum

# pydantic models describing our API structures. These classes perform
# automatic validation of JSON data

# Error codes that can be returned by API endpoints


class ErrorId(Enum):
    BadRequest = "BAD_REQUEST"
    LoginFailed = "LOGIN_FAILED"
    UsernameExists = "USERNAME_EXISTS"
    EmailExists = "EMAIL_EXISTS"


class APIErrorResponse(BaseModel):
    id: ErrorId
    message: str


# POST /api/create-account
class CreateAccountRequest(BaseModel):
    username: str
    email: str
    password: str


# POST /api/login
class LoginRequest(BaseModel):
    email: str
    password: str


# Chat rooms and messages
class User(BaseModel):
    id: int
    username: str


class ClientMessage(BaseModel):
    content: str


class ServerMessage(BaseModel):
    id: str = Field(pattern=r'[0-9]+\-[0-9]+')  # Redis stream ID
    timestamp: int
    content: str
    user: User


class Room(BaseModel):
    id: str
    name: str
    messages: List[ServerMessage]
    hasMoreMessages: bool


class HelloEventPayload(BaseModel):
    me: User
    rooms: List[Room]


class HelloEvent(BaseModel):
    type: Literal['hello']
    payload: HelloEventPayload


class ClientMessagesEventPayload(BaseModel):
    roomId: str
    messages: List[ClientMessage]


class ClientMessagesEvent(BaseModel):
    type: Literal['clientMessages']
    payload: ClientMessagesEventPayload


class ServerMessagesEventPayload(BaseModel):
    roomId: str
    messages: List[ServerMessage]


class ServerMessagesEvent(BaseModel):
    type: Literal['serverMessages']
    payload: ServerMessagesEventPayload
