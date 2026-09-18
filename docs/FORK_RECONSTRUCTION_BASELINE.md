# FORK reconstruction baseline — frozen at FORK-00

Status: **FORK-00 baseline contract**.

This file is the durable implementation-side handoff for the BLU2USB reconstruction in this repository. It exists so future work does not depend on chat history. The planning source of truth is `tiagooliveirajs/markdown-only-format-planner`; this document freezes the implementation references and the non-regression rules that every `FORK-*` branch must preserve.

FORK-00 adds documentation/provenance/test scaffolding only. It does not claim a new firmware feature, a new physical firmware acceptance, or G06 product equivalence.

## 1. Repository and branch authority

Implementation repository: `tiagooliveirajs/picow_ble_usb_hid_bridge_gui`.

At FORK-00 execution:

- destination `main`: `399c6b23bddd3c4404f4db194fce6cc6827c9bf3`;
- destination `main` tree: `5f7cd79af3b9afb70bafd79ffea10480b99cc751`;
- accepted Classic POC branch: `poc/rp2350-classic-hid-usb-cdc`;
- accepted Classic POC acceptance-record HEAD: `857fd66e64d7ca4c24586d962063c5d43e925eee`;
- acceptance-record tree: `d45c70d9e3a7662f99185e1cf986138f5cabc52a`;
- physically accepted Classic implementation: `b04aaf146844cead484e2a4191a4b07806bdf6f6`;
- physically accepted implementation tree: `b55608dbe45b6a6066adf0cbdc6c2d42a0678e61`;
- FORK-00 implementation branch: `fork/fork-00-baseline-freeze`, created from `857fd66e64d7ca4c24586d962063c5d43e925eee`.

`main` is intentionally not rewritten or used as a scratch branch. Later gates continue from an explicitly accepted FORK predecessor unless an explicit recorded decision changes that rule.

## 2. Physically accepted Classic HID foundation

Hardware physically proven by the operator:

- Raspberry Pi Pico 2 W / RP2350;
- Goldentec / TEC / SFIO GT T1 code 40062;
- advertised names `Bluetooth keyboard 3.0` / `BKB-3G`;
- observed address `20:20:01:60:0B:94` is evidence only and must never become a hard-coded product identity.

Toolchain/runtime provenance recorded by the accepted POC:

- Pico SDK `2.2.0`;
- BTstack `501e6d2b86e6c92bfb9c390bcf55709938e25ac1`;
- one Bluetooth runtime on Core1 with explicit 8 KiB stack in the accepted POC envelope;
- USB serviced on Core0;
- cross-core communication and flash-safe participation sufficient for BTstack TLV/link-key writes.

Durable acceptance record: `docs/BKB3G_ACCEPTED_SOLUTION.md` at `857fd66e64d7ca4c24586d962063c5d43e925eee`.

Physical acceptance at that POC is deliberately limited to discovery/pairing, successful Level-2 bonding, deferred post-bond HID startup, HID open + 263-byte descriptor, and USB press/release for `a`, `s`, `d`. It does **not** prove broad keyboard coverage, production USB identity, HAT/UI, simultaneous BLE Mouse, cold-boot Keyboard reconnect, Composite or final-product behavior.

## 3. Classic HID sequencing invariant — must never regress

The accepted GT T1/BKB-3G sequence is mandatory:

1. Bluetooth Classic inquiry.
2. Resolve remote name if it is absent from EIR.
3. Match the accepted keyboard name/capability path; do not hard-code the observed address.
4. Call `gap_dedicated_bonding(target, 0)` — Security Level 2, no MITM requirement.
5. Use SSP `NoInputNoOutput`, bonding and Just Works acceptance; do not raise the requirement to MITM to “fix” this keyboard.
6. On successful `GAP_EVENT_DEDICATED_BONDING_COMPLETED`, set the pending state, queue `start_hid_after_bonding` with `btstack_run_loop_execute_on_main_thread`, then **return from the bonding event**.
7. Only the queued callback may call `hid_host_connect(target, HID_PROTOCOL_MODE_REPORT, ...)` and start the HID/SDP path.
8. READY requires HID connection open **and** HID descriptor availability.
9. Remote report IDs/framing stay inside the Classic adapter and become logical/canonical Keyboard state before production USB-facing logic.
10. Both press and release snapshots must be preserved. CID allocation alone is never proof of successful HID operation.

Why: in the pinned BTstack, dedicated-bonding completion is emitted while the old ACL entry is still inside disconnection cleanup. `hid_host_connect` can synchronously start SDP/L2CAP. Starting it inside the bonding callback can therefore observe stale ACL state and suppress the required new connection. A guessed delay is not an acceptable substitute for the run-loop scheduling boundary.

