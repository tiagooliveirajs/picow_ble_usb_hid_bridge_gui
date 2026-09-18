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

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root" \
    "$root/canonical_hid.c" "$here/canonical_hid_test.c" \
    -o "$work/canonical_hid_test"
"$work/canonical_hid_test"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$root" \
    "$root/keyboard_report_queue.c" "$here/keyboard_report_queue_test.c" \
    -o "$work/keyboard_report_queue_test"
"$work/keyboard_report_queue_test"

python3 "$here/architecture_test.py" "$root"
python3 "$here/canonical_hid_contract_test.py" "$root"
