import {
  AnyServerEvent,
  ClientMessagesEvent,
  ClientMessage,
} from "@/lib/apiTypes";

// Contains functions to parse raw data into API types and vice-versa

export function parseWebsocketMessage(s: string): AnyServerEvent {
  // TODO: some validation here would be good
  return JSON.parse(s);
}

export function serializeMessagesEvent(
  roomId: string,
  message: ClientMessage,
): string {
  const evt: ClientMessagesEvent = {
    type: "clientMessages",
    payload: {
      roomId,
      messages: [message],
    },
  };
  return JSON.stringify(evt);
}
