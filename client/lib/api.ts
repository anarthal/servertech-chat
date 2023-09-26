import {
  CreateAccountRequest,
  CreateAccountResponse,
  ErrorId,
  LoginRequest,
  LoginResponse,
} from "@/lib/apiTypes";

const sendRequest = ({
  body,
  method,
  path,
}: {
  path: string;
  body: object;
  method: "GET" | "POST";
}) => {
  return fetch(`/api/${path}`, {
    method,
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
};

const parseErrorResponse = async <T extends ErrorId>(
  res: Response,
  allowedCodes: T[],
): Promise<{ type: T }> => {
  const { id } = await res.json();
  if (typeof id === "string") {
    for (const code of allowedCodes) {
      if (id === code) {
        return { type: code };
      }
    }
  }
  throw new Error(`Request error: HTTP ${res.status}, error ${id}`);
};

export async function login({
  email,
  password,
}: LoginRequest): Promise<LoginResponse> {
  // Compose and send the request
  const res = await sendRequest({
    method: "POST",
    path: "login",
    body: { email, password },
  });

  // Parse the response
  if (res.ok) {
    return { type: "ok" };
  } else {
    return await parseErrorResponse(res, [ErrorId.LoginFailed]);
  }
}

export async function createAccount({
  username,
  email,
  password,
}: CreateAccountRequest): Promise<CreateAccountResponse> {
  // Compose and send the request
  const res = await sendRequest({
    method: "POST",
    path: "create-account",
    body: { username, email, password },
  });

  // Parse the response
  if (res.ok) {
    return { type: "ok" };
  } else {
    return await parseErrorResponse(res, [
      ErrorId.EmailExists,
      ErrorId.UsernameExists,
    ]);
  }
}