Historical observations `0x66` (HID L2CAP security refusal) and `0x2f` (insufficient security when MITM was required) are diagnosis evidence, not requirements to increase security beyond the proven Level-2/no-MITM association.

### Executable protection

Positive host regression:

```sh
PICO_SDK_PATH="$HOME/pico/pico-sdk" \
  bash src_fw/poc_bkb3g_classic_hid_cdc/tests/run_host_test.sh
```

`run_host_test.sh` verifies the exact pinned BTstack revision, compiles the production transition code against the real BTstack headers and exercises deferred launch, duplicate/foreign/failed completions, incoming takeover, immediate failure and diagnostic behavior.

FORK-00 also makes the historical scratch negative control executable: `tests/run_negative_control.sh` copies the POC to a temporary directory, replaces the queued run-loop call with intentionally synchronous `connect_target(g_target_addr)`, compiles the same transition test, and requires the binary to fail specifically at the `old_acl_present` invariant. The scratch copy is deleted; production source is never modified by the negative control.

If the positive test stops passing **or** the synchronous negative control stops failing for the stale-ACL reason, later gates are blocked.

## 4. Physically accepted BLU2USB behavioral authority

Reference repository: `tiagooliveirajs/blu2usb`.

Accepted G06:

- branch: `gate/g06-profiles-remap-logitech-hidpp`;
- commit: `7eee024ad4ee726c5a85ffa2f32b9f47187878af`;
- tree: `3a92348b25112abf67a879307c2273a9b018bedd`;
- accepted by the historical G06 hardware qualification/PR evidence.

Normative files and frozen Git blob IDs at that SHA:

| Contract | Blob |
| --- | --- |
| `docs/product/00-product-contract.md` | `3c72c342d1858092cfce90f3d762f2b35481bc22` |
| `docs/technical/00-architecture-contract.md` | `c94e95b0acfafdb395ce76c57c327d6d4404e31b` |
| `docs/technical/02-g04-fixed-usb-validation.md` | `757bb60ab3d12b78c7bc5897ca919075fa243825` |
| `docs/technical/03-g05-canonical-hid-validation.md` | `6ce8c0bd85a7aa090a31c0d73298e1cfe9b25e18` |
| `docs/technical/04-g06-profiles-remap-hidpp-validation.md` | `461038fe569f34dc7c33e8b4f587e929b5d8a0b9` |
| `docs/ux/00-interaction-visual-contract.md` | `1a3818e08e0b071548391032e31073db9c77cc16` |
| `docs/ux/01-screen-layouts.md` | `7f65a309c7df23adfdc4cff4b238c9f457d575f3` |

These immutable files remain authoritative even when this summary is shorter than their literal content.

## 5. G06 no-regression ledger

Every later FORK gate inherits all applicable items below.

### 5.1 Fixed USB product identity

From boot, independently of Bluetooth state:

- VID `0xCAFE`;
- PID `0x4010`;
- revision `0x0100`;
- interface 0 = HID Mouse;
- interface 1 = HID Keyboard;
- manufacturer `BLU2USB`;
- product `BLU2USB Mouse + Keyboard`;
- no CDC, MSC, MIDI or vendor production interface;
- no runtime `tud_disconnect()` / `tud_connect()` re-enumeration workaround.

The Keyboard interface exists even with no physical Keyboard so Mouse remapping can synthesize Escape. Pairing, connection changes, profiles and lock/unlock do not alter USB identity.

### 5.2 Canonical HID ownership

- Transport-specific report layouts never reach USB-facing application/domain code.
- Stable internal sources distinguish physical Mouse, physical Keyboard, Composite streams and Synthetic Remap.
- Persistent button/key/modifier state is owned per source.
- Aggregate state remains held until the final owner releases.
- Duplicate press/release is idempotent.
- Disconnect, parser/runtime failure, queue overflow, profile transition and device removal release only the affected source.
- Relative X/Y/wheel/pan are transient.
- Profile changes clear stale Mouse/synthetic ownership before the new mapping becomes authoritative.
- Locking the LCD does not stop Bluetooth/USB forwarding.

### 5.3 BLE HOGP Mouse

- Accept only a peer whose Report Map includes a Mouse Application collection.
- Decode buttons, signed relative X/Y, vertical wheel and Consumer AC Pan.
- Normalize accepted framing and reject malformed/truncated shifted framing.
- Connection/security/HIDS/report failure must recover rather than trap the product loop.
- Disconnect while held cannot leave a stuck host button.
- Generic/non-Logitech Mouse behavior remains valid when HID++ probing is unsupported.
- HAT/UI remain responsive during Bluetooth activity.

### 5.4 Frozen profile mappings

