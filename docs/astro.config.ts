import mdx from '@astrojs/mdx'
import { defineConfig } from 'astro/config'
import { codeBlock } from './src/lib/code'
import { theme } from './src/lib/theme'

export default defineConfig({
  server: { host: '127.0.0.1', port: 4500 },
  markdown: {
    smartypants: false,
    shikiConfig: { theme, transformers: [codeBlock] },
  },
  integrations: [mdx()],
})
