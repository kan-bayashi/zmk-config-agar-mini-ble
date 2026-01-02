# Deep Sleep / Soft Off 復帰問題の調査結果

日付: 2026-01-02

## 概要

Agar Mini BLE（klink）ボードにおいて、Deep SleepおよびSoft Offからの復帰が長時間後に不安定になる問題を調査した。

## 結論

**ハードウェア設計上の制約により、shifter経由のCol GPIOではスリープからの復帰が困難**

## ボード構成

```
MCU (nRF52840)
    │
    │ SPI
    ▼
Shifter (74HC595) ──→ Col 0〜11 （12列全部）

    │ 直接GPIO
    ▼
Row 0〜3 （GPIO0.30, 0.31, 0.29, 0.2）
```

- Col側: 全てshifter（74HC595）経由、SPIで制御
- Row側: MCUのGPIOに直接接続
- ダイオード方向: `col2row`

## 問題の原因

### 1. SPIの停止

Deep Sleep / Soft Off時にMCUの周辺機器（SPI含む）が停止する。これによりshifterへの制御信号が途絶え、Col出力が不定（フロート）状態になる。

### 2. ダイオード方向の制約

```
【正常動作時】
Col=LOW ──▶|── [SW押下] ── Row=LOW (検出可能)

【スリープ時】
Col=フロート ──▶|── [SW押下] ── Row=??? (検出不可)
```

ダイオードが`col2row`方向のため、電流はCol→Rowにしか流れない。ColがLOWを出力しないと、キーを押してもRow側の状態が変化しない。

### 3. ZMKの技術的制限

ZMK Issue #2932によると、shifterをwakeupソースのGPIOとして使用すること自体が技術的に困難：

```
ERROR: /wakeup_source PRE_KERNEL_2 1 < /soc/spi@40004000/595@0 POST_KERNEL 29
```

- wakeup機能: `PRE_KERNEL_2`（早期初期化）
- shifter: `POST_KERNEL`（後期初期化）
- → 初期化順序の依存関係エラー

参考: https://github.com/zmkfirmware/zmk/issues/2932

## 影響範囲

- **全48キー**が同じ問題を抱える
- 特定のキーを変更しても解決不可
- ソフトウェアのみでの完全解決は困難

## 現在の設定

### klink.conf
```conf
CONFIG_ZMK_SLEEP=y
CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000  # 1時間
```

### common.overlay (wakeup関連)
```devicetree
wakeup_scan: wakeup_scan {
    compatible = "zmk,kscan-gpio-matrix";
    wakeup-source;
    diode-direction = "col2row";
    col-gpios = <&shifter 2 GPIO_ACTIVE_LOW>;  // ← 問題箇所
    row-gpios = <&gpio0 31 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
};

kscan0: kscan_keys {
    compatible = "zmk,kscan-gpio-matrix";
    wakeup-source;
    // col-gpios: 全てshifter経由
    // row-gpios: 直接GPIO
};
```

## 対策案

### 案1: Deep Sleep / Soft Offを無効化（推奨）

最も確実な方法。電力消費は増えるが動作は安定する。

```conf
CONFIG_ZMK_SLEEP=n
CONFIG_ZMK_PM_SOFT_OFF=n
```

### 案2: リセットボタンで復帰

スリープからの復帰にはリセットボタンを使用する運用。不便だが動作は確実。

### 案3: ハードウェア改造

shifterの出力ピンにプルダウン抵抗を追加し、スリープ時でもColがLOWに固定されるようにする。要はんだ作業。

### 案4: row-gpiosのdirect scan（効果不明）

```devicetree
wakeup_scan: wakeup_scan {
    compatible = "zmk,kscan-gpio-direct";
    wakeup-source;
    input-gpios
        = <&gpio0 30 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
        , <&gpio0 31 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
        , <&gpio0 29 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
        , <&gpio0  2 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>
        ;
};
```

ダイオード方向の問題により効果がない可能性が高い。

## ZMK低電力状態の比較

| 状態 | 消費電力 | 復帰方法 | このボードでの問題 |
|------|---------|---------|------------------|
| Idle | 中 | 任意のキー | なし |
| Deep Sleep | 低 | wakeup-source設定キー | shifter依存で不安定 |
| Soft Off | 最低 | 指定GPIOのみ | shifter依存で不安定 |

参考: https://zmk.dev/docs/features/low-power-states

## 備考

- このボード（KBDFans Agar Mini BLE）の設計では、Deep Sleep復帰が考慮されていない可能性
- yangdigi/zmkフォークを使用しているが、この問題はZMK本体の制約に起因
