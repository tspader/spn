#!/bin/bash

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
API_TESTS="$REPO_ROOT/test/manual/api"
OUT_DIR="$REPO_ROOT/build/llm/graphs"
AMALG="$OUT_DIR/all_graphs.md"

mkdir -p "$OUT_DIR"
rm -f "$AMALG"

failed=0
passed=0

echo "# API Test Graphs" > "$AMALG"
echo "" >> "$AMALG"
echo "Generated: $(date)" >> "$AMALG"
echo "" >> "$AMALG"

for dir in "$API_TESTS"/*/; do
  name=$(basename "$dir")
  
  # Skip build_deps - known broken (sqlite linking issue)
  if [[ "$name" == "build_deps" ]]; then
    echo "SKIP: $name (known broken)"
    continue
  fi
  
  echo "=== $name ==="
  
  pushd "$dir" > /dev/null
  
  # Clean and build first
  rm -rf build
  if ! tspn build -p debug 2>&1; then
    echo "FAILED: $name (build)"
    ((failed++)) || true
    popd > /dev/null
    continue
  fi
  
  # Generate graph
  out_file="$OUT_DIR/${name}.mmd"
  if tspn graph -o "$out_file" 2>&1 && [[ -f "$out_file" ]]; then
    echo "OK: $name -> $out_file"
    ((passed++)) || true
    
    # Append to amalgamation
    echo "## $name" >> "$AMALG"
    echo "" >> "$AMALG"
    # Include test.md if it exists
    if [[ -f "test.md" ]]; then
      cat "test.md" >> "$AMALG"
      echo "" >> "$AMALG"
    fi
    echo '```mermaid' >> "$AMALG"
    cat "$out_file" >> "$AMALG"
    echo '```' >> "$AMALG"
    echo "" >> "$AMALG"
  else
    echo "FAILED: $name (graph)"
    ((failed++)) || true
  fi
  
  popd > /dev/null
done

echo ""
echo "=== Summary ==="
echo "Passed: $passed"
echo "Failed: $failed"
echo "Amalgamation: $AMALG"

if [[ $failed -gt 0 ]]; then
  exit 1
fi
