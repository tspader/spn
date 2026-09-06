import { readFileSync, readdirSync, writeFileSync } from 'node:fs'
import { basename, join } from 'node:path'
import { parseFrontmatter } from '@astrojs/markdown-remark'
import GithubSlugger from 'github-slugger'
import type { Link } from 'mdast'
import { toString } from 'mdast-util-to-string'
import remarkMdx from 'remark-mdx'
import remarkParse from 'remark-parse'
import { unified } from 'unified'
import { visit } from 'unist-util-visit'
import { targets } from '@spn/docs/data/install'
import { Page } from '@spn/docs/lib/page'

const DOCS = join(import.meta.dir, '..')
const CONTENT = join(DOCS, 'content')
const INTRO = join(DOCS, 'readme', 'intro.md')
const README = join(DOCS, '..', 'README.md')

const parser = unified().use(remarkParse)
const mdx = unified().use(remarkParse).use(remarkMdx)

// components used in content, and the markdown they stand for in the README
const components: Record<string, () => string> = {
  Install: () => targets.map((target) => `### ${target.label}\n\n\`\`\`${target.lang}\n${target.command}\n\`\`\``).join('\n\n'),
}

interface Section {
  id: string
  page: Page
  body: string
}

function headings(markdown: string): string[] {
  const found: string[] = []
  visit(parser.parse(markdown), 'heading', (node) => {
    found.push(toString(node))
  })
  return found
}

function links(markdown: string): Link[] {
  const found: Link[] = []
  visit(parser.parse(markdown), 'link', (node) => {
    found.push(node)
  })
  return found
}

function lower(markdown: string): string {
  const nodes: { start: number; end: number; text: string }[] = []
  visit(mdx.parse(markdown), (node) => {
    const { start, end } = node.position!
    if (node.type === 'mdxjsEsm') nodes.push({ start: start.offset!, end: end.offset!, text: '' })
    if (node.type === 'mdxJsxFlowElement') {
      const render = components[node.name!]
      if (!render) throw new Error(`no readme rendering for <${node.name} />`)
      nodes.push({ start: start.offset!, end: end.offset!, text: render() })
    }
  })
  return nodes.reduceRight((text, node) => text.slice(0, node.start) + node.text + text.slice(node.end), markdown)
}

function load(name: string): Section {
  const { frontmatter, content } = parseFrontmatter(readFileSync(join(CONTENT, name), 'utf8'))
  const body = name.endsWith('.mdx') ? lower(content) : content
  return { id: name.replace(/\.mdx?$/, ''), page: Page.parse(frontmatter), body: body.trim() }
}

function anchors(intro: string, sections: Section[]): Map<string, string> {
  const readme = new GithubSlugger()
  const map = new Map<string, string>()
  for (const heading of headings(intro)) readme.slug(heading)
  for (const section of sections) {
    const site = new GithubSlugger()
    map.set(`/docs/${section.id}`, `#${readme.slug(section.page.title)}`)
    for (const heading of headings(section.body)) map.set(`/docs/${section.id}#${site.slug(heading)}`, `#${readme.slug(heading)}`)
  }
  return map
}

function rewrite(markdown: string, map: Map<string, string>): string {
  const internal = links(markdown).filter((link) => link.url.startsWith('/docs/'))
  return internal.reduceRight((text, link) => {
    const anchor = map.get(link.url)
    if (!anchor) throw new Error(`unresolved link ${link.url}`)
    const end = link.position!.end.offset! - 1
    const start = end - link.url.length
    if (text.slice(start, end) !== link.url) throw new Error(`unexpected link syntax at ${link.position!.start.line}`)
    return text.slice(0, start) + anchor + text.slice(end)
  }, markdown)
}

const intro = readFileSync(INTRO, 'utf8').trim()
const sections = readdirSync(CONTENT)
  .map(load)
  .filter((section) => section.page.readme)
  .sort((a, b) => a.page.order - b.page.order)
const map = anchors(intro, sections)
const parts = [rewrite(intro, map), ...sections.map((section) => `# ${section.page.title}\n\n${rewrite(section.body, map)}`)]
writeFileSync(README, `${parts.join('\n\n')}\n`)
