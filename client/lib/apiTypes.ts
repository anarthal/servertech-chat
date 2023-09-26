// Contains all type definitions for out HTTP and websocket APIs

export enum ErrorId {
  BadRequest = "BAD_REQUEST",
  LoginFailed = "LOGIN_FAILED",
  UsernameExists = "USERNAME_EXISTS",
  EmailExists = "EMAIL_EXISTS",
}

export type APIErrorResponse = {
  id: string;
};

export type CreateAccountRequest = {
  username: string;
  email: string;
  password: string;
};

export type CreateAccountResponse =
  | { type: "ok" }
  | { type: ErrorId.UsernameExists }
  | { type: ErrorId.EmailExists };

export type LoginRequest = {
  email: string;
  password: string;
};

export type LoginResponse = { type: "ok" } | { type: ErrorId.LoginFailed };

export type User = {
  id: number;
  username: string;
};

export type ServerMessage = {
  id: string;
  user: User;
  content: string;
  timestamp: number; // miliseconds since epoch
};

export type ClientMessage = {
  content: string;
};

export type Room = {
  id: string;
  name: string;
  messages: ServerMessage[];
  hasMoreMessages: boolean;
};

export type HelloEvent = {
  type: "hello";
  payload: {
    me: User;
    rooms: Room[];
  };
};

export type ServerMessagesEvent = {
  type: "serverMessages";
  payload: {
    roomId: string;
    messages: ServerMessage[];
  };
};

export type AnyServerEvent = HelloEvent | ServerMessagesEvent;

export type ClientMessagesEvent = {
  type: "clientMessages";
  payload: {
    roomId: string;
    messages: ClientMessage[];
  };
};
