#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -I"$root" "$root/bridge_bus.c" "$here/bridge_bus_test.c" -o "$work/bridge_bus_test"
"$work/bridge_bus_test"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -I"$root" "$root/canonical_hid.c" "$here/canonical_hid_test.c" -o "$work/canonical_hid_test"
"$work/canonical_hid_test"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -I"$root" "$root/keyboard_report_queue.c" "$here/keyboard_report_queue_test.c" -o "$work/keyboard_report_queue_test"
"$work/keyboard_report_queue_test"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -I"$root" \
  "$root/ui_interaction.c" "$root/ui_model.c" "$root/ui_profile_state.c" \
  "$root/ui_renderer.c" "$root/ui_profile_feedback.c" "$root/hat.c" \
  "$here/fork05_ui_test.c" -o "$work/fork05_ui_test"
"$work/fork05_ui_test"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic -I"$root" \
  "$root/ble_mouse_parser.c" "$here/ble_mouse_parser_test.c" \
  -o "$work/ble_mouse_parser_test"
"$work/ble_mouse_parser_test"
python3 "$here/architecture_test.py" "$root"
python3 "$here/canonical_hid_contract_test.py" "$root"
python3 "$here/fixed_usb_contract_test.py" "$root"
python3 "$here/ble_mouse_contract_test.py" "$root"
python3 "$here/fork05_contract_test.py" "$root"
