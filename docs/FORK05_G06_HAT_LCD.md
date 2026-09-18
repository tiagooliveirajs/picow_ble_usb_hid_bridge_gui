# FORK-05 — G06 HAT/LCD interaction and renderer baseline

This gate restores the physically accepted BLU2USB G06 HAT/LCD interaction and renderer contract on the reconstructed production target without changing the accepted Classic transport, canonical ownership or fixed USB identity.

## Normative authority

Behavioral authority: `tiagooliveirajs/blu2usb` SHA `7eee024ad4ee726c5a85ffa2f32b9f47187878af`.

Normative G06 artifacts include:

- `docs/ux/00-interaction-visual-contract.md` blob `1a3818e08e0b071548391032e31073db9c77cc16`;
- `docs/ux/01-screen-layouts.md` blob `7f65a309c7df23adfdc4cff4b238c9f457d575f3`;
- `include/blu2usb/renderer/renderer.h` blob `a5f1375f094ef87b16b4883623a9b1407be37376`;
- `src/renderer/renderer.c` blob `4f1a2ea96a815c2059f2236e9cc28b6d348bcde4`;
- `src/renderer/st7789_pico.c` blob `6bff1400437fa8dccf84a12da872eb80ed78b1a6`;
- `include/blu2usb/hat/hat.h` blob `d4d11b5c7c0204938ffd0f6aafdcd1a13708e45d`;
- `src/hat/hat_pico.c` blob `0e2575a9912713fb4a1a3f23cc23a481cb2d291c`;
- `src/interaction/interaction.c` blob `d6187fe0a0eecab51346edf26655f523b5942a48`;
- `src/ux_model/ux_model.c` blob `da44d98a599daa1aade5606d5a8439f1185473a3`.

## Restored invariants

- Waveshare HAT GPIO map from G06, 20 ms debounce, actions on release.
- ST7789 240x240 on SPI1 with the accepted pin map and panel setup.
- 9x21 semantic grid, 5x7 font at scale 2, 10x14 glyphs, x=7/y=8, 11 px character advance.
- first standard body y=39, 26 px body/hint advance, final standard hint y=214, dark-magenta hint boundary 11 px above first hint, Learn 25 px row spacing.
- title magenta; static yellow/off-white; actionable light gray; selection/pressed white; current/success cyan; Learn full dark-magenta.
- option rows use one leading space and no `>` selector.
- Joy Up/Down wraps; Joy Press accesses; Key B is one-page Back outside HOME/Learn; HOME Back is no-op; no Go Home shortcut exists.
- Pair help is Key X; Help owns every HAT control and Key Y cannot lock while Help is visible.
- Key Y locks on release. First complete HAT interaction while locked unlocks, is consumed and returns HOME.
- displayed Learn title is exactly `PRESS TO LEARN A KEY`; Key A/B/X start at 1-based column 16.
- Lock is presentation-only: Core0 continues USB/app servicing and Core1 continues Bluetooth while LCD backlight is off.

## Current integration boundary

FORK-05 does not implement BLE Mouse, profiles/remap persistence or saved-device semantics. Their reserved G06 layouts are retained so later gates do not redefine navigation. Pair Keyboard and its Retry/Cancel controls are wired to the already accepted Classic façade; UI code never calls BTstack directly.
