#!/bin/bash

if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    export PATH="$HOME/.local/bin:$PATH"
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc 2>/dev/null || true
fi

curl --proto '=https' --tlsv1.2 -LsSf https://github.com/tspader/spn/releases/download/v0.3.1/spn-installer.sh | sh

mkdir -p "$HOME/.config/spn"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cat > "$HOME/.config/spn/spn.toml" << EOF
spn = "$SCRIPT_DIR"
EOF

spn build
