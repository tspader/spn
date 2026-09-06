export interface Target {
  id: string
  label: string
  lang: string
  prompt: string
  command: string
}

export const targets: Target[] = [
  { id: 'posix', label: 'macOS & Linux', lang: 'sh', prompt: '$', command: 'curl -fsSL https://spn.spader.zone/install | sh' },
  { id: 'windows', label: 'Windows', lang: 'powershell', prompt: '>', command: 'irm https://spn.spader.zone/install.ps1 | iex' },
]
