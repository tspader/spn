import { z } from 'astro/zod'

export const Page = z.object({
  title: z.string(),
  group: z.string().default('docs'),
  order: z.number(),
  site: z.boolean().default(true),
  readme: z.boolean().default(true),
})

export type Page = z.infer<typeof Page>
