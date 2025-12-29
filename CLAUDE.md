# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ZMK firmware configuration for the Agar Mini BLE keyboard. This is a user config repository that uses GitHub Actions to build firmware automatically.

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
├── agar_mini_ble.keymap  # Main keymap definition
├── info.json             # ZMK Studio layout metadata
└── west.yml              # Zephyr west manifest (defines ZMK dependencies)
build.yaml                # GitHub Actions build matrix
```

## Keymap Development

Edit `config/agar_mini_ble.keymap` to modify key bindings. The keymap:
- Uses devicetree syntax (`.dtsi` includes)
- Imports common definitions from `./boards/shields/klink_kbd/common.keymap`
- Supports mouse movement (`mmv`) and click (`mkp`) behaviors
- Has 8 layers (default + layers 1-7)

### Key Behavior Reference

- `&kp KEY` - Key press
- `&lt LAYER KEY` - Layer-tap (hold for layer, tap for key)
- `&mt MOD KEY` - Mod-tap (hold for modifier, tap for key)
- `&trans` - Transparent (pass through to lower layer)
- `&mmv MOVE_*` - Mouse movement
- `&mkp LCLK/RCLK` - Mouse click
