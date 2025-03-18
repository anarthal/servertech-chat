import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import MessageInputBar from "@/components/MessageInputBar";
import "@testing-library/jest-dom";

describe("MessageInputBar", () => {
  test("renders message input bar", () => {
    // Render
    const { asFragment } = render(<MessageInputBar onMessage={jest.fn()} />);

    // The bar is there
    expect(
      screen.getByPlaceholderText("Type a message..."),
    ).toBeInTheDocument();

    // The send icon is there
    expect(screen.getByTestId("SendIcon")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("keyboard interactions", async () => {
    const onMessage = jest.fn();
    const user = userEvent.setup();

    // Render
    render(<MessageInputBar onMessage={onMessage} />);

    // Typing autofocus the inputs
    await user.keyboard("this is a");

    // No callback until enter is hit
    expect(onMessage).toHaveBeenCalledTimes(0);

    // Hitting enter calls the callback
    await user.keyboard(" message{Enter}");
    expect(onMessage).toHaveBeenCalledWith("this is a message");
    expect(onMessage).toHaveBeenCalledTimes(1);

    // Hitting Enter cleared the input bar, so we can send another message
    onMessage.mockReset();
    await user.keyboard("another message!{Enter}");
    expect(onMessage).toHaveBeenCalledWith("another message!");
    expect(onMessage).toHaveBeenCalledTimes(1);
  });

  test("send icon interactions", async () => {
    const onMessage = jest.fn();
    const user = userEvent.setup();

    // Render
    render(<MessageInputBar onMessage={onMessage} />);

    // Type a message
    await user.keyboard("this is a message");

    // Clicking the send icon calls the callback
    screen.getByTestId("SendIcon").parentElement.click();
    expect(onMessage).toHaveBeenCalledWith("this is a message");
    expect(onMessage).toHaveBeenCalledTimes(1);

    // Sending the message cleared the input. We can now send another message
    onMessage.mockReset();
    await user.keyboard("another message!");
    screen.getByTestId("SendIcon").parentElement.click();
    expect(onMessage).toHaveBeenCalledWith("another message!");
    expect(onMessage).toHaveBeenCalledTimes(1);
  });
});
