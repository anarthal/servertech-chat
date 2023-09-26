import * as auth from "@/lib/hasAuth";
import * as api from "@/lib/api";
import LoginPage from "@/pages/login";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { useRouter } from "next/router";
import { ErrorId } from "@/lib/apiTypes";
import "@testing-library/jest-dom";

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

  test("Snapshot test", () => {
    mockedAuth.hasAuth.mockReturnValue(false);

    const { asFragment } = render(<LoginPage />);

    expect(asFragment()).toMatchSnapshot();
  });

  test("Redirects to chat if the user is logged in", () => {
    mockedAuth.hasAuth.mockReturnValue(true);

    render(<LoginPage />);

    expect(mockedAuth.hasAuth).toBeCalled();
    expect(useRouter().replace).toBeCalledTimes(1);
    expect(useRouter().replace).toBeCalledWith("/chat");
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
    expect(mockedApi.login).toBeCalledTimes(1);
    expect(mockedApi.login).toBeCalledWith({ email, password });

    // We stored the auth state and navigated
    expect(mockedAuth.setHasAuth).toBeCalled();
    expect(useRouter().push).toBeCalledTimes(1);
    expect(useRouter().push).toBeCalledWith("/chat");
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
    expect(mockedApi.login).toBeCalledTimes(1);
    expect(mockedApi.login).toBeCalledWith({ email, password });

    // We showed an error message and did not navigate
    expect(
      screen.getByText("Invalid username or password."),
    ).toBeInTheDocument();
    expect(mockedAuth.setHasAuth).not.toBeCalled();
    expect(useRouter().push).not.toBeCalled();
  });
});
