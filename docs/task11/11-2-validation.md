# 11-2 検証記録: Analogの描画・登録・差分復元

実施日: 2026-09-29。[11-2計画](plan-11-2.md)の完了条件をホストと実機の自動描画検証で確認し、**11-2を完了とした**。
実機での手操作（設定からの選択・長押し・再起動復元）と目視の最終判断は11-5で行う。

## 1. 実装の要点

| 場所 | 内容 |
|---|---|
| `src/features/home/faces/AnalogWatchFace.h/.cpp`（新規） | WatchFace契約。黒背景（Rendererの基底）に日付・情報欄（グレー）→時針→分針→中心円（白）→秒の丸（オレンジ）の順で描く |
| `src/features/home/faces/InfoRowView.h/.cpp`（新規） | 情報欄の文字・アイコン。`plan`（計測・省略・マスク・配置・変更キー）と`paint`（単色背景への描画）を分けた。Forestの処理を移植 |
| `src/features/home/HomeLayer.h/.cpp` | `analog_`を所有し、Digital→Forest→Analogの順で登録 |
| `src/features/home/FaceSelection.h` | 描画検証版（`LAUNCHER_RENDER_DIAGNOSTICS`）だけ登録容量を6にした。製品版と設定一覧は4のまま |
| `src/CMakeLists.txt` | 2ソースを追加 |
| `src/host/HostRenderer.h`、`RenderDiagnostics.cpp` | `analogSeconds()`、Analogの画素比較・性能測定・画像取得ケース |
| `tests/watchface_tests.cpp` | 3件の選択・復元、設定一覧の3件＋戻る。未知IDの例を`analog`から`sundial`へ変更 |

計画からの補足・決定:

- **要素は8個**（日付・電池・情報0・情報1・時針・分針・中心円・秒の丸）。FramePlanのコメントにある「時計8要素」の範囲に収まる。各要素の矩形は`strokeBounds`／`placeAnalogDate`／`InfoRowView::box`、キーは`AnalogTime`の整数キーと文字列ハッシュで、同じ矩形のまま針が回る場合も変更として扱う。
- **中心円と秒の丸**は長さ0の線として`drawWideLineClipped`で描く。針と同じクリップ規則・同じ矩形計算になり、専用の円描画は追加していない。
- **日付フォント**は計画どおり既存資産（D-DIN-PRO Exp Bold 28px、`watchTextFont`）を使う。数字の高さは20pxで、参照画像（26px）より小さい。縦位置はフォントのascent（22）を墨の高さとして渡すため、約1px下寄りになる。見た目の判断は11-5の目視へ渡す。
- **文字の不透明背景**: 日付・情報欄は黒を背景色とする不透明描画。針より先に描くため、重なった針は欠けない（画素比較と画像で確認）。多色背景のNoonishでは`InfoRowView::plan`をそのまま使い、描画側を11-3で用意する。
- **一覧の被覆**: Analogは黒背景で静止するため、Forestのようなクリップ変化の帯damageは不要。要素矩形をクリップと交差させて登録するだけでよい。
- **登録容量**: 描画検証版は検証用の文字盤2つ（`test-overlap`・`test-backdrop`）を追加登録するため、Digital・Forest・Analogと合わせて5件になり、容量4では足りなかった。11-3のNoonishで6件になる。設定一覧は先頭4件だけを表示するので、検証用文字盤は一覧に出ない。
- Forestは今回変更していない。`InfoRowView`とForestの情報欄は同じ処理が2か所にある状態で、統合は作業11の範囲外とした（計画3節）。

## 2. ホスト検証

全8スイートPASS（[ログ](20260929-11-2-host-1.log)）。

| 項目 | 検証 |
|---|---|
| 3件目として登録・選択・保存・復元 | `selection`。Analogの選択で`watch_sel`だけを書き、再起動相当で3件目が復元される |
| 設定一覧の3件と「戻る」 | `settingsFaces`。Digital／Forest／Analog／戻る、Analog選択で「使用中」が移る。戻るでメニューへ |
| 未知IDの扱い | 未登録のIDを`sundial`に変更（`analog`が登録されるため） |

