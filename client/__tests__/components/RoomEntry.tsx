import { render, screen } from "@testing-library/react";
import RoomEntry from "@/components/RoomEntry";
import "@testing-library/jest-dom";

describe("RoomEntry", () => {
  test("RoomEntry not selected", () => {
    const onClick = jest.fn();

    // Render
    const { asFragment } = render(
      <RoomEntry
        id="myRoomId"
        name="My Room"
        lastMessage="What happened?"
        onClick={onClick}
        selected={false}
      />,
    );

    // Sanity check
    expect(screen.getByText("My Room")).toBeInTheDocument();
    expect(screen.getByText("What happened?")).toBeInTheDocument();

    // Clicking calls the callback
    screen.getByText("My Room").click();
    expect(onClick).toBeCalledWith("myRoomId");

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("RoomEntry selected", () => {
    // Render
    const { asFragment } = render(
      <RoomEntry
        id="myRoomId"
        name="My Room"
        lastMessage="What happened?"
        onClick={jest.fn()}
        selected={true}
      />,
    );

    // Sanity check
    expect(screen.getByText("My Room")).toBeInTheDocument();
    expect(screen.getByText("What happened?")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
