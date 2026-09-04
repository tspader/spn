import { cp, readdir, rm } from 'node:fs/promises'
import { join, resolve } from 'node:path'
import { build } from 'astro'

const DOCS = join(import.meta.dir, '..')
const DIST = join(DOCS, 'dist')

const out = resolve(process.argv[2])

await build({ root: DOCS })
for (const entry of await readdir(DIST)) await rm(join(out, entry), { recursive: true, force: true })
await cp(DIST, out, { recursive: true })
