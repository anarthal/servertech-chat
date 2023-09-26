import { createAccount, login } from "@/lib/api";
import { ErrorId } from "@/lib/apiTypes";
import fetchMock from "jest-fetch-mock";

describe("api", () => {
  // Mock the fetch browser API, only for these tests
  beforeAll(() => {
    fetchMock.enableMocks();
  });

  afterAll(() => {
    fetchMock.disableMocks();
  });

  beforeEach(() => {
    fetchMock.resetMocks();
  });

  // Data
  const email = "test@test.com";
  const password = "Useruser10!";
  const username = "somenickname";

  test("login success", async () => {
    // Mock setup
    fetchMock.mockIf("/api/login", async (req) => {
      // Validate headers
      expect(req.headers.get("Content-Type")).toEqual("application/json");

      // Validate body
      expect(await req.json()).toStrictEqual({ email, password });

      // Return OK
      return {
        status: 204,
      };
    });

    // Call the function
    const res = await login({
      email,
      password,
    });

    // Validate
    expect(fetchMock).toBeCalledTimes(1);
    expect(res).toStrictEqual({ type: "ok" });
  });

  test("login failure", async () => {
    // Mock setup
    fetchMock.mockIf("/api/login", async () => {
      return {
        status: 400,
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          id: "LOGIN_FAILED",
        }),
      };
    });

    // Call the function
    const res = await login({
      email,
      password,
    });

    // Validate
    expect(fetchMock).toBeCalledTimes(1);
    expect(res).toStrictEqual({ type: ErrorId.LoginFailed });
  });

  test("create account success", async () => {
    // Mock setup
    fetchMock.mockIf("/api/create-account", async (req) => {
      // Validate headers
      expect(req.headers.get("Content-Type")).toEqual("application/json");

      // Validate body
      expect(await req.json()).toStrictEqual({ email, password, username });

      // Return response
      return {
        status: 204,
      };
    });

    // Call the function
    const res = await createAccount({
      email,
      password,
      username,
    });

    // validate
    expect(fetchMock).toBeCalledTimes(1);
    expect(res).toStrictEqual({ type: "ok" });
  });

  test.each([
    ["EMAIL_EXISTS", ErrorId.EmailExists],
    ["USERNAME_EXISTS", ErrorId.UsernameExists],
  ])("Renders the initial and color for %s", async (errorIdStr, errorId) => {
    // Mock setup
    fetchMock.mockIf("/api/create-account", async () => {
      return {
        status: 400,
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          id: errorIdStr,
        }),
      };
    });

    // Call the function
    const res = await createAccount({
      email,
      password,
      username,
    });

    // Validate
    expect(fetchMock).toBeCalledTimes(1);
    expect(res).toStrictEqual({ type: errorId });
  });
});
