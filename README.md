# M5StopWatch wararyoLauncher

[M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm) の軽量ホストを目指すランチャーです。  
UserDemo に代わる、より軽く電池の持つホストを自作することが目的です。

## 目標

| 項目 | 目標 |
|---|---|
| 消費電力 | 画面OFF時 20 mA、アイドル時 40 mA (UserDemo は常時 80 mA) |
| 描画性能 | 操作中は常に30 fpsを上回る (UserDemo は体感10 fps以下になることがある) |
| ホーム画面 | 時計。WatchFace は容易に拡張可能な設計とする |
| 描画 | LVGL を使わず素の M5GFX のみ |
| 無線 | 基本的に使用しない |

時計画面から上にスクロールするとアプリ一覧が現れます。  
アプリとしてはアラーム、タイマー、ストップウォッチ、バッジ、
設定、外部アプリ1〜3(`ota_1` から `ota_3` を起動) を想定しています。

## リポジトリの構成

| 場所 | 内容 |
|---|---|
| `measurement/` | 測定用の試作。描画性能と消費電力を実機で測るためのもの |
| `icons/` | アプリ一覧アイコンの原本。埋め込み資産は `tools/build_icons.py` で生成する |
| `src/`、ルートの `platformio.ini` | MultiFirmホスト用の製品構成。Runtime・共通入力・消灯復帰、時計と5項目一覧、設定、MultiFirm連携 |
| `tools/`、`tests/` | 製品用書き込み保護、ビルド検証、PC上のテスト |
| `docs/plan.md` | 測定結果と合意事項を統合した製品版設計書・実装計画 |
| [docs/architecture.md](docs/architecture.md) | 現行の全体構成、機能の追加方法、差分描画、期限管理と省電力の仕組み |
| `docs/measurements.md` | 実機測定の結果と、踏んだ罠の記録 |

本体は作業3の時計・一覧・差分描画まで実装し、2026-09-20にユーザーによる実機確認を完了しました。性能基準の達成判定は、操作区間を分けた再測定を作業7で行います。作業2は2026-09-20に修正版の実機確認を完了しました。[設計書・実装計画](docs/plan.md) に、初版の範囲、責務分離、
入力・描画・電源・MultiFirm連携、受け入れ基準をまとめています。

## 現状

2026-09-23に初版ファームウェアの基準測定を行いました（作業7）。以下は製品版の実測です。

| 項目 | 結果 | 目標との関係 |
|---|---|---|
| ストップウォッチ実行中 | 描画平均1.8ms、実fps 39.98〜40.00 | 達成 |
| 時計↔一覧の遷移 | 描画平均16.0ms、実fps 35.1〜37.2 | 達成 |
| アプリ一覧スクロール | 描画平均30.1〜30.4ms、実fps 28.7〜29.8 | **30fps未達** |
| 入力→表示 / 消灯→復帰 | 平均31.2ms / 平均24.9ms | 達成（目標100ms / 300ms） |
| 時計の静止時 | 1フレームも描画しない | 達成 |
| アイドル時の消費電力 | 36.0mA（満充電・非充電中のUSB側測定、目視精度約±2mA） | アイドル40mA目標は達成 |
| 画面OFF時の消費電力 | 35.5mA（同条件） | **20mA目標は未達** |
| 最大速度の一覧スクロール | 55.0mA | 試作と同値 |

一覧スクロールだけが「操作中は常に30fpsを上回る」に届いていません。操作停止時間では
説明できず（休止は3窓中2窓で0件）、変更領域がほぼ全画面になる遷移が16msで収まっている
ことから全画面転送だけでも説明できません。原因の内訳は未測定で、最適化は別作業とします。

画面OFF時20mAは未達で、残る改善余地は描画ではなく CPU とスリープにあります。
CPU は 240 MHz のまま light sleep に入っていません。電池側の電流とUSB側の電流は
直接比較せず、電池駆動の稼働時間は別に評価します。

**電力を測るときはシリアルポートを開かないでください。** ホストがCDCポートを開くと
全条件に +16mA 以上が乗り、閉じても本体をリセットするまで戻りません。

詳細は [docs/measurements.md](docs/measurements.md)（試作と製品版の両方）と
[作業7の検証記録](docs/task7/task7-validation.md) を参照してください。

