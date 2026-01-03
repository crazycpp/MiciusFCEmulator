import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],
  // GitHub Pages serves this repo under /MiciusFCEmulator/
  // Keep dev server at '/' but build with correct base when deploying.
  base: process.env.GITHUB_PAGES ? '/MiciusFCEmulator/' : '/',
})
