# M5StopWatch wararyoLauncher

M5StopWatch-MultiFirm の軽量ホストを目指すランチャーです。UserDemo に代わる、
より軽く電池の持つホストを自作することが目的です。

## 目標

| 項目 | 目標 |
|---|---|
| 消費電力 | 画面OFF時 20 mA、アイドル時 40 mA (UserDemo は常時 80 mA) |
| 描画性能 | 操作中は常に30 fpsを上回る (UserDemo は体感10 fps以下になることがある) |
| ホーム画面 | 時計。WatchFace は容易に拡張可能な設計とする |
| 描画 | 全画面。LVGL を使わず素の M5GFX のみ |
| 無線 | 基本的に使用しない |

時計画面から上にスクロールするとアプリ一覧が現れます。アプリとしては
アラーム、タイマー、ストップウォッチ、バッジ、設定、外部アプリ1〜3
(`ota_1` から `ota_3` を起動) を想定しています。

## リポジトリの構成

| 場所 | 内容 |
|---|---|
| `measurement/` | 測定用の試作。描画性能と消費電力を実機で測るためのもの |
| `icons/` | アプリ一覧アイコンの原本。埋め込み資産は `tools/build_icons.py` で生成する |
| `src/`、ルートの `platformio.ini` | MultiFirmホスト用の製品構成。Runtime・共通入力・消灯復帰、時計と5項目一覧、設定、MultiFirm連携 |
| `tools/`、`tests/` | 製品用書き込み保護、ビルド検証、PC上のテスト |
| `docs/plan.md` | 測定結果と合意事項を統合した製品版設計書・実装計画 |
| `docs/measurements.md` | 実機測定の結果と、踏んだ罠の記録 |

本体は作業3の時計・一覧・差分描画まで実装し、2026-09-20にユーザーによる実機確認を完了しました。性能基準の達成判定は、操作区間を分けた再測定を作業7で行います。作業2は2026-09-20に修正版の実機確認を完了しました。[設計書・実装計画](docs/plan.md) に、初版の範囲、責務分離、
入力・描画・電源・MultiFirm連携、受け入れ基準をまとめています。

## 現状

測定用試作で描画性能の実現性と、240MHz固定時の電力基準値を確認しました。

| 項目 | 結果 |
|---|---|
| アプリ一覧スクロール | 描画平均13.6ms、最悪25.6ms。製品版のフレーム間隔は再測定 |
| 時計の静止時 | 1フレームも描画しない |
| アイドル時の消費電力 | 38mA（満充電・非充電中のUSB側測定、目視精度約±2mA） |
| 画面OFF時の消費電力 | 38mA（同条件） |
| 最大速度の一覧スクロール | 55mA（全白30fpsは62mA） |

試作の描画時間は33.3ms以内でした。製品版では入力・待機を含む実fpsも評価します。
当初のUSB側で画面OFF時20mAという目標は未達です。電池側の電流とUSB側の電流は
直接比較せず、電池駆動の消費電力・稼働時間は別に測定します。

残る改善余地は描画ではなく CPU とスリープにあります。画面OFF時でも 38 mA あり、
CPU は 240 MHz のまま light sleep に入っていません。

詳細は [docs/measurements.md](docs/measurements.md) を参照してください。

## ビルド

製品用はルートで `pio run -e m5stopwatch`、検証は `python tools/verify_build.py` を実行します。
通常のupload・erase・uploadfs・uploadfsotaは拒否されます。更新にはMultiFirmの `install-host` を使います。
[製品用ビルド・導入・復旧](docs/task1/product-build.md) と [作業1の検証記録](docs/task1/task1-validation.md) を参照してください。
2026-09-20にユーザーによる実機へのインストール・起動・待機確認を終え、作業1は完了しました。

作業2のPCテストは `python tools/test_runtime.py`。過負荷診断版は
`pio run -e m5stopwatch-diagnostics` でビルドします。
[作業2の構成・実機手順](docs/task2/runtime.md)と[検証記録](docs/task2/task2-validation.md)を参照してください。

作業3の描画検証版は `pio run -e m5stopwatch-render-check`。通常版では時刻・電池を不明表示とし、
描画検証版だけに合成データを注入します。[構成・操作・実機手順](docs/task3/rendering.md)と
[検証記録](docs/task3/task3-validation.md)を参照してください。

作業4で実時刻・設定保存・設定画面を、作業6で `ota_1`〜`ota_3` の走査と起動を実装しました。
作業6は作業5（ストップウォッチ）より先に実装し、2026-09-22にユーザーによる実機確認を完了しています。
計測に触れる3項目だけが作業5待ちです。
[時計・設定保存・設定画面](docs/task4/settings.md)、[MultiFirmホスト連携](docs/task6/multifirm.md)と
各検証記録を参照してください。

一覧のアイコンは `icons/*.png`（44×44のグレースケールマスク）から
`python tools/build_icons.py` で `src/ui/icons/AppIcons.bin` を作り、円の上に重ねて描きます。
アイコンを差し替えたときだけ実行し直します。

日本語表示にはGenShinGothic 28pxの部分集合を埋め込みます。TrueTypeフォントはリポジトリに含めないため、
フォントや文字集合を変えたときは `pip install freetype-py` のうえ
`python tools/build_font.py --source <GenShinGothic-Medium.ttf>` で
`src/ui/fonts/GenShinGothicMedium28.vlw` を作り直します。

試作のビルドと書き込み手順、測定モードの使い方は
[measurement/README.md](measurement/README.md) にあります。
