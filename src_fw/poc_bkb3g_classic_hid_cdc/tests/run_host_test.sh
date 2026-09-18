#!/usr/bin/env bash
set -euo pipefail
poc_dir=$(cd "$(dirname "$0")/.." && pwd)
btstack_dir=${BTSTACK_ROOT:-${PICO_SDK_PATH:?Set PICO_SDK_PATH or BTSTACK_ROOT}/lib/btstack}
expected=501e6d2b86e6c92bfb9c390bcf55709938e25ac1
test "$(git -C "$btstack_dir" rev-parse HEAD)" = "$expected" || {
    echo "Expected Pico SDK 2.2.0 BTstack $expected" >&2
    exit 1
}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
includes=(
    -I"$poc_dir/tests/include" -I"$poc_dir" -I"$btstack_dir/src"
    -I"$btstack_dir/3rd-party/bluedroid/encoder/include"
    -I"$btstack_dir/3rd-party/bluedroid/decoder/include"
    -I"$btstack_dir/3rd-party/yxml"
)
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DENABLE_CLASSIC \
    "${includes[@]}" -fsyntax-only "$poc_dir/classic_hid.c"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DENABLE_CLASSIC \
    -ffunction-sections -fdata-sections "${includes[@]}" \
    "$poc_dir/tests/bonding_transition_test.c" "$btstack_dir/src/btstack_util.c" \
    -Wl,--gc-sections -o "$test_dir/bonding_transition_test"
"$test_dir/bonding_transition_test"
bash "$poc_dir/tests/run_negative_control.sh"
