/** @type {import('tailwindcss').Config} */
export default {
  content: ["./lib/**/stats/_layout.html.eex"],
  corePlugins: {
    preflight: false,
  },
  theme: {
    extend: {},
  },
  plugins: [],
  experimental: {
    optimizeUniversalDefaults: true,
  }
}
