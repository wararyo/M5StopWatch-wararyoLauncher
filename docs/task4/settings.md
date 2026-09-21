# 作業4：時計・設定保存・設定画面

## 現在の操作

起動するとRTCから同期した実時刻・実日付・実電池が時計に表示されます。
RTCが2024〜2099の実在日でなければ `--:--` / `SET TIME` / `--%` のままで、設定画面から日時を入れると直ります。

- アプリ一覧で「設定」を決定すると設定メニューが開く。5行（日時・輝度・消灯時間・情報・戻る）。
- 輝度と消灯時間の行には現在値が並ぶので、開かなくても今の設定が分かる。
- Aで行を送り、Bで開く。行タップでも開く。「戻る」で一覧へ戻り、一覧の選択位置は保たれる。
- 編集画面ではAでフィールドを送り、Bで編集に入る。編集中はAが値を送り、Bで確定する。
  タッチはフィールドのタップで選択、▲▼のタップで増減。編集モードには入らない。
- 「保存」「キャンセル」はカーソルの末尾2つ。情報画面は「戻る」の1つだけ。
- A+B連続600msのホームは設定画面でも最優先で、未保存の編集と輝度プレビューを捨てて時計へ戻る。

設定メニューにアイコンはありません。新しいマスク資産を作業4の前提にしないためで、
`icons/` にPNGを足して `IconId` の末尾へ追加すれば後から付けられます。
一覧→設定はスライドせず即時に切り替わります（画面切り替えは全面再描画）。

## 構成

| 場所 | 責務 |
|---|---|
| `services/CivilTime.h` | 純粋な暦演算。JSTは固定オフセット。TZ環境変数を使わない |
| `services/TimeService.*` | RTC妥当性検証、システム時計同期、JST変換、手動保存と失敗回復 |
| `services/LauncherData.*` | `DisplayDataSource` 実装。時刻と30秒キャッシュした電池 |
| `storage/Settings.h` | 設定のPODと候補値。表示モデルがNVSに依存しないための分離 |
| `storage/SettingsStore.*` | スキーマ版付きレコードの検証・既定値・保存 |
| `storage/NvsBackend.*` | 名前空間 `launcher`・キー `config` の単一blob |
| `apps/SettingsScreen.*` | メニューと編集画面の状態機械。入力と保存だけを持ち、描画を持たない |
| `ui/SettingsLayout.h` | フィールド配置と当たり判定。描画と入力で共有する |
| `ui/SettingsLayer.*` | 設定レイヤーのplan/paint |

画面基底クラスはまだ作っていません。`SettingsScreen` は全体計画4.1節の契約と同じ形
（`enter / exit / handle / model`）の具象クラスで、作業5でストップウォッチが2つ目の実装に
なった時点でインターフェースを抽出します。

### 時刻

RTCはUTCとして読み書きし、表示と手動入力だけJSTへ変換します（全体計画7.3節）。
変換は `CivilTime.h` の暦演算だけで行い、`setenv("TZ")` / `tzset` / `localtime_r` は使いません。
試作は呼び出しのたびにTZ環境変数を差し替えていましたが、グローバル状態に依存すると
うるう日・月末・JST日付境界をPCで検証できないためです。

`TimeService::save` は 検証 → JST→UTC変換 → RTC書き込み → **読み戻し検証** → システム時計更新 の順です。
`M5.Rtc.setDateTime` は成否を返さないので、読み戻しだけが書き込みの証拠になります。
書き込みと読み戻しの間にRTCが進む分は2秒まで同一の書き込みとみなします。
どこかで失敗したらRTCを読み直して整合を回復し、回復できなければ `--:--` へ戻します。
成功として表示することはありません。

システム時計も `Hal` 経由です（`setUtcClock` / `utcClockUs`）。`settimeofday` がPCの
ツールチェーンに無いため、ここを通さないと `TimeService` のPCテストが成立しません。

### 設定の保存

既定 `nvs` パーティションの名前空間 `launcher`、キー `config` に5バイトのレコードを1つ書きます。
先頭2バイトがスキーマ版なので、未知の版は中身を信用せずに既定値へ退避できます。
構造体をそのまま書かないのはパディングを保存形式に含めないためです。

`nvs_flash_init` が `NO_FREE_PAGES` / `NEW_VERSION_FOUND` を返しても **`nvs_flash_erase()` は呼びません**。
名前空間はMultiFirmの他ファームウェアと共有しているので、消すと巻き添えになります（全体計画8.3節）。
初期化に失敗した場合はRAM上の既定値で動作します。

不正値は項目ごとに既定値へ戻すので、片方が壊れてももう片方は残ります。
書き込みに失敗したときRAMは更新しません。表示と再起動後の値が食い違わないようにするためです。
輝度は30〜255（可視性の下限30、刻み15の16段）、消灯は15/30/60/180秒の候補内だけを受け付けます。

### 輝度と消灯時間の適用

`ScreenManager::model()` が毎フレーム `SettingsStore` から読むので、キャッシュはどこにもありません。
輝度プレビューは「輝度編集画面が開いているときだけ編集中の値を返す」という
1行のルールで実現していて、キャンセル・ホーム・保存のすべてが同じ経路で元に戻ります。

