import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import MessageInputBar from "@/components/MessageInputBar";
import "@testing-library/jest-dom";

describe("MessageInputBar", () => {
  test("renders message input bar", () => {
    // Render
    const { asFragment } = render(<MessageInputBar onMessage={jest.fn()} />);

    // Sanity check
    expect(
      screen.getByPlaceholderText("Type a message..."),
    ).toBeInTheDocument();

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
    expect(onMessage).toBeCalledTimes(0);

    // Hitting enter calls the callback
    await user.keyboard(" message{Enter}");
    expect(onMessage).toBeCalledWith("this is a message");
    expect(onMessage).toBeCalledTimes(1);

    // Hitting Enter cleared the input bar, so we can send another message
    onMessage.mockReset();
    await user.keyboard("another message!{Enter}");
    expect(onMessage).toBeCalledWith("another message!");
    expect(onMessage).toBeCalledTimes(1);
  });
});
