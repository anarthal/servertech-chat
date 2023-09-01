module.exports = async () => {
  // We show local times in some components and test for it, so we need
  // to set a reliable time zone
  process.env.TZ = "Europe/Madrid";

  // Used by tests that employ a mock websocket server
  process.env.NEXT_PUBLIC_WEBSOCKET_URL = "ws://localhost:1234";
};
