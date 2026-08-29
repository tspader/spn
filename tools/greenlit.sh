#!/usr/bin/env sh
set -eu

if [ $# -ne 2 ]; then
  echo "usage: greenlit.sh <workflow> <sha>" >&2
  exit 1
fi
WORKFLOW="$1"
SHA="$2"

CONCLUSION="$(gh run list --commit "$SHA" --workflow "$WORKFLOW.yml" --branch main --event push --json conclusion --jq '.[0].conclusion // empty')"
if [ -z "$CONCLUSION" ]; then
  echo "greenlit: no completed push run of $WORKFLOW.yml on main for $SHA" >&2
  exit 1
fi
if [ "$CONCLUSION" != "success" ]; then
  echo "greenlit: $WORKFLOW concluded '$CONCLUSION' on $SHA" >&2
  exit 1
fi