## 3. ビルド

| 構成 | 結果 | RAM / Flash（bytes） |
|---|---|---|
| 通常版 | SUCCESS（[ログ](20260929-11-2-build-m5stopwatch-5.log)） | 55,772 / 1,235,887 |
| 描画検証版 | SUCCESS（[ログ](20260929-11-2-build-m5stopwatch-render-check-2.log)） | 74,636 / 1,273,495 |
| 測定版 | SUCCESS（[ログ](20260929-11-2-build-m5stopwatch-measure-2.log)） | 72,900 / 1,239,987 |

通常版は11-1比でRAM +4,128 bytes、Flash +12,324 bytes。RAMの増分は主に`InfoRowView`のアイコンマスク（40×40を2つ）とAnalog本体の静的確保。4MiBの上限に対して29.5%。

- [`build-m5stopwatch`](20260929-11-2-build-m5stopwatch.log): `InfoRowView.cpp`の`Element.h`のinclude漏れでコンパイルエラー。修正後に3構成を再ビルドした（`-2`）。同時に走らせた[描画検証版](20260929-11-2-build-m5stopwatch-render-check.log)は中断したため結果なし。
- `-3`・`-4`: 画像取得版ビルドで`PLATFORMIO_BUILD_FLAGS`を変えたため、PlatformIOが構成のチェックサム変化で`.pio/build`全体を消していた。その後の通常版コールドビルドがCMake構成（`component_requires.temp.cmake`がない）で2回失敗。`-t clean`（[ログ](20260929-11-2-clean-m5stopwatch.log)）後の`-5`で成功し、`-2`と同じサイズ。

## 4. 実機の自動描画検証

COM11のM5StopWatchへMultiFirmの`install-host`（`multifirm.ps1`経由）で導入した（[導入ログ](20260929-11-2-install-render-2.log)、読み戻し一致）。
[`install-render`](20260929-11-2-install-render.log)は既定の`python`（3.13、esptoolなし）で直接実行して失敗したもので、実機には書き込んでいない。

### 4.1 画素比較

[起動からのログ](20260929-11-2-render-check.log)。**701件・不一致0件でPASS**（10-6は551件）。追加したAnalogの主なケース:

| 計画6節の項目 | ケース |
|---|---|
| 秒の丸だけの更新、10秒境界、日付更新、無効時刻、秒OFFで旧位置が消える | `analog-second`（全60位置）、`analog-step`／`-59`、`analog-new-day`、`analog-unknown`／`-known`、`analog-seconds-unknown`、`analog-seconds-hidden` |
| 丸だけが変わる秒に針を変更扱いにしない | `analog-quiet-second`: 秒非表示で:01〜:09の各秒は描画範囲が空であることを確認 |
| 針同士、針と日付・情報の交差 | `analog-angles`（0/3/6/9時、対角、10:07、針の重なり6:32）、`analog-over-date`→`analog-date-under-hands`→`analog-off-date`、`analog-step-over-row` |
| 情報増減、長い日本語、不正アイコン、電池条件変化 | `analog-battery`（31/30/29/5/0/100/不明/30）、充電・不明・読取不可、`analog-item(s)`、長いラベル、`計測中`、アイコンなし、推奨色だけの変化で描画なし（`analog-colour-only`） |
| 一覧の部分被覆と往復 | `analog-transition`（12段階、秒が進みながら）、一覧の下での情報増減・10秒境界、`analog-slow-return`（遅い戻り） |
| 文字盤切替、再初期化、容量超過 | `analog-reselected`、`analog-overflow`／`-recovery`、`analog-to-digital` |

文字盤の反復切替（Digital→Forest→Analog＋長押し×16）で内部空きは234,423→234,423 bytesと減少なし。既存の反復も減少なし。

起動55.5秒に`task_wdt`（CPU 1の`IDLE1`）の警告が出た。10-6と同じ時刻・同じ内容で、一覧遷移の比較中に発生する既知の警告。その後も検証を完了しており、画素比較の合格とは区別する。

