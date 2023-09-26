// The session ID cookie is HttpOnly, so the client has no way to inspect
// whether it's logged in or not. We keep a flag in localStorage that we set
// when we authenticate and clear when the server sends an unauthorized response.
// This is used for client-side redirections.

const key = "hasAuth";
const value = "1";

// Are we authenticated?
export function hasAuth(): boolean {
  return localStorage.getItem(key) === value;
}

// Set the authenticated flag
export function setHasAuth() {
  localStorage.setItem(key, value);
}

// Remove the authenticated flag
export function clearHasAuth() {
  localStorage.removeItem(key);
}
