#!/usr/bin/env bash
set -euo pipefail

poc_dir=$(cd "$(dirname "$0")/.." && pwd)
btstack_dir=${BTSTACK_ROOT:-${PICO_SDK_PATH:?Set PICO_SDK_PATH or BTSTACK_ROOT}/lib/btstack}
expected=501e6d2b86e6c92bfb9c390bcf55709938e25ac1

test "$(git -C "$btstack_dir" rev-parse HEAD)" = "$expected" || {
    echo "Expected Pico SDK 2.2.0 BTstack $expected" >&2
    exit 1
}

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cp -a "$poc_dir" "$scratch/poc"

python3 - "$scratch/poc/classic_hid.c" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
needle = "    btstack_run_loop_execute_on_main_thread(&g_start_hid_callback);"
replacement = "    connect_target(g_target_addr); /* FORK-00 negative control: intentionally synchronous */"
count = text.count(needle)
if count != 1:
    raise SystemExit(f"expected exactly one deferred HID scheduling call, found {count}")
path.write_text(text.replace(needle, replacement, 1))
PY

includes=(
    -I"$scratch/poc/tests/include" -I"$scratch/poc" -I"$btstack_dir/src"
    -I"$btstack_dir/3rd-party/bluedroid/encoder/include"
    -I"$btstack_dir/3rd-party/bluedroid/decoder/include"
    -I"$btstack_dir/3rd-party/yxml"
)

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -DENABLE_CLASSIC \
    -ffunction-sections -fdata-sections "${includes[@]}" \
    "$scratch/poc/tests/bonding_transition_test.c" "$btstack_dir/src/btstack_util.c" \
    -Wl,--gc-sections -o "$scratch/bonding_transition_negative"

set +e
"$scratch/bonding_transition_negative" >"$scratch/output.log" 2>&1
status=$?
set -e

if [ "$status" -eq 0 ]; then
    cat "$scratch/output.log" >&2
    echo "FAIL: synchronous post-bond HID startup unexpectedly passed" >&2
    exit 1
fi

if ! grep -q "old_acl_present" "$scratch/output.log"; then
    cat "$scratch/output.log" >&2
    echo "FAIL: negative control failed for an unexpected reason" >&2
    exit 1
fi

echo "PASS: synchronous post-bond HID startup is rejected by the stale-ACL invariant"