### 4.2 性能（合成入力の描画時間）

| シナリオ | 平均 / 最大（ms） | 平均描画範囲（px） |
|---|---:|---:|
| 静止（変化なし） | 0.414 / 0.927 | 0（描画0件） |
| 秒非表示、10秒境界（両針） | 16.963 / 22.642 | 15,297 |
| 秒非表示、情報だけ毎秒 | 2.229 / 2.440 | 3,404 |
| 秒表示、丸だけ（境界を除く54秒） | 4.405 / 29.013 | 6,402 |
| 秒表示、毎フレーム10秒境界（丸＋両針） | 23.682 / 31.692 | 68,900 |
| 秒表示＋情報毎秒（60秒） | 12.365 / 26.720 | 38,744 |
| 一覧遷移 | 10.163 / 25.119 | 65,467 |
| 全面描画 | 42.806 / 42.814 | 218,088 |

- 丸と針が同時に変わる10秒境界は、FramePlanが1つの矩形に統合するため、離れた丸と針の間まで範囲が広がる（計画4節の想定どおり）。丸だけの秒とは約10倍の差。
- 丸だけの最大29.0msは54フレーム中の外れ値で、原因は未特定（平均は4.4ms）。11-5で実操作時に再確認する。
- 全面描画はForest（32.9ms）より遅い。針を画素ごとの距離計算で描くためで、全面描画は選択・復帰時に限られる。
- 作業10の「定常フレーム間隔33.3ms以内は未達」は引き継ぐ。

### 4.3 画像

`LAUNCHER_RENDER_SHOTS`版を導入し（[導入ログ](20260929-11-2-install-render-shots.log)）、[ログ](20260929-11-2-render-shots.log)から`tools/render_shots.py`で[画像](11-2-shots/)に変換した。この実行でも701件・不一致0件。

- `analog-info.png`は参照画像[Analog-1](../Images/WatchFace/Analog-1.png)と同じ6:00:00・20日・18%・2件で、針・中心円・秒の丸・情報欄の位置が一致する。時針は情報欄の`02:40`の上に欠けずに重なる。
- `analog-over-date.png`: 3:15で両針が日付`19`の上を通り、日付の不透明背景による欠けはない。
- `analog-unknown.png`: 針と秒の丸がなく、中心円・`--`・電池（不明・充電）を表示。
- `analog-mixed.png`: 長いラベルは`A ve...`、`計測中`は`計...`に省略され、アイコンなしは輪で代替。
- 日付の数字は参照より小さい（1節）。
- 最下2行の赤い線は、画像取得のプローブ（全面を赤で塗る）の残り。`d1a88fa`で描画をパネル実寸の466行に制限したため、468行バッファの末尾2行が塗り直されない。パネルには表示されない領域で、Forest・Digitalの画像にも同じ線がある。

### 4.4 製品版への復帰

製品版を導入し（[導入ログ](20260929-11-2-install-product-2.log)、読み戻し一致）、[起動ログ](20260929-11-2-product-boot.log)で保存済みの選択（Digital）と設定（輝度150）の復元、文字盤登録の拒否がないことを確認した。
[`install-product`](20260929-11-2-install-product.log)は、ビルド出力が消えていたため（3節）ファイルが見つからずに失敗したもので、実機には書き込んでいない。

## 5. 未実施・引き継ぎ

- 実機での手操作: 設定の3番目にAnalogが出ること、選択・長押し・再起動での復元、A+B・戻るでの表示。ロジックはホストで確認済み。11-5で実施する。
- 目視: 針の太さ・長さ、丸の径と軌道、日付の大きさ（参照より小さい）と情報欄の視認性。11-5で判断する。
- 丸だけの更新の最大値の外れ値と、10秒境界の広い描画範囲の実操作への影響は11-5で測る。
- 11-3は`InfoRowView::plan`の結果（矩形・ラベル・マスク）を多色背景上で描く。電池アイコンの中抜きは現在背景色で塗っているため、Noonishでは別の描き方が必要になる。
