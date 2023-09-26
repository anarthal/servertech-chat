import { render, screen } from "@testing-library/react";
import FormCard from "@/components/FormCard";
import "@testing-library/jest-dom";

describe("FormCard", () => {
  test("renders correctly", () => {
    // Render
    const { asFragment } = render(
      <FormCard title="Hello card">
        <p>A child</p>
      </FormCard>,
    );

    // Sanity check
    expect(screen.getByText("Hello card")).toBeInTheDocument();
    expect(screen.getByText("A child")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });
});
