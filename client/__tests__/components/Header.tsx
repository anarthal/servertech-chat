import { render, screen } from "@testing-library/react";
import Header, { SmallHeader, LargeHeader } from "@/components/Header";
import "@testing-library/jest-dom";
import useIsSmallScreen from "@/hooks/useIsSmallScreen";

jest.mock("@/hooks/useIsSmallScreen");

describe("Header", () => {
  test("Small header: arrow shown", () => {
    // Setup
    const fn = jest.fn();

    // Render
    const { asFragment } = render(
      <SmallHeader showArrow={true} onArrowClick={fn} />,
    );

    // The arrow is there
    const arrow = screen.getByTestId("ArrowBackIcon");
    expect(arrow).toBeInTheDocument();

    // Clicking the arrow triggers the function
    arrow.parentElement.click();
    expect(fn).toHaveBeenCalledTimes(1);

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Small header: arrow shown disabled when callback is undefined", () => {
    // Render
    const { asFragment } = render(
      <SmallHeader showArrow={true} onArrowClick={undefined} />,
    );

    // The arrow is there
    const arrow = screen.getByTestId("ArrowBackIcon");
    expect(arrow).toBeInTheDocument();

    // Clicking the arrow does not crash
    arrow.parentElement.click();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Small header: arrow not shown", () => {
    // Render
    const { asFragment } = render(
      <SmallHeader showArrow={false} onArrowClick={undefined} />,
    );

    // The arrow is not there
    expect(screen.queryByTestId("ArrowBackIcon")).toBeNull();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Large header", () => {
    // Render
    const { asFragment } = render(<LargeHeader />);

    // Sanity check
    expect(screen.getByText("Source code")).toBeInTheDocument();

    // Snapshot test
    expect(asFragment()).toMatchSnapshot();
  });

  test("Header: large screen", () => {
    // Setup
    // @ts-ignore
    useIsSmallScreen.mockReturnValue(false);

    // Render
    render(<Header onArrowClick={undefined} showArrow={true} />);

    // The large header was rendered
    expect(screen.getByText("Source code")).toBeInTheDocument();
    expect(screen.queryByTestId("ArrowBackIcon")).toBeNull();
  });

  test("Header: small screen", () => {
    // Setup
    // @ts-ignore
    useIsSmallScreen.mockReturnValue(true);

    // Render
    render(<Header onArrowClick={undefined} showArrow={true} />);

    // The small header was rendered
    expect(screen.queryByText("Source code")).toBeNull();
    expect(screen.getByTestId("ArrowBackIcon")).toBeInTheDocument();
  });
});
