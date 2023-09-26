import Head from "@/components/Head";
import Header from "@/components/Header";
import RoomEntry from "@/components/RoomEntry";
import { useCallback, useEffect, useReducer, useRef } from "react";
import {
  parseWebsocketMessage,
  serializeMessagesEvent,
} from "@/lib/apiSerialization";
import { ServerMessage, Room, User } from "@/lib/apiTypes";
import { MyMessage, OtherUserMessage } from "@/components/Message";
import MessageInputBar from "@/components/MessageInputBar";
import autoAnimate from "@formkit/auto-animate";
import { useRouter } from "next/router";
import { clearHasAuth } from "@/lib/hasAuth";

// The chat screen. This component uses React state management using
// useReducer.

// The type of the state that useReducer handles
type State = {
  currentUser: User | null;
  loading: boolean;
  rooms: Record<string, Room>; // id => room
  currentRoomId: string | null;
};

// To be dispatched when the hello event is received. Sets the initial state
type SetInitialStateAction = {
  type: "set_initial_state";
  payload: {
    currentUser: User;
    rooms: Room[];
  };
};

// Adds a batch of messages to a certain room
type AddMessagesAction = {
  type: "add_messages";
  payload: {
    roomId: string;
    messages: ServerMessage[];
  };
};

// Sets the current active room (e.g. when the user clicks in a room)
type SetCurrentRoomAction = {
  type: "set_current_room";
  payload: {
    roomId: string;
  };
};

// Any of the above
type Action = SetInitialStateAction | AddMessagesAction | SetCurrentRoomAction;

// A Message, either from our user or from another user
const Message = ({
  userId,
  username,
  content,
  timestamp,
  currentUserId,
}: {
  userId: number;
  username: string;
  content: string;
  timestamp: number;
  currentUserId: number;
}) => {
  if (userId === currentUserId) {
    return <MyMessage content={content} timestamp={timestamp} />;
  } else {
    return (
      <OtherUserMessage
        content={content}
        timestamp={timestamp}
        username={username}
      />
    );
  }
};

// Helpers to process actions
const addNewMessages = (
  messages: ServerMessage[],
  newMessages: ServerMessage[],
): ServerMessage[] => {
  const res = [...newMessages];
  res.reverse();
  return res.concat(messages);
};

function doSetInitialState(action: SetInitialStateAction): State {
  const { rooms, currentUser } = action.payload;
  const roomsById = {};
  for (const room of rooms) roomsById[room.id] = room;
  return {
    currentUser,
    loading: false,
    rooms: roomsById,
    currentRoomId: findRoomWithLatestMessage(rooms)?.id || null,
  };
}

function doAddMessages(state: State, action: AddMessagesAction): State {
  // Find the room
  const room = state.rooms[action.payload.roomId];
  if (!room) return state; // We don't know what the server is talking about

  // Add the new messages to the array
  return {
    ...state,
    rooms: {
      ...state.rooms,
      [action.payload.roomId]: {
        ...room,
        messages: addNewMessages(room.messages, action.payload.messages),
      },
    },
  };
}

function getLastMessageTimestamp(room: Room): number {
  return room.messages.length > 0 ? room.messages[0].timestamp : 0;
}

function sortRoomsByLastMessageInplace(rooms: Room[]) {
  rooms.sort(
    (r1, r2) => getLastMessageTimestamp(r2) - getLastMessageTimestamp(r1),
  );
}

function findRoomWithLatestMessage(rooms: Room[]): Room | null {
  return rooms.reduce((prev, current) => {
    if (prev === null) return current;
    else
      return getLastMessageTimestamp(prev) > getLastMessageTimestamp(current)
        ? prev
        : current;
  }, null);
}

// The reducer
function reducer(state: State, action: Action): State {
  const { type, payload } = action;
  switch (type) {
    case "set_initial_state":
      return doSetInitialState(action);
    case "add_messages":
      return doAddMessages(state, action);
    case "set_current_room":
      return {
        ...state,
        currentRoomId: payload.roomId,
      };
    default:
      return state;
  }
}

// Retrieves the websocket URL to connect to. This is either embedded by
// the client generation process, defaulting to the host and port we're been
// served from
function getWebsocketURL(): string {
  return (
    process.env.NEXT_PUBLIC_WEBSOCKET_URL ||
    `ws://${location.hostname}:${location.port}/api/ws`
  );
}