`AppRuntime` は表示モデルの輝度が前回適用値と違うときだけ `Hal::setBrightness()` を呼びます。
`M5Hal::setScreenOff(false)` は輝度を触らず `wakeup()` だけを行い、復帰後の最初のフレームで
改めて適用します。消灯時間は `PowerManager::setTimeout()` に渡します（プレビューはしません）。

### 描画

設定表示中は時計と一覧の行を空矩形で登録し、既存の差分計画にそのまま消させます。
要素数は時計5＋一覧5＋通知1＋設定最大11で、`FramePlan::Capacity=32` に収まります。
▲▼は `fillTriangle` で描きます。埋め込みグリフ部分集合に依存させないためで、
アンチエイリアスは掛かりません。

設定メニューはアプリ一覧と同じ円弧配置（`ListLayout`）を、スクロール量を固定した
合成モデル経由で使います。5行×84pxは468pxに収まるので、メニューはスクロールせず
カーソルだけが動きます。アプリ一覧のスクロールと選択には一切触れません。

### フォント

設定画面の文言を追加したので、`tools/build_font.py` で部分集合を作り直しています。
307グリフ・146,646 bytes → **326グリフ・160,378 bytes**。
文言を増やしたときは必ず再実行して `.vlw` をコミットし直してください。

```powershell
python tools/build_font.py --source <GenShinGothic-Medium.ttf>
python tools/build_font.py --source <GenShinGothic-Medium.ttf> --check
```

描画検証版のグリフ収録検査（`RenderDiagnostics.cpp` の `covered`）にも固定文言を全部並べてあるので、
部分集合が追いついていなければ実機の起動検証で `FAIL missing fixed UI glyph` が出ます。

## PC検証とビルド

```powershell
python tools/build_icons.py --check
python tools/build_font.py --source <GenShinGothic-Medium.ttf> --check
python tools/test_runtime.py
python -m unittest discover -s tests -v
pio run -e m5stopwatch
python tools/verify_build.py
pio run -e m5stopwatch-render-check
python tools/verify_build.py --environment m5stopwatch-render-check
pio run -e m5stopwatch-diagnostics
python tools/verify_build.py --environment m5stopwatch-diagnostics
```

`src/CMakeLists.txt` の `REQUIRES` に `nvs_flash` を足したため、この変更を取り込んだ
最初のビルドは `.pio/build/<環境>` を削除してから実行してください。
CMakeのキャッシュが古いままだと `Couldn't find target config target-...json` で失敗します。

PCテストは4スイートです。`time_tests` が暦・JST境界・RTC保存、
`settings_tests` がレコード検証・メニュー・編集・保存/キャンセル・Runtimeへの適用を見ます。

## 実機確認手順

導入は[作業1のinstall-host手順](../task1/product-build.md)に従います。通常uploadは使いません。

1. 通常版で起動。起動ログの `[Time] rtc_read=... trusted=...`、`[Nvs] init=...`、
   `[Settings] brightness=... screen_off=...` を確認する。
   `trusted=yes` なら時計に実時刻・実日付・実電池が出る。
2. 作業3から持ち越した未確認項目を併せて消化する。28pxフォントの見た目、行の高さ、
   円形端での省略、一覧のピクセル比較、描画時間への影響
   （[作業3の検証記録](../task3/task3-validation.md) の「実機未確認」）。
3. 一覧から設定を開く。メニュー5行の文字が欠けないこと、輝度・消灯時間の現在値が出ること、
   上下端の行がベゼルに掛からないことを目視する。
4. 日時を開く。物理A/Bとタッチの両方で年月日時分を変え、保存・キャンセル・ホームを確認する。
   2月30日や9月31日を作って保存し、「日付が正しくありません」が出て保存されないことを確認する。
   うるう日（2028-02-29）は保存できることも確認する。
5. 正しい日時を保存し、時計へ戻って即時に反映されること、次の分境界でも更新されることを確認する。
   JST 0:00〜0:59 を入れて日付が正しいことも確認する（UTCでは前日）。
6. 輝度を変える。編集中に画面の明るさが即座に変わること、キャンセルとホームで元へ戻ること、
   保存後は消灯→復帰しても保存値が使われることを確認する。最小値30で画面が読めることも確認する。
7. 消灯時間を15秒にして実際に15秒で消灯すること、30秒へ戻せることを確認する。
8. 情報画面でファームウェア名・版とIDF版が出ることを確認する。
9. 再起動して輝度・消灯時間が復元され、時計も再同期されることを確認する。
10. 電池表示が30秒間隔で更新され、充電の抜き差しで `+` 表示が変わることを確認する。
    USB抜き差しで不要な点灯・描画が起きないことも確認する。
11. 描画検証版を導入し、起動検証の `[Verify] ... result=PASS` と `mismatches=0` を確認する。
    `FAIL missing fixed UI glyph` が出ないことも確認する。
    この版はRTCを読み書きしないので、日時画面は既定値を表示し保存は失敗する。
12. 過負荷診断版で60秒以上操作し、`idf_cpu1` が正でWDT・再起動がないことを確認する。
    最後に通常版へ戻す。

結果は[作業4の検証記録](task4-validation.md)へ追記します。
