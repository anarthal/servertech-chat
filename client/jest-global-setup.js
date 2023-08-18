module.exports = async () => {
  process.env.TZ = "Europe/Madrid";
  process.env.NEXT_PUBLIC_WEBSOCKET_URL = "ws://localhost:1234";
};
