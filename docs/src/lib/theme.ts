import type { ThemeRegistrationRaw } from 'shiki'

const token = (name: string, scope: string[]) => ({ scope, settings: { foreground: `var(--code-${name})` } })

export const theme: ThemeRegistrationRaw = {
  name: 'spn',
  type: 'dark',
  colors: {
    'editor.background': 'var(--code-bg)',
    'editor.foreground': 'var(--code-fg)',
  },
  settings: [
    token('comment', ['comment', 'punctuation.definition.comment']),
    token('string', ['string', 'punctuation.definition.string']),
    token('keyword', ['keyword', 'storage.modifier', 'entity.name.section', 'markup.heading', 'entity.name.tag', 'punctuation.separator.prompt']),
    token('type', ['storage.type', 'support.type', 'entity.name.type', 'keyword.type']),
    token('function', ['entity.name.function', 'support.function']),
    token('constant', ['constant.numeric', 'constant.language', 'variable.other.constant']),
    token('fg', ['support.type.property-name']),
    token('output', ['meta.output']),
  ],
}
