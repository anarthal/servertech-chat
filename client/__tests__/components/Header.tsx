import { render, screen } from "@testing-library/react";
import Header from "@/components/Header";
import "@testing-library/jest-dom";

describe("Header", () => {
  test("renders the page header", () => {
    // Render
    const { asFragment } = render(<Header />);

    // Sanity check
    expect(screen.getByText("Source code")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
