# Keymap Customizations (2025-12-29)

## Custom Behaviors

### blt (Balanced Layer-Tap)

```dts
blt: balanced_layer_tap {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "balanced";
    tapping-term-ms = <200>;
    bindings = <&mo>, <&kp>;
};
```

- `&lt`の代替（flavor: balanced = QMKのPermissive Hold相当）
- 使用例: `&blt 1 SPACE`

### lock_ht (Lock Hold-Tap)

```dts
lock_ht: lock_hold_tap {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "hold-preferred";
    tapping-term-ms = <3000>;
    bindings = <&tog>, <&none>;
};
```

- 3秒長押しでレイヤートグル
- 使用例: `&lock_ht 4 0` (layer 4をトグル)

## Lock Layer System

### Layer 4 (lock_layer)

全キーが`&none`のロックレイヤー。キーボードを無効化する。

### Lock Layer ON (layer3から)

- **方法**: 左スペース位置で`&lock_ht 4 0`を3秒長押し
- layer3はmo3を押しながらアクセスするため、単一キー長押しで十分

### Lock Layer OFF (lock_layerから)

- **方法**: コンボ（mo3位置 + 左スペース）を3秒同時押し
- コンボにより誤操作を防止

```dts
combos {
    compatible = "zmk,combos";

    combo_lock_toggle {
        timeout-ms = <50>;
        key-positions = <36 39>;  // mo3 + left space
        bindings = <&lock_ht 4 0>;
        layers = <4>;
    };
};
```

## Device Switching (layer2)

klink/yangdigiフォーク提供のカスタムビヘイビアを使用。

| Key | Binding | Tap | Hold (3s) |
|-----|---------|-----|-----------|
| H | `&Device_USB` | USB接続に切替 | - |
| J | `&device_bt1 0 0` | BT Profile 0に接続 | Profile 0クリア |
| K | `&device_bt2 0 0` | BT Profile 1に接続 | Profile 1クリア |
| L | `&device_bt3 0 0` | BT Profile 2に接続 | Profile 2クリア |

## Key Position Reference

```
Row 0: TAB(0) Q(1) W(2) E(3) R(4) T(5) Y(6) U(7) I(8) O(9) P(10) \(11) BSPC(12)
Row 1: LCTRL(13) A(14) S(15) D(16) F(17) G(18) H(19) J(20) K(21) L(22) ;(23) RET(24)
Row 2: LSHFT(25) Z(26) X(27) C(28) V(29) B(30) N(31) M(32) ,(33) .(34) /(35) mo3(36)
Row 3: LALT(37) LGUI(38) SPACE(39) blt1(40) RGUI(41) lt2(42)
```

## Configuration

### Deep Sleep (agar_mini_ble.conf)

```conf
CONFIG_ZMK_SLEEP=y
CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=1800000  # 30 minutes
```
