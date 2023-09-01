/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./components/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  important: "#__next",
  theme: {
    extend: {},
  },
  plugins: [],
  // Required for compatibility with MUI, otherwise styles break
  corePlugins: {
    preflight: false,
  },
};