| Source | Passthrough | Default | Escape |
| --- | --- | --- | --- |
| Left | Left | Forward | Escape |
| Right | Right | Backward | Backward |
| Middle | Middle | Middle | Forward |
| Forward | Forward | Left | Left |
| Backward | Backward | Right | Right |

Relative motion/wheel/pan always remain passthrough.

A profile is successful only after runtime acceptance plus required persistence; the UI never optimistically claims success.

### 5.5 CustomTemplate

- One persistent global CustomTemplate.
- Sources: Left, Right, Middle, Forward, Backward.
- Targets: Left, Right, Middle, Backward, Forward, Escape.
- Editing works without an active/saved Mouse.
- `APPLY AND BACK` updates the draft immediately and the edit page must reflect it immediately.
- The accepted dirty draft persists even before `APPLY CUSTOM`; after reboot it remains dirty/unapplied while the last actually applied profile remains active.
- Applying a preset does not erase the Custom draft.
- `APPLY CUSTOM` may show success only after complete runtime+persistent commit.
- Per-Mouse Custom profile references the global template in v1.

### 5.6 Product persistence and bonded Mouse reconnect

Product state is versioned, integrity-protected and power-loss safe using two alternating generations/slots or an evidence-equivalent strategy. Corrupt/torn newest data falls back to a prior valid generation. Product records and BTstack credentials remain distinct.

Persist/restore at least confirmed profile, committed CustomTemplate, dirty unapplied Custom draft and state needed for UX/HID++ correction before normal Mouse input becomes authoritative.

BTstack owns BLE security credentials. After `HCI_STATE_WORKING`, existing bonds are restored into resolving/accept-list behavior and a bounded bonded reconnect is attempted before generic discovery. Logitech Lift and generic bonded mice must be able to reconnect after Pico power cycle without fresh pairing where supported. The absent-peer attempt is bounded and falls back to generic discovery.

### 5.7 Logitech HID++

- Automatic vendor backend, never a user-facing transport/profile switch.
- `REPROG_CONTROLS_V4` feature `0x1b04`, Forward CID `0x0056` only when supported/needed.
- If Forward is remapped, acknowledged diversion must preserve true physical down/hold/up so Forward->Left drag remains held for the full press.
- Returning to Passthrough restores native Forward.
- Unsupported peers fail safe to Standard HID.
- Backward stays Standard HID unless a later explicit product decision changes it.
- Profile/disconnect transitions release stale vendor-derived ownership.

### 5.8 Live Mouse UX

Runtime `CONNECTED` / `DISCONNECTED` events are authoritative; movement is not a connection proxy.

- `MOUSE CONNECTED` is cyan when connected; `MOUSE NOT CONNECTED` is ordinary yellow when disconnected.
- `PROFILE:` reflects PASSTHROUGH/DEFAULT/ESCAPE/CUSTOM, including after reboot restore.
- `PAIR MOUSE` is cyan while connected/unselected and white while selected.
- Accessing Pair Mouse while already connected opens `MOUSE PAIRED`, not a new search.
- Connection while the search page is visible transitions immediately to `MOUSE PAIRED` without another HAT event.
- Back from `MOUSE PAIRED` returns directly to `MOUSE OPTIONS`.

### 5.9 Interaction/navigation/lock

- Actions execute on release, not initial press.
- `KEY B` is exactly one logical page Back outside HOME and Learn, even when labeled Cancel.
- HOME Back is a no-op; there is no `GO TO HOME` action.
- Hidden controls declared by the contract remain functional.
- Help owns interaction and uses `ANY KEY: BACK`; Key Y exits Help rather than locking while Help is active.
- Applied-profile feedback and `MOUSE PAIRED` do not add navigation depth; Back returns directly to `MOUSE OPTIONS`.
- Key Y locks on release where declared.
- While locked, the first complete HAT interaction from **any** control is consumed only to unlock/display-on/return HOME and may not also navigate.

### 5.10 Renderer and accepted literal corrections

- LCD 240x240, fixed 9x21 semantic grid.
- 5x7 glyph source at scale 2; glyph box 10x14.
- title origin x=7/y=8; horizontal advance 11 px.
- first standard body y=39; standard body advance 26 px.
- standard final hint y=214, bottom-anchored by 26 px.
- dark-magenta hint region starts 11 px above first visible hint.
- Learn didactic rows use 25 px vertical advance.
- title magenta; static/example body off-white yellow; resting action gray; selected/pressed white; current/success/connected cyan.
- white selection/press has priority over cyan and returns to cyan when selection moves away.
- option rows use exactly one leading space; no `>` selector.
- displayed Learn title exactly `PRESS TO LEARN A KEY` while the screen identity/HOME option remains `LEARN THE KEYS`.
- Learn `KEY A`, `KEY B`, `KEY X` begin at 1-based column 16.
- Pair Help uses `KEY X: HELP`; `KEY C: HELP` is invalid.

