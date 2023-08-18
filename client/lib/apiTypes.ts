// Contains all type definitions for out HTTP and websocket APIs

export type User = {
  id: string;
  username: string;
};

export type Message = {
  id: string;
  user: User;
  content: string;
  timestamp: number; // miliseconds since epoch
};

export type MessageWithoutId = {
  user: User;
  content: string;
};

export type Room = {
  id: string;
  name: string;
  messages: Message[];
  hasMoreMessages: boolean;
};

export type HelloEvent = {
  type: "hello";
  payload: {
    rooms: Room[];
  };
};

export type ServerMessagesEvent = {
  type: "messages";
  payload: {
    roomId: string;
    messages: Message[];
  };
};

export type AnyServerEvent = HelloEvent | ServerMessagesEvent;

export type ClientMessagesEvent = {
  type: "messages";
  payload: {
    roomId: string;
    messages: MessageWithoutId[];
  };
};
