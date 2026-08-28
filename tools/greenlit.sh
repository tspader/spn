#!/usr/bin/env sh
set -eu

if [ $# -ne 2 ]; then
  echo "usage: greenlit.sh <workflow> <sha>" >&2
  exit 1
fi
WORKFLOW="$1"
SHA="$2"

RUN="$(gh run list --commit "$SHA" --workflow "$WORKFLOW.yml" --branch main --event push --json databaseId --jq '.[0].databaseId')"
if [ -z "$RUN" ]; then
  echo "greenlit: no push run of $WORKFLOW.yml on main for $SHA" >&2
  exit 1
fi

CONCLUSION="$(gh run view "$RUN" --json jobs --jq ".jobs[] | select(.name == \"$WORKFLOW\") | .conclusion")"
if [ "$CONCLUSION" != "success" ]; then
  echo "greenlit: $WORKFLOW aggregate concluded '$CONCLUSION' on run $RUN" >&2
  exit 1
fi
