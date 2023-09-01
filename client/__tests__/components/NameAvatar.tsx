import { render, screen } from "@testing-library/react";
import NameAvatar from "@/components/NameAvatar";
import "@testing-library/jest-dom";

describe("NameAvatar", () => {
  test.each([
    ["John", "J"],
    ["Doe", "D"],
    ["Fran", "F"],
  ])("Renders the initial and color for %s", (name, initial) => {
    // Render
    const { asFragment } = render(<NameAvatar name={name} />);

    // Sanity check
    expect(screen.getByText(initial)).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
