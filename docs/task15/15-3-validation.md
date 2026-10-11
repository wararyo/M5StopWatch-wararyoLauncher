# 15-3 検証記録: 描画・文言・フォント

2026-10-10。[作業15の計画](plan.md) 5章の15-3。状態: 完了。

## 変更

| 対象 | 内容 |
|---|---|
| `TutorialLayer`・`TutorialLayout.h`（新規） | 日時の段階は `SettingsEditorView`（設定と同じ見出し「日時」・ボタン）。案内の段階は、上端の帯の中の下向きの印（ライム）、見出し（灰）、3行の本文（白）。段階の切り替えと開閉は全面再描画 |
| `FrameComposer`・`HostRenderer` | `FrameLayer::Tutorial`（歩数計の次、ホームジェスチャーの帯の下）、可視性、下地の色、準備 |
| `Strings.h` | `TutorialTitle`・`TutorialLine1〜3`（日本語・英語、計画2.6の案のまま） |
| `GenShinGothicMedium28.vlw` | 作り直した。355字→362字（方 画 面 上 端 下 押）。186,321 bytes、SHA-256 `d5116099f9bf1dcc16cadb0a25c547e976d3fba17d09f4b6f5ec5a5e6af14b1f` |
| `RenderDiagnostics` | チュートリアルの画素比較16件と画面写真3枚。画面写真の処理を関数 `shootPictures()` に分けた（下記） |

フォントの元は `GenShinGothic-Medium.ttf`（genshingothic-20150607、SHA-256 `772181b6…09ce`。[task10/fonts.md](../task10/fonts.md) と一致）。
作り直す前の資産と比べ、共通の355字は寸法の差0、画素の差0（[比較ログ](20261010-15-3-font-compare.log)）。

## 確認

| 項目 | 結果 | 記録 |
|---|---|---|
| ホストテスト（日本語・英語） | 全スイート成功。合成のテスト（`ui_tests`）にチュートリアルの層を足した | [20261010-15-3-host-1.log](20261010-15-3-host-1.log) |
| 描画検証（実機） | `checks=1385 mismatches=0 result=PASS`（15-1の1369件＋16件）。日時の段階、フォーカスと値の変化、保存のトーストと段階の切り替え、案内の段階、帯（30px・80px）と帯が消えた後、案内から日時へ戻る切り替え、時計へ戻る。すべての固定文字列がフォントに収録されていること | [20261010-15-3-render-check-2.log](20261010-15-3-render-check-2.log) |
| 画面写真 | 日時の段階、案内の段階、帯80pxが重なった案内の段階（下記） | [15-3-shots](15-3-shots/) |
| 製品版・英語版・描画検証版のビルド、`verify_build.py` | 成功。製品版を実機へ書き込み、起動を確認 | [20261010-15-3-build-m5stopwatch-2.log](20261010-15-3-build-m5stopwatch-2.log)、[20261010-15-3-verify-m5stopwatch-2.log](20261010-15-3-verify-m5stopwatch-2.log)、[20261010-15-3-build-m5stopwatch-en-2.log](20261010-15-3-build-m5stopwatch-en-2.log)、[20261010-15-3-build-m5stopwatch-render-check-2.log](20261010-15-3-build-m5stopwatch-render-check-2.log)、[20261010-15-3-product-boot.log](20261010-15-3-product-boot.log) |

### 1回目の描画検証の不一致

1回目は `tutorial-guide-band` の1件が不一致だった（4画素、印の先端の下の1行。[ログ](20261010-15-3-render-check.log)）。
印を `Gfx::drawWideLine` で描いていたため。これは自分の外接矩形でクリップを置き換えて終わりに外すので、フレームの範囲の外へ描く（`ui/graphics/Gfx.h` の注意書き）。
`drawWideLineClipped`（`ui/graphics/Shapes.h`）に替えて一致した。

### 描画検証版のスタック

描画検証は 8KiB のメインタスクのスタックで動き、余裕は14-4から380 bytesしかない（`stack_free=380`。15-1・15-3の通常の描画検証版も同じ値で、今回の変更では減っていない）。
画面写真を有効にした版（`-DLAUNCHER_RENDER_SHOTS`）は、検証の途中（Forest の幕の測定）でスタックが溢れて再起動を繰り返した。
画面写真の処理を `runRepaintCheck` のフレームから出すため、`[[gnu::noinline]]` の関数 `shootPictures()` に分け、その中の `FrameModel` を static にした。
それでも、書き込み後の最初の起動では同じ箇所で1回溢れ、リセット後の2回目で最後まで通った（[ログ](20261010-15-3-render-shots-3.log)、282行目と681行目）。
描画検証版は時間に依存して（分の変わり目など）深い経路を通るため、限界の近くで結果が揺れる。製品版には関係しないが、描画検証を足すと溢れやすいので、`runRepaintCheck` の分割を別途検討したい。

## 画面写真

| 日時の段階 | 案内の段階 | 案内の段階（帯80px） |
|---|---|---|
| ![](15-3-shots/tutorial-date.png) | ![](15-3-shots/tutorial-guide.png) | ![](15-3-shots/tutorial-guide-band.png) |

## 記録の手順

- フォント: `python tools/build_font.py --source <GenShinGothic-Medium.ttf>`（[ログ](20261010-15-3-font-build.log)）。
- 描画検証版は `install-host` で書き込み、`tools/capture_serial.py COM11 420 <ログ> reset` で記録した。2回目の記録は、結果を出した後にシリアルの読み取りが失敗して終了コード1になったが、結果は出ている。
- 画面写真は `PLATFORMIO_BUILD_FLAGS=-DLAUNCHER_RENDER_SHOTS` で作り、600秒記録して `tools/render_shots.py` で復号した（[ログ](20261010-15-3-render-shots-decode.log)）。
- 画面写真の版は `.pio/build` を消すので、最後に3つの版を作り直した（`-2` のログ）。このとき、`drawWideLineClipped` に替えた直後の描画検証版のビルドログ（同じ名前の `-2`）を上書きしてしまった。描画検証の結果のログ（`render-check-2.log`）は残っている。
- 製品版は `install-host` で書き込んだ（[20261010-15-3-install-product.log](20261010-15-3-install-product.log)）。実機には `tutorial` の記録がなく、時刻は設定済みなので、起動すると案内の段階が開く。
