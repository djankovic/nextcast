#!/usr/bin/env sh
set -eu

required="fd haproxy pnpm iex"
for cmd in $required; do
  if ! command -v "$cmd" > /dev/null 2>&1; then
    echo "$cmd not found"
    exit 1
  fi
done

trap 'kill $(jobs -p) 2>/dev/null' EXIT

haproxy -f restricted/priv/haproxy.dev.cfg &
fd "\.ex$" | entr -s 'mix compile' &
pnpm run build &
iex -S mix
