# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ZMK firmware configuration for the Agar Mini BLE 40% keyboard. This is a user config repository that uses GitHub Actions to build firmware automatically.

## Build System

Firmware is built via GitHub Actions on push, pull request, or manual dispatch. No local build is required - push changes and download the built firmware from GitHub Actions artifacts.

### Build Configuration

- **Board:** klink
- **Shield:** agar_mini_ble
- **ZMK Studio:** Enabled (via cmake-args)
- **ZMK Fork:** yangdigi/zmk (not the official zmkfirmware/zmk)

## Repository Structure

```
config/
├── agar_mini_ble.keymap    # Main keymap definition
├── agar_mini_ble.conf      # Keyboard configuration
├── agar_mini_ble.overlay   # Overlay configuration
├── info.json               # ZMK Studio layout metadata
├── west.yml                # Zephyr west manifest (defines ZMK dependencies)
└── boards/shields/klink_kbd/
    ├── common.keymap       # Shared behaviors and macros
    ├── common.overlay      # Shared overlay (wakeup config etc.)
    └── src/klink_indicator.c  # LED indicator implementation
build.yaml                  # GitHub Actions build matrix
```

## Keymap Development

Edit `config/agar_mini_ble.keymap` to modify key bindings. The keymap:
- Uses devicetree syntax (`.dtsi` includes)
- Imports common definitions from `./boards/shields/klink_kbd/common.keymap`
- Supports mouse movement (`mmv`), scroll (`msc`), and click (`mkp`) behaviors
- Has 6 active layers + 2 reserved (layers 6-7)

### Layer Structure

- **Layer 0 (default)**: Base QWERTY layout
- **Layer 1**: Numbers, symbols, mouse movement
- **Layer 2**: Navigation, device control (USB/BT switching)
- **Layer 3**: Media controls, system functions
- **Layer 4**: Lock layer (all keys disabled)
- **Layer 5**: Function keys (F1-F12)
- **Layers 6-7**: Reserved

### Key Behavior Reference

#### Basic Behaviors
- `&kp KEY` - Key press
- `&trans` - Transparent (pass through to lower layer)
- `&none` - No action

#### Layer Behaviors
- `&mo LAYER` - Momentary layer (active while held)
- `&lt LAYER KEY` - Layer-tap (tap-preferred: tap for key, hold for layer)
- `&lt_hp LAYER KEY` - Layer-tap hold-preferred

#### Modifier Behaviors
- `&mt MOD KEY` - Mod-tap (hold-preferred: hold for mod, tap for key)
- `&mt_tp MOD KEY` - Mod-tap tap-preferred
- `&gresc` - Grave Escape (ESC normally, ` with GUI/Shift)

#### Mouse Behaviors
- `&mmv MOVE_*` - Mouse movement (UP/DOWN/LEFT/RIGHT)
- `&msc SCRL_*` - Mouse scroll (UP/DOWN/LEFT/RIGHT)
- `&mkp LCLK/RCLK` - Mouse click
- `&msu`, `&msd`, `&msl`, `&msr` - Scroll shortcuts

#### Device Control
- `&Device_USB` - Switch to USB output
- `&device_bt1 0 0`, `&device_bt2 0 0`, `&device_bt3 0 0` - BT switch (tap) / clear (3s hold)
- `&bootloader_reset 0 0` - System reset (tap) / bootloader (2s hold)
- `&ht_soft_off SHOW_SOFT_OFF 0` - Soft off with LED feedback (2s hold)
- `&ht_tog LAYER 0` - Toggle layer (2s hold)

#### LED Status Keys
- `&kp SHOW_LED` (0xAB) - Show BLE connection status
- `&kp SHOW_BATTERY` (0xAC) - Show battery level
- `&kp SHOW_SOFT_OFF` (0xAD) - Trigger soft off

### Special Features

- **BT1/BT2/BT3**: Tap to switch, hold 3s to clear pairing
- **Reset**: Tap for system reset, hold 2s for bootloader
- **SoftOff**: Hold 2s to power off (red LED feedback)
- **Lock Layer**: Hold 2s to enter lock mode, combo (keys 36+40) to exit
- **Wakeup**: S key wakes from soft off (configured in common.overlay)

### LED Indicator Colors

| Event | Color |
|-------|-------|
| BT1 connected | Yellow |
| BT2 connected | Cyan |
| BT3 connected | Magenta |
| USB connected | White |
| Lock layer enter | Yellow |
| Lock layer exit | Green |
| Soft off | Red |
| Battery 80%+ | Green |
| Battery 50-80% | Cyan |
| Battery 20-50% | Yellow |
| Battery <20% | Red |
