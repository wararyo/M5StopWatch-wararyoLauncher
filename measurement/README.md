# 測定用試作

設計判断に必要な描画性能と消費電力を実機で測るための試作です。製品版の
コードではありません。ここで得た数値と失敗例をもとに本体を設計します。

測定結果は [../docs/measurements.md](../docs/measurements.md) にまとめてあります。

## 試作範囲

- M5GFX のみで描画するデジタル時計
- 時計を上へスワイプすると現れるアプリ一覧
- `A`: 次へ、`B`: 決定、`A+B` 0.6秒保持: 時計へ戻る
- 30秒無操作でパネルをOFF。タッチによる復帰操作は画面復帰だけに使う
- RTCをUTCとして読み、表示はJST固定
- CPU 240 MHz固定、無線なし、Grove 5 V出力なし

ストップウォッチ、設定、外部アプリ起動は一覧の入口だけです。MultiFirmホストAPIと
省電力PMは入っていません。

## 描画の構成

全画面スプライトは使っていません。M5StopWatch では M5GFX が
`Panel_AMOLED_Framebuffer` を組み込むため、`M5.Display` は最初から PSRAM 上の
フルスクリーンバッファとダーティ矩形追跡を持ちます。そこへスプライトを重ねると
PSRAM 転送量が 3 倍になるため、パネルへ直接描いています。

さらにフレームバッファを毎フレーム消さず、ボックスか内容が変わった要素だけが
前回の矩形を消してから描き直します。静止時は 1 フレームも描きません。

| ファイル | 役割 |
|---|---|
| `src/ui/Element.h` | 要素の矩形と内容フィンガープリント、フレーム計画 (`FramePlan`) |
| `src/ui/Renderer.cpp` | パネルへの直接描画とアプリ一覧 |
| `src/ui/DigitalWatchFace.cpp` | 時計盤。大きな時刻テキストは内蔵SRAMのスプライトにキャッシュ |
| `src/ui/RepaintCheck.cpp` | 部分再描画の自己検証とフレームコスト計測 |
| `src/power/PowerProfile.cpp` | 消費電力の台本計測 |

## ビルドと書き込み

```powershell
pio run
pio run -t upload
```

`partitions.csv` はこの試作単体用です。MultiFirm環境へ書き込む用途には使いません。

## 通常動作のログ

シリアルの `FrameStats` は5秒ごとに出ます。

```
[FrameStats] frames=190 missed_33ms=0 max_us=26699 duty=52% dragging=1
```

| 項目 | 内容 |
|---|---|
| `frames` | 5秒間に実際に描画したフレーム数。変化がなければ増えません |
| `missed_33ms` | 33.3 ms を超えた描画数。ドラッグ中に 0 であることが性能基準 |
| `max_us` | 最長描画時間 |
| `duty` | 描画に費やした時間の割合 |

`duty` は充電電流にも USB にも汚されず完全に再現するため、消費電力の代理指標に
使えます。電流計での実測を2、3点取って係数を決めれば、以降はこの数字だけで
電力を予測できます。

## 測定モード

`platformio.ini` の `build_flags` で切り替えます。どちらも既定では無効で、
有効にすると起動時に自動で走り、シリアルへ結果を出します。

| フラグ | 内容 | 所要 |
|---|---|---|
| `-DLAUNCHER_BENCH` | 部分再描画の自己検証とフレームコスト計測 | 約30秒 |
| `-DLAUNCHER_POWER_PROFILE` | 消費電力の台本計測 | 約21分 |

ビルドフラグを変更した直後の1回目のビルドは `Couldn't find target config` で
失敗します。もう一度実行すれば通ります。

### 部分再描画の自己検証

部分再描画はフレームバッファを毎フレーム消さないため、結果がそれ以前の全フレームに
依存します。目視では気付けない破綻が起きるので、スクロールと遷移を台本通りに
動かしてからフレームバッファを読み戻し、同じ状態を全面再描画した結果と
1ピクセル単位で比較します。

```
[Bench] app-list-scroll        painted=120/120 avg=13595us worst=25614us
[Verify] checks=24 mismatches=0 reference_nonblack_pixels=11834
```

`mismatches` が 0 でなければ、差分の範囲と、消し残り (`stale`) か消しすぎ
(`missing`) かが出ます。描画コードを変えたら必ず通してください。

### 消費電力の台本計測

10条件を120秒ずつ保持し、開始と終了をシリアルに出します。USBテスターの
表示を条件ごとに読みます。

```
[Power] >>> START watch-forced-30fps brightness=90 hold=120s  (mark the tester now)
[Power] <<< END   watch-forced-30fps  120016ms  steps=3636 (30.3/s, 1 over period)
                  duty=70%  avg=23360us worst=23688us
[Power]     battery=4208mV vbus=5192mV charging=0  (average mA = delta mAh * 3600 / 120)
```

積算 mAh が読めるテスターなら、開始と終了の差分から平均電流を出します。

```
平均電流[mA] = ΔmAh × 3600 / 120
```

**事前に満充電にしてください。** 充電中はテスターが充電器を見ているだけで、
本体の消費は読めません。各条件の `charging=1` はその条件が無効という印です。

条件は消費電力を3つの項に分けられるように組んであります。

| 項 | 分離する条件 |
|---|---|
| ベースライン | `screen-off` |
| パネル発光 | `fill-black-30fps` と `fill-white-30fps` の差、輝度10と255の差 |
| 描画負荷 | `watch-forced-10/20/30fps` の傾き |

## 実機で踏んだ罠

同じところで詰まらないように記録しておきます。詳細は
[../docs/measurements.md](../docs/measurements.md) を参照してください。

- **命令キャッシュ32 KBとフラッシュQIOは起動しません。** どちらも約6秒ごとに
  リセットを繰り返し、コンソール出力が一切出ません。Kconfig上は普通に選べるため
  警告も出ません。`sdkconfig.defaults` にコメントで残してあります。
- **PMICはI2C 100 kHzでないと応答しません。** 400 kHzでは全読み出しが失敗します。
  さらに `M5.In_I2C.readRegister8()` は失敗時に黙って0を返すため、レジスタが
  全部ゼロに見えて誤読します。
- **PMICに電流レジスタはありません。** 自己消費を本体から測る道はないので、
  外部計測が必須です。

## 未修正の問題

`src/main.cpp` の `vTaskDelayUntil` がフレーム時間超過時にブロックせず戻るため、
CPU1のアイドルタスクが走らずタスクウォッチドッグが発火します。フレーム時間が
下がって発火しなくなっている可能性はありますが未確認です。修正案は
[../docs/measurements.md](../docs/measurements.md) にあります。