The exact literal screen layouts in G06 `docs/ux/01-screen-layouts.md` remain normative, including HOME, Mouse/other status, Help pages, Mouse Options, Pair Mouse/Keyboard/Composite, Mouse Paired, profile apply/applied pages, Custom edit/target/applied pages, Other Options, Saved Devices, Device Details, Remove Device and Learn The Keys.

### 5.11 G06 physical behavior classes to reproduce at FORK-09

The destination must eventually reproduce every applicable accepted G06 class on one exact candidate:

1. boot/UI regression, fixed USB identity and live Mouse state;
2. exact Passthrough mapping/feedback;
3. exact Default mapping/feedback;
4. exact Escape mapping/feedback;
5. synthetic Escape press/hold/release without stuck key;
6. Custom draft reflection, full apply and success feedback;
7. profile transition while mapped input was active without stale ownership;
8. generic/non-Logitech HID++ fail-safe;
9. Logitech Lift Forward->Left held drag;
10. Passthrough removes diversion/restores native Forward;
11. remap continues while LCD is locked and unlock is consumed;
12. USB identity remains fixed across profile changes;
13. profile, exact Custom mapping and dirty unapplied Custom draft survive power cycles;
14. Logitech Lift and generic bonded Mouse reconnect after Pico power cycle, with bounded generic fallback when the peer is absent.

## 6. G07 and historical work: research-only

The following are not accepted product baselines and may be used only for research, lessons, tests or cause analysis:

- BLU2USB `gate/g07-validated-classic-from-g06` / `cd74f74c4662cf26131f2f1e8d3d17482d1632ce` — integrated candidate whose product physical acceptance was still pending;
- older G07 history including `f14c108c5f053486305753411c652643f3482f42` and `8af57762db553518cf23f0ec2840c58dbb1c4fc9`;
- historical Pico-08 references `6b09417`, `8fbb36f`, `208a487` when available in repository history.

Do not wholesale cherry-pick a rejected/pending G07 branch and call it migration success.

## 7. Reconstruction architecture frozen for later gates

The destination reconstruction intentionally preserves G06 **observable behavior**, not necessarily every G06 implementation detail:

- Core0: TinyUSB and production application/HAT/LCD servicing.
- Core1: exactly one CYW43/BTstack lifecycle owner for BLE + Classic, with explicit stack no smaller than the accepted 8 KiB envelope unless later evidence deliberately changes it.
- One coherent Bluetooth runtime; no competing stacks/poll loops/hidden transport owners.
- Compatible dual-mode BTstack configuration across Bluetooth translation units.
- Bounded cross-core command/status/input channels; full snapshots preserve releases; overflow fails release-safe.
- Product flash writes coordinated with BTstack TLV/link-key activity.
- `usb_hid` is the only production TinyUSB descriptor/report owner.
- UI emits application commands and never directly owns Bluetooth primitives.
- Canonical aggregation is the sole held-state authority.
- Product persistence is distinct from Bluetooth credential persistence.
- Logitech/vendor quirks remain behind adapters.
- Production has no diagnostic USB CDC, debug PID/personality, UART diagnostic dependency, debug-only LCD page or alternate debug UF2 product identity.

## 8. Evidence and physical-test policy

Every later gate records exact predecessor/base SHA, final candidate SHA, toolchain, commands and results.

A physical gate cannot be accepted by an agent, mock, emulator or host test. For every physical candidate the executor must supply:

1. exact `.uf2`;
2. exact source SHA;
3. UF2 SHA-256;
4. board/build/toolchain metadata;
5. enumerated physical scenarios.

Only the operator’s reported results can close physical acceptance. A failure keeps the same gate open; after remediation, affected scenarios and inherited high-risk regressions are rerun on the replacement candidate.

FORK-00 itself normally needs no physical test because it does not change runtime firmware behavior.

## 9. FORK-00 completion boundary

FORK-00 is complete only when:

- all immutable references above have been verified;
- `fork/fork-00-baseline-freeze` exists from the accepted POC acceptance head;
- destination `main` is unchanged;
- this durable baseline exists in the destination repo;
- the positive host transition regression remains executable;
- the synchronous-start scratch negative control is executable and is automatically included in the host regression;
- CI exercises both controls against Pico SDK 2.2.0 / the pinned BTstack revision;
- the Markdown planner records the exact evidence and marks FORK-00 ACCEPTED.

Completion authorizes **FORK-01** but does not execute it.
