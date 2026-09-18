#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root" \
    "$root/bridge_bus.c" "$here/bridge_bus_test.c" \
    -o "$work/bridge_bus_test"
"$work/bridge_bus_test"
python3 "$here/architecture_test.py" "$root"
