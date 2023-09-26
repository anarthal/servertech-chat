import * as auth from "@/lib/hasAuth";
import * as api from "@/lib/api";
import CreateAccountPage from "@/pages/index";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { useRouter } from "next/router";
import { CreateAccountResponse, ErrorId } from "@/lib/apiTypes";
import "@testing-library/jest-dom";

jest.mock("@/lib/hasAuth");
jest.mock("@/lib/api");

const mockedAuth = jest.mocked(auth);
const mockedApi = jest.mocked(api);

describe("CreateAccountPage page", () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  const email = "test@test.com";
  const password = "Useruser10!";
  const username = "somenickname";

  test("Snapshot test", () => {
    const { asFragment } = render(<CreateAccountPage />);

    expect(asFragment()).toMatchSnapshot();
  });

  test("Create account success", async () => {
    // Mock setup
    const user = userEvent.setup();
    mockedApi.createAccount.mockResolvedValue({ type: "ok" });

    // Render
    render(<CreateAccountPage />);

    // Fill in the form and hit submit
    await user.type(screen.getByLabelText("email"), email);
    await user.type(screen.getByLabelText("password"), password);
    await user.type(screen.getByLabelText("username"), username);
    await user.click(screen.getByText("Create my account"));

    // We sent the data to the server
    expect(mockedApi.createAccount).toBeCalledTimes(1);
    expect(mockedApi.createAccount).toBeCalledWith({
      email,
      password,
      username,
    });

    // We stored the auth state and navigated
    expect(mockedAuth.setHasAuth).toBeCalled();
    expect(useRouter().push).toBeCalledTimes(1);
    expect(useRouter().push).toBeCalledWith("/chat");
  });

  test.each([
    [
      ErrorId.UsernameExists,
      "This username is already in use. Please pick a different one.",
    ],
    [ErrorId.EmailExists, "This email address is already in use."],
  ])("Create account failure: %s", async (errorId, expectedMsg) => {
    // Mock setup
    const user = userEvent.setup();
    mockedApi.createAccount.mockResolvedValue({
      type: errorId,
    } as CreateAccountResponse);

    // Render
    render(<CreateAccountPage />);

    // Fill in the form and hit submit
    await user.type(screen.getByLabelText("email"), email);
    await user.type(screen.getByLabelText("password"), password);
    await user.type(screen.getByLabelText("username"), username);
    await user.click(screen.getByText("Create my account"));

    // We sent the data to the server
    expect(mockedApi.createAccount).toBeCalledTimes(1);
    expect(mockedApi.createAccount).toBeCalledWith({
      email,
      password,
      username,
    });

    // We showed an error message and did not navigate
    expect(screen.getByText(expectedMsg)).toBeInTheDocument();
    expect(mockedAuth.setHasAuth).not.toBeCalled();
    expect(useRouter().push).not.toBeCalled();
  });
});