## ビルド

製品用はルートで `pio run -e m5stopwatch`、検証は `python tools/verify_build.py` を実行します。
通常のupload・erase・uploadfs・uploadfsotaは拒否されます。更新にはMultiFirmの `install-host` を使います。
[製品用ビルド・導入・復旧](docs/task1/product-build.md) と [作業1の検証記録](docs/task1/task1-validation.md) を参照してください。
2026-09-20にユーザーによる実機へのインストール・起動・待機確認を終え、作業1は完了しました。

作業2のPCテストは `python tools/test_runtime.py`。過負荷診断版は
`pio run -e m5stopwatch-diagnostics` でビルドします。
[作業2の構成・実機手順](docs/task2/runtime.md)と[検証記録](docs/task2/task2-validation.md)を参照してください。

作業3の描画検証版は `pio run -e m5stopwatch-render-check`。通常版では時刻・電池を不明表示とし、
描画検証版だけに合成データを注入します。
作業7の基準測定には `pio run -e m5stopwatch-measure` を使います。こちらは実時刻・実スロット・
保存済み設定のまま `[RenderDiag]` の集計だけを出す製品構成で、合成データもピクセル検証も含みません。
作業8の電池駆動の測定には `pio run -e m5stopwatch-drain` を使い、記録は `python tools/battery_drain.py COM11 <CSV>` で吸い出します（[作業8の詳細計画](docs/task8/plan.md)）。
長時間のログ取得は `python tools/capture_serial.py COM11 <秒> <出力ファイル> [reset]`。[構成・操作・実機手順](docs/task3/rendering.md)と
[検証記録](docs/task3/task3-validation.md)を参照してください。

作業4で実時刻・設定保存・設定画面を、作業6で `ota_1`〜`ota_3` の走査と起動を実装しました。
作業6は作業5（ストップウォッチ）より先に実装し、2026-09-22にユーザーによる実機確認を完了しています。
そのあと、起動できるスロットは確認画面を挟まず直接起動する仕様へ変え、これも実機で確認しています（全体計画8.2節）。
[時計・設定保存・設定画面](docs/task4/settings.md)、[MultiFirmホスト連携](docs/task6/multifirm.md)と
各検証記録を参照してください。

作業5のストップウォッチは2026-09-22に実装・PC検証・ユーザーによる実機確認を完了しました。
描画検証版は実機で `checks=229 mismatches=0 result=PASS`、過負荷診断版は100秒でWDTなしです。
UIは M5StopWatch-UserDemo のストップウォッチに合わせ、左右2つのボタンをAとBに割り当てました。
この画面だけは「Aが次へ、Bが決定」に従いません。A/Bの意味は画面ごとに決めてよく、
短押しのリリース確定とA+Bのホームだけが全画面共通です（全体計画5.1・5.3節）。
作業6が残していた計測連携の2項目も同じ実機確認で消化したので、**作業6もこれで完了**です。
1時間以上の長時間計測だけは未実施で、作業7へ送りました。
[ストップウォッチ](docs/task5/stopwatch.md)と[検証記録](docs/task5/task5-validation.md)を参照してください。

これで作業1〜6が揃い、初版の機能は出そろいました。
作業7では測定用の `m5stopwatch-measure` を用意して基準測定と9章の受け入れ確認を行い、
一覧スクロールの30fps未達と画面OFF時20mA未達を記録しています。
[作業7の詳細計画](docs/task7/plan.md)と[検証記録](docs/task7/task7-validation.md)を参照してください。

一覧のアイコンは `icons/*.png`（44×44のグレースケールマスク）から
`python tools/build_icons.py` で `src/features/launcher/icons/AppIcons.bin` を作り、円の上に重ねて描きます。
アイコンを差し替えたときだけ実行し直します。

日本語表示にはGenShinGothic 28pxの部分集合を埋め込みます。TrueTypeフォントはリポジトリに含めないため、
フォントや文字集合を変えたときは `pip install freetype-py` のうえ
`python tools/build_font.py --source <GenShinGothic-Medium.ttf>` で
`src/ui/graphics/fonts/GenShinGothicMedium28.vlw` を作り直します。

試作のビルドと書き込み手順、測定モードの使い方は
[measurement/README.md](measurement/README.md) にあります。
