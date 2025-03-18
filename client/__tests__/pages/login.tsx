import * as auth from "@/lib/hasAuth";
import * as api from "@/lib/api";
import LoginPage from "@/pages/login";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { useRouter } from "next/router";
import { ErrorId } from "@/lib/apiTypes";
import "@testing-library/jest-dom";
import useIsSmallScreen from "@/hooks/useIsSmallScreen";

jest.mock("@/hooks/useIsSmallScreen");
jest.mock("@/lib/hasAuth");
jest.mock("@/lib/api");

const mockedAuth = jest.mocked(auth);
const mockedApi = jest.mocked(api);

describe("login page", () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  const email = "test@test.com";
  const password = "Useruser10!";

  test("Snapshot test with large screen", () => {
    // @ts-ignore
    useIsSmallScreen.mockReturnValue(false);
    mockedAuth.hasAuth.mockReturnValue(false);

    const { asFragment } = render(<LoginPage />);

    expect(asFragment()).toMatchSnapshot();
  });

  test("Snapshot test with small screen", () => {
    // @ts-ignore
    useIsSmallScreen.mockReturnValue(true);
    mockedAuth.hasAuth.mockReturnValue(false);

    const { asFragment } = render(<LoginPage />);

    expect(asFragment()).toMatchSnapshot();
  });

  test("Redirects to chat if the user is logged in", () => {
    mockedAuth.hasAuth.mockReturnValue(true);

    render(<LoginPage />);

    expect(mockedAuth.hasAuth).toHaveBeenCalled();
    expect(useRouter().replace).toHaveBeenCalledTimes(1);
    expect(useRouter().replace).toHaveBeenCalledWith("/chat");
  });

  test("Login success", async () => {
    // Mock setup
    const user = userEvent.setup();
    mockedAuth.hasAuth.mockReturnValue(false);
    mockedApi.login.mockResolvedValue({ type: "ok" });

    // Render
    render(<LoginPage />);

    // Fill in the form and hit submit
    await user.type(screen.getByLabelText("email"), email);
    await user.type(screen.getByLabelText("password"), password);
    await user.click(screen.getByText("Log me in!"));

    // We sent the data to the server
    expect(mockedApi.login).toHaveBeenCalledTimes(1);
    expect(mockedApi.login).toHaveBeenCalledWith({ email, password });

    // We stored the auth state and navigated
    expect(mockedAuth.setHasAuth).toHaveBeenCalled();
    expect(useRouter().push).toHaveBeenCalledTimes(1);
    expect(useRouter().push).toHaveBeenCalledWith("/chat");
  });

  test("Login failure", async () => {
    // Mock setup
    const user = userEvent.setup();
    mockedAuth.hasAuth.mockReturnValue(false);
    mockedApi.login.mockResolvedValue({ type: ErrorId.LoginFailed });

    // Render
    render(<LoginPage />);

    // Fill in the form and hit submit
    await user.type(screen.getByLabelText("email"), email);
    await user.type(screen.getByLabelText("password"), password);
    await user.click(screen.getByText("Log me in!"));

    // We sent the data to the server
    expect(mockedApi.login).toHaveBeenCalledTimes(1);
    expect(mockedApi.login).toHaveBeenCalledWith({ email, password });

    // We showed an error message and did not navigate
    expect(
      screen.getByText("Invalid username or password."),
    ).toBeInTheDocument();
    expect(mockedAuth.setHasAuth).not.toHaveBeenCalled();
    expect(useRouter().push).not.toHaveBeenCalled();
  });
});
