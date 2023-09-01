/**
 * @type {import('next').NextConfig}
 */
const nextConfig = {
  output: "export", // we're generating static files, since we don't have a node server
  images: {
    unoptimized: true, // required by export
  },
};

module.exports = nextConfig;
