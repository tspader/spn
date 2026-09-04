import { h } from 'hastscript'
import type { ShikiTransformer } from 'shiki'

export const codeBlock: ShikiTransformer = {
  name: 'code-block',
  root(root) {
    root.children = [h('figure.code-block', [h('button.code-copy.floating', { type: 'button', 'aria-label': 'Copy' }), ...root.children])]
  },
}