// Exposed for the sake of testing
export const ChatScreen = ({
  rooms,
  currentRoom,
  onClickRoom,
  currentUserId,
  onMessage,
}: {
  rooms: Room[];
  currentRoom: Room | null;
  currentUserId: number;
  onClickRoom: (roomId: string) => void;
  onMessage: (msg: string) => void;
}) => {
  // Animate transitions. We use this instead of useAutoAnimate because the
  // latter doesn't play well with Jest
  const messagesRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);
  const roomsRef = useCallback((elm) => {
    if (elm) autoAnimate(elm);
  }, []);

  const currentRoomMessages = currentRoom?.messages || [];

  return (
    <div
      className="flex-1 flex min-h-0"
      style={{ borderTop: "1px solid var(--boost-light-grey)" }}
    >
      <div className="flex-1 flex flex-col overflow-y-scroll" ref={roomsRef}>
        {rooms.map(({ id, name, messages }) => (
          <RoomEntry
            key={id}
            id={id}
            name={name}
            selected={id === currentRoom.id}
            lastMessage={messages[0]?.content || "No messages yet"}
            onClick={onClickRoom}
          />
        ))}
      </div>
      <div className="flex-[3] flex flex-col">
        <div
          className="flex-1 flex flex-col-reverse p-5 overflow-y-scroll"
          style={{ backgroundColor: "var(--boost-light-grey)" }}
          ref={messagesRef}
        >
          {currentRoomMessages.length === 0 ? (
            <p>No more messages to show</p>
          ) : (
            currentRoomMessages.map((msg) => (
              <Message
                key={msg.id}
                userId={msg.user.id}
                username={msg.user.username}
                content={msg.content}
                timestamp={msg.timestamp}
                currentUserId={currentUserId}
              />
            ))
          )}
        </div>
        <MessageInputBar onMessage={onMessage} />
      </div>
    </div>
  );
};

// Websocket close code to signal that authentication is required
const CODE_POLICY_VIOLATION = 1008;

// The actual component
export default function ChatPage() {
  // State management
  const [state, dispatch] = useReducer(reducer, {
    currentUser: null,
    loading: true,
    rooms: {},
    currentRoomId: null,
  });

  // Send a websocket message to the server when the user hits enter
  const onMessageTyped = useCallback(
    (msg: string) => {
      const evt = serializeMessagesEvent(state.currentRoomId, {
        content: msg,
      });
      websocketRef.current.send(evt);
    },
    [state.currentRoomId],
  );

  // Change the current active room when the user clicks on it
  const onClickRoom = useCallback(
    (roomId: string) => {
      dispatch({ type: "set_current_room", payload: { roomId } });
    },
    [dispatch],
  );

  const router = useRouter();

  // Handle server websocket messages
  const onWebsocketMessage = useCallback((event: MessageEvent) => {
    const { type, payload } = parseWebsocketMessage(event.data);
    switch (type) {
      case "hello":
        dispatch({
          type: "set_initial_state",
          payload: {
            currentUser: payload.me,
            rooms: payload.rooms,
          },
        });
        break;
      case "serverMessages":
        dispatch({
          type: "add_messages",
          payload: {
            roomId: payload.roomId,
            messages: payload.messages,
          },
        });
        break;
    }
  }, []);

  const onClose = useCallback(
    (event: CloseEvent) => {
      // On authentication failure, the server closes the websocket with this code.
      if (event.code === CODE_POLICY_VIOLATION) {
        clearHasAuth();
        router.replace("/login");
      }
    },
    [router],
  );

  // Create a websocket
  const websocketRef = useRef<WebSocket>(null);
  useEffect(() => {
    websocketRef.current = new WebSocket(getWebsocketURL());
    websocketRef.current.addEventListener("message", onWebsocketMessage);
    websocketRef.current.addEventListener("close", onClose);

    // Close the socket on page close
    return () => {
      websocketRef.current.removeEventListener("message", onWebsocketMessage);
      websocketRef.current.removeEventListener("close", onClose);
      websocketRef.current.close();
    };
  }, [onWebsocketMessage, onClose]);

  // Handle loading state
  if (state.loading) return <p>Loading...</p>;

  // The actual screen
  const currentRoom = state.rooms[state.currentRoomId] || null;
  const rooms = Object.values(state.rooms);
  sortRoomsByLastMessageInplace(rooms);

  return (
    <>
      <Head />
      <div className="flex flex-col h-full">
        <Header />
        <ChatScreen
          rooms={rooms}
          currentRoom={currentRoom}
          currentUserId={state.currentUser.id}
          onClickRoom={onClickRoom}
          onMessage={onMessageTyped}
        />
      </div>
    </>
  );
}
