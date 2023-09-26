/**
 * @type {import('next').NextConfig}
 */
const devConfig = {
  rewrites: async () => [
    {
      source: "/api/:path(.*)",
      destination: `${
        process.env.SERVER_BASE_URL || "http://localhost:8080"
      }/api/:path`,
    },
  ],
};

/**
 * @type {import('next').NextConfig}
 */
const prodConfig = {
  output: "export", // we're generating static files, since we don't have a node server
  images: {
    unoptimized: true, // required by export
  },
};

module.exports =
  process.env.NODE_ENV === "development" ? devConfig : prodConfig;
