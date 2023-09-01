import { fireEvent, render, screen } from "@testing-library/react";
import ChatPage, { ChatScreen } from "@/pages/chat";
import "@testing-library/jest-dom";
import {
  ClientMessagesEvent,
  HelloEvent,
  Room,
  ServerMessagesEvent,
  User,
} from "@/lib/apiTypes";
import WS from "jest-websocket-mock";
import userEvent from "@testing-library/user-event";
import { saveUser } from "@/lib/user";

describe("chat page", () => {
  // Data to mock the server
  const u1: User = { id: "u1", username: "User one" };
  const u2: User = { id: "u2", username: "User two" };
  const beastRoom: Room = {
    id: "beast",
    name: "Boost.Beast",
    hasMoreMessages: false,
    messages: [
      { id: "2-0", user: u2, timestamp: 1000, content: "Message 2 beast" },
      { id: "1-0", user: u1, timestamp: 900, content: "Message 1 beast" },
    ],
  };
  const wasmRoom: Room = {
    id: "wasm",
    name: "Web Assembly",
    hasMoreMessages: false,
    messages: [
      { id: "2-0", user: u2, timestamp: 12000, content: "Message 2 wasm" },
      { id: "1-0", user: u1, timestamp: 11000, content: "Message 1 wasm" },
    ],
  };
  const dbRoom: Room = {
    id: "db",
    name: "Database connectors",
    hasMoreMessages: false,
    messages: [
      { id: "2-0", user: u2, timestamp: 10100, content: "Message 2 db" },
      { id: "1-0", user: u1, timestamp: 10000, content: "Message 1 db" },
    ],
  };
  const emptyRoom: Room = {
    id: "empty",
    name: "Empty room",
    hasMoreMessages: false,
    messages: [],
  };

  // ChatScreen contains the page rendering, without state-management and
  // network interactions
  test("ChatScreen component", () => {
    // Render
    const { asFragment } = render(
      <ChatScreen
        rooms={[beastRoom, wasmRoom]}
        currentRoom={wasmRoom}
        currentUserId="u2"
        onClickRoom={jest.fn()}
        onMessage={jest.fn()}
      />,
    );

    // Rooms rendered
    expect(screen.getByText("Boost.Beast")).toBeInTheDocument();
    expect(screen.getByText("Web Assembly")).toBeInTheDocument();

    // Only messages for the wasm room are rendered, since it's the one selected
    expect(screen.getByText("Message 1 wasm")).toBeInTheDocument();
    expect(screen.getAllByText("Message 2 wasm").length).toBe(2); // latest message
    expect(screen.queryByText("Message 1 beast")).toBeNull();
    expect(screen.queryByText("Message 2 beast")).toBeInTheDocument(); // latest message

    // Only user one's avatar is shown, since we're user two
    expect(screen.getByText("User one")).toBeInTheDocument();
    expect(screen.queryByText("User two")).toBeNull();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  // ChatPage contains the entire page, including websocket interactions and
  // state management. We use an in-process, mock webserver to simulate the
  // C++ server
  describe("ChatPage", () => {
    // Data
    const helloEvt: HelloEvent = {
      type: "hello",
      payload: {
        rooms: [beastRoom, wasmRoom, dbRoom, emptyRoom],
      },
    };

    // Cleanup any data and connections from previous tests
    beforeEach(() => {
      WS.clean();
    });

    test("Room navigation", async () => {
      // create a WS instance, listening on the expected port
      const server = new WS(process.env.NEXT_PUBLIC_WEBSOCKET_URL);

      // render the page. This should connect the websocket
      render(<ChatPage />);
      await server.connected;

      // The page shows loading until the hello event is received
      expect(screen.getByText("Loading...")).toBeInTheDocument();

      // Hello event
      server.send(JSON.stringify(helloEvt));

      // The client draws the screen, selecting the room with the
      // latest message
      expect(screen.getByText("Boost.Beast")).toBeInTheDocument();
      expect(screen.getByText("Web Assembly")).toBeInTheDocument();
      expect(screen.getByText("Message 1 wasm")).toBeInTheDocument(); // not the latest
      expect(screen.getAllByTestId("room-name")[0]).toHaveTextContent(
        "Web Assembly",
      );

      // Clicking on a room switches to it
      fireEvent.click(screen.getByText("Database connectors"));
      expect(screen.getByText("Message 1 db")).toBeInTheDocument();
      expect(screen.queryByText("Message 1 wasm")).toBeNull();

      // Clicking on an empty room is OK
      fireEvent.click(screen.getByText("Empty room"));
      expect(screen.getByText("No more messages to show")).toBeInTheDocument();
      expect(screen.queryByText("Message 1 db")).toBeNull();
    });

    test("Sending and receiving messages", async () => {
      // Set up
      saveUser(u2);
      const server = new WS(process.env.NEXT_PUBLIC_WEBSOCKET_URL);

      // render the page and send the hello
      render(<ChatPage />);
      await server.connected;
      server.send(JSON.stringify(helloEvt));
      expect(screen.getAllByTestId("room-name")[0]).toHaveTextContent(
        "Web Assembly",
      ); // wasm is the room with the latest message

      // A message is received, and the target room is not selected
      const serverMsgsEvt: ServerMessagesEvent = {
        type: "messages",
        payload: {
          roomId: "db",
          messages: [
            {
              id: "3-0",
              user: u1,
              timestamp: Date.now(),
              content: "A new db message",
            },
          ],
        },
      };
      server.send(JSON.stringify(serverMsgsEvt));

      // The room moves up and the message shows in the room entry
      expect(screen.getAllByTestId("room-name")[0]).toHaveTextContent(
        "Database connectors",
      );
      expect(screen.getByText("A new db message")).toBeInTheDocument();

      // A message is sent by the user. It is sent to the server
      userEvent.keyboard("A message for the wasm guys{Enter}");
      const clientMsgsEvt = JSON.parse((await server.nextMessage) as string);
      const expectedClientMsgsEvt: ClientMessagesEvent = {
        type: "messages",
        payload: {
          roomId: "wasm",
          messages: [
            {
              user: u2,
              content: "A message for the wasm guys",
            },
          ],
        },
      };
      expect(clientMsgsEvt).toStrictEqual(expectedClientMsgsEvt);

      // It doesn't appear until acked by the server
      expect(screen.queryByText("A message for the wasm guys")).toBeNull();

      // The server relays it
      const serverMsgsEvt2: ServerMessagesEvent = {
        type: "messages",
        payload: {
          roomId: "wasm",
          messages: [
            {
              id: "5-0",
              user: u2,
              timestamp: Date.now(),
              content: "A message for the wasm guys",
            },
          ],
        },
      };
      server.send(JSON.stringify(serverMsgsEvt2));

      // It appears both in the room entry and in the messages screen
      expect(screen.getAllByText("A message for the wasm guys")).toHaveLength(
        2,
      );
      expect(screen.getAllByTestId("room-name")[0]).toHaveTextContent(
        "Web Assembly",
      );
    });
  });
});
