import { glob } from 'astro/loaders'
import { defineCollection } from 'astro:content'
import { Page } from '@spn/docs/lib/page'

export const collections = {
  docs: defineCollection({ loader: glob({ pattern: '*.{md,mdx}', base: './content' }), schema: Page }),
}
