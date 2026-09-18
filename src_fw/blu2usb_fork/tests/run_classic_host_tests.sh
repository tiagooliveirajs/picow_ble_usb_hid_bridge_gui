#!/usr/bin/env bash
set -euo pipefail
fork_dir=$(cd "$(dirname "$0")/.." && pwd)
btstack_dir=${BTSTACK_ROOT:-${PICO_SDK_PATH:?Set PICO_SDK_PATH or BTSTACK_ROOT}/lib/btstack}
expected=501e6d2b86e6c92bfb9c390bcf55709938e25ac1

test "$(git -C "$btstack_dir" rev-parse HEAD)" = "$expected" || {
    echo "Expected Pico SDK 2.2.0 BTstack $expected" >&2
    exit 1
}

python3 "$fork_dir/tests/classic_keyboard_contract_test.py"

test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
includes=(
    -I"$fork_dir" -I"$btstack_dir/src"
    -I"$btstack_dir/3rd-party/bluedroid/encoder/include"
    -I"$btstack_dir/3rd-party/bluedroid/decoder/include"
    -I"$btstack_dir/3rd-party/yxml"
)

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DENABLE_CLASSIC -DENABLE_BLE \
    "${includes[@]}" -fsyntax-only "$fork_dir/classic_keyboard.c"

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DENABLE_CLASSIC \
    "${includes[@]}" \
    "$fork_dir/tests/classic_keyboard_report_test.c" \
    "$fork_dir/classic_keyboard_report.c" \
    "$btstack_dir/src/btstack_hid_parser.c" \
    "$btstack_dir/src/btstack_util.c" \
    -o "$test_dir/classic_keyboard_report_test"

"$test_dir/classic_keyboard_report_test"
