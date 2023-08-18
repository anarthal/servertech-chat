import { render, screen } from "@testing-library/react";
import { OtherUserMessage, MyMessage } from "@/components/Message";
import "@testing-library/jest-dom";

describe("Message", () => {
  const ts = new Date("2020-03-10T21:03:02.123+00:00").getTime();

  test("OtherUserMessage", () => {
    // Render
    const { asFragment } = render(
      <OtherUserMessage
        content="Hello dear!"
        timestamp={ts}
        username="Fancy user"
      />,
    );

    // Rendered correctly
    expect(screen.getByText("F")).toBeInTheDocument();
    expect(screen.getByText("Hello dear!")).toBeInTheDocument();
    expect(screen.getByText("Mar 10, 2020, 10:03 PM")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("MyMessage", () => {
    // Render
    const { asFragment } = render(
      <MyMessage content="Hello dear!" timestamp={ts} />,
    );

    // Rendered correctly
    expect(screen.getByText("Hello dear!")).toBeInTheDocument();
    expect(screen.getByText("Mar 10, 2020, 10:03 PM")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
