# 作業4：時計・設定保存・設定画面の実装計画

作成日: 2026-09-21。設計の基準は [全体計画](../plan.md) の4・5.4・7.3・8.3章と作業4。
本書は作業3の実機確認が完了した前提の計画であり、実装・検証完了の記録ではない。

## 到達点と範囲

起動するとRTCから同期した実時刻・実日付・実電池が時計に表示される。
一覧から設定を開き、日時・輝度・消灯時間を変更して保存でき、再起動後も復元される。
情報画面でファームウェア版とIDF版を確認できる。

計測は作業5、スロット走査・外部起動は作業6に残す。設定メニューに未実装項目は並べない。
一覧のストップウォッチと外部アプリ1〜3は作業3のまま「準備中」の通知を出す。
タイムゾーン設定UI、共有NVS、USB時刻設定は初版の対象外（全体計画7.3・8.3節）。

## 現状からの変更

| 現状 | 作業4での変更 |
|---|---|
| `src/main.cpp:86` が基底の `DisplayDataSource` を渡し、時刻・電池が常に不明値 | `services/LauncherData` が実RTC・実電池のスナップショットを返す |
| `src/` に `M5.Rtc`・`M5.Power`・`nvs_flash` の参照がない | `Hal` にRTC読み書き・電池・輝度を追加し、`M5Hal` で実装する |
| 輝度90が `main.cpp:44`・`M5Hal.cpp:31,32` の3か所に固定 | `SettingsStore` の値を `ScreenModel` 経由で `Hal` へ適用する |
| 消灯30秒が `PowerManager.h:17,23` の2か所に固定 | `PowerManager` の期限を設定値で可変にする |
| `ScreenId` は `Home`/`AppList` の2値、画面は `ScreenManager` の分岐のみ | 設定画面群を `apps/SettingsScreen` へ分離し、`ScreenManager` が委譲する |
| 一覧の行数5が `ListLayout`・`Renderer`・`ScreenManager` に固定 | `ScreenModel::rowCount` で可変化する。配列容量5は据え置く |
| `Renderer::draw` が時計＋一覧を固定で計画 | 設定レイヤーを追加し、設定表示中は時計・一覧を空矩形で計画する |

作業2・3の入力・電源・差分描画の規約は変更しない。試作 `measurement/` は基準測定用に保持する。

## 画面の抽象について

全体計画4.1節は内蔵アプリ画面の契約（入場・退場・意味イベント・表示モデル・更新期限）を定めるが、
現在のコードに画面基底クラスはなく、`ScreenId` の分岐だけで表現されている。
作業4では基底クラスを作らない。実装が1つしかない段階で抽象を先に立てず、
`SettingsScreen` を同じ形（`enter / exit / handle / model / nextUpdate`）の具象クラスとして作る。
作業5でストップウォッチが2つ目の実装になった時点でインターフェースを抽出する。

## 実装順序

### 4-1. 時刻基盤

- `services/CivilTime.h` に純粋な日付演算を置く。`civilToUnix` と `unixToCivil`（曜日を含む）を
  実装し、`setenv("TZ")` / `tzset` / `localtime_r` を使わない。
  試作 `measurement/src/time/TimeService.cpp` は呼び出しごとにTZ環境変数を差し替えるが、
  グローバル状態に依存せずPCで単体検証できる方式へ置き換える。JSTは固定オフセット `+9h` の定数1つ。
- `Hal` へ `readRtc(std::tm& utc)` / `writeRtc(const std::tm& utc)` / `sampleBattery()` /
  `setBrightness(int)` を追加する。`M5Hal` は `M5.Rtc`・`M5.Power`・`M5.Display` で実装し、
  読み取り失敗を0や0%と解釈しない。RTCへはUTCとして読み書きする（全体計画7.3節）。
- `services/TimeService.{h,cpp}`
  - `begin(Hal&)`: RTC読み出し → 範囲と実在日の検証（年2024〜2099、うるう日を含む）→
    UTCとして `settimeofday` → 有効性を確定する。無効なら同期せず `valid()==false` にする。
  - `now(std::tm& jst, TimeUs& subsecondUs)`: システム時計へ `+9h` して `unixToCivil` する。
    毎描画でRTCを読まない（全体計画7.3節）。
  - `save(const std::tm& jstInput)`: 検証 → JST→UTC変換 → `writeRtc` → `settimeofday` →
    RTC再読み出しで整合確認。途中失敗を成功として返さない。回復できなければ無効状態を明示する。
    戻り値は `Saved / Invalid / RtcWriteFailed / Unrecoverable` とし、画面が結果を出し分ける。
    秒を0へ合わせるのは `save()` の責務（全体計画5.4節）。

確認: 月末、うるう日2028-02-29、年跨ぎ、UTC15:00のJST日付境界、範囲外年、不実在日2026-02-30、
書き込み失敗、再読み出し不一致をPCで検証する。

### 4-2. 設定保存

- `storage/SettingsStore.{h,cpp}` は純粋なロジックに保つ。
  レコードはスキーマ版・輝度・消灯秒の単一構造体とし、既定は輝度90・消灯30秒。
  輝度は可視性を保てる下限30から255、消灯は15/30/60/180秒の候補内で検証する。
  未知の版・範囲外・読み出し失敗は診断を出してRAM上の既定値で動作する。
- バックエンドは `load` / `save` だけのインターフェースにする。PCテストはメモリ実装、
  製品は `storage/NvsBackend.{h,cpp}` が既定 `nvs` パーティションの名前空間 `launcher` へ
  単一blobとして読み書きする。1レコードにまとめ、確定時だけ1回書く。描画・毎秒処理では書かない。
- `nvs_flash_init()` は `main.cpp` のRuntime開始前に呼ぶ。`ESP_ERR_NVS_NO_FREE_PAGES` /
  `ESP_ERR_NVS_NEW_VERSION_FOUND` でも **`nvs_flash_erase()` を呼ばない**（全体計画8.3節）。
  失敗はログに残して既定値で起動する。`src/CMakeLists.txt` の `REQUIRES` に `nvs_flash` を足す。

確認: 既定値、往復保存、未知の版、範囲外、load失敗、save失敗をPCで検証する。

### 4-3. 表示データの接続

- `services/LauncherData.{h,cpp}` が `DisplayDataSource` を実装し、`main.cpp:86` の
  `static launcher::DisplayDataSource data;` を置き換える。
  - `sample(now)`: JST時刻と有効性、30秒キャッシュした電池値・充電状態を `WatchData` で返す。
    期限を過ぎていればこの中で `Hal::sampleBattery()` を1回だけ呼ぶ。I2CはUIタスクからのみ。
  - `nextUpdate(now)`: 電池の次回取得期限（前回＋30秒）。分境界は
    `DigitalWatchFace::nextUpdate` → `Renderer::nextUpdate` として既に接続済み。
- `AppRuntime.cpp:42-45` の既存の期限合成をそのまま使う。設定保存や復帰の即時更新は
  既存の `AppRuntime::dataChanged()` と `renderer_.invalidate()` で行い、新しい経路を足さない。
- 画面OFF中は描画も期限も止まる既存挙動を維持する。復帰時は無効化後の再サンプルで最新値になる。
  システム時計を読むのでRTCアクセスは復帰経路に入らない。

確認: 電池の30秒期限、無効時刻時の `--:--`、消灯中に電池取得もRTC読み出しも起きないことを検証する。

### 4-4. 設定画面の状態機械

- `ScreenId` に `Settings` を追加し、表示モデルへ設定用の構造体を持たせる。
  画面種別（メニュー／日時／輝度／消灯時間／情報）、カーソル、編集値、編集中フラグ、
  プレビュー設定、通知を含める。M5GFXに依存させない既存の方針を守る。
- `ScreenManager` はホーム・ドラッグ・遷移・通知の責務を維持し、設定表示中は
  `SettingsScreen` へ委譲する。一覧で `AppId::Settings` を決定したら入場する。
  `e.home` は既存どおり `ScreenManager` が先に処理し、退場で未保存編集と輝度プレビューを破棄する。
- メニューは `ListLayout` の円弧配置を流用した4行（日時・輝度・消灯時間・情報）。
  行数固定5を `ScreenModel::rowCount` で可変化する（`ListLayout.h:65` の `hitRow`、
  `ScreenManager.cpp:52-53,74` の `%5` と `4*rowSpacing`）。配列容量5は据え置きで足りる。
  **設定メニューにアイコンは置かない**。新しいマスク資産（`icons/*.png` の追加とID採番）を
  作業4の前提にしないため、ラベルと現在値だけを描く。選択行はライム、非選択は白。
  アイコンは資産が用意できた時点で `IconId` の末尾へ追加する。
- 操作
  - 物理: Aでフィールド／候補を送り、Bで編集開始・確定する（全体計画5.4節）。
  - タッチ: フィールドのタップで選択、▲▼のタップで増減、保存／キャンセルのタップで確定する。
    当たり判定は `ui/SettingsLayout.h` に置き、描画と共有する（`ListLayout` と同じ流儀）。
    ドラッグ後のタップ抑止は作業2の規約に従う。
  - 日時は年・月・日・時・分の5フィールド。編集中は実在日へ丸めず、保存時に検証して
    不正なら通知を出して保存しない。
  - 輝度はプレビューを即時反映し、キャンセル・ホームで保存値へ戻す。
  - 情報はファームウェア名・版とIDF版のみを表示する。起動時に一度取得して保持し、
    画面から `esp_*` を直接呼ばない。
- 輝度の適用経路: 表示モデルに実効輝度（編集中はプレビュー、通常は保存値）を持たせ、
  `AppRuntime` が前回適用値と異なるときだけ `Hal::setBrightness()` を呼ぶ。
  プレビュー・キャンセル・ホーム破棄・消灯復帰が1か所に集まり、偽HALでPC検証できる。
  `M5Hal::setScreenOff(false)` から輝度90の固定値を外し、`wakeup()` だけにする。
- 消灯時間の適用: `PowerManager` の `30000000` を可変の期限へ置き換え、保存確定時に反映する。

確認: フィールド送り、候補循環、保存、キャンセル、ホームでの破棄、輝度プレビューの復元、
消灯時間の反映、保存失敗時の通知と時刻無効化をPCで検証する。

### 4-5. 設定画面の描画

- `ui/SettingsView.{h,cpp}` が `plan` / `paint` を持ち、`Renderer::draw` から時計・一覧と同じ
  順序で呼ばれる。設定表示中は時計・一覧の行を空矩形で登録し、消し残しを防ぐ既存の流儀を守る。
- 要素数はメニュー6、日時編集8（題名・5フィールド列・保存・キャンセル）、輝度6、消灯6、情報5。
  ▲・値・▼は1フィールド1要素にまとめる。最大でも時計5＋一覧5＋通知1＋設定8＝19で
  `FramePlan::Capacity=32` に収まる。超過時の全面描画退避は既存経路をそのまま使う。
- フィンガープリントは既存の `hashString` / `hashValue` に編集中フラグ・カーソル・表示文字列を混ぜる。
  描画は既存部品を再利用する（`Text.h` の `fitText`、`fillSmoothRoundRect`、`fillRoundRect`、`scaled()`）。
  新しい描画プリミティブは追加しない。円は偶数径で中心を画素境界に置く既存の規約に従う。
- 画面切り替えは全面再描画（既存の `m.screen!=previousScreen_` 判定）。
  一覧→設定のスライド演出は作らず即時切り替えとする。
- フォントの再生成: `tools/build_font.py` は `src/` 以下の非ASCII文字を走査して部分集合を作る。
  設定・日時・輝度・消灯時間・情報・保存・キャンセル・戻る・秒・年月日時分・保存結果の通知文など、
  追加した文言の分だけ `python tools/build_font.py --source <GenShinGothic-Medium.ttf>` を
  実行し直し、`src/ui/fonts/GenShinGothicMedium28.vlw` を再コミットする。`--check` でズレを検出する。
  描画検証版のグリフ収録検査（`RenderDiagnostics.cpp` の `covered()`）にも新文言を追加する。

### 4-6. 検証とドキュメント

- `tests/runtime_tests.cpp` の偽HALへRTC・電池・輝度を追加する（失敗注入と呼び出し回数の記録を含む）。
  既存の入力・電源・過負荷テストは変更しない。
- `CivilTime`・`TimeService`・`SettingsStore`・`SettingsScreen` のテストを追加し、
  新規ファイルを `tools/test_runtime.py` のビルド対象へ入れる。
- 描画検証版のピクセル比較へ、設定メニュー・日時編集・輝度・消灯時間・情報の各画面、
  編集中フラグの切り替え、通知の重なりを追加する。RTC書き込み失敗と無効RTCは
  診断版の偽実装から注入し、通常版に含めない（`tools/verify_build.py` の環境分離検査を維持する）。
- 実装後に `docs/task4/settings.md`（構成・操作・実機手順）と
  `docs/task4/task4-validation.md`（版・環境・シナリオ・結果・未達）を残す。
  [全体計画](../plan.md) の作業4チェックは実装と必要な実機確認が済んでから更新する。

## 変更する主なファイル

| ファイル | 変更 |
|---|---|
| `src/services/CivilTime.h` | 新規。純粋なJST/UTC日付演算 |
| `src/services/TimeService.{h,cpp}` | 新規。RTC検証・同期・手動保存・失敗回復 |
| `src/services/LauncherData.{h,cpp}` | 新規。`DisplayDataSource` 実装（時刻＋電池30秒） |
| `src/storage/SettingsStore.{h,cpp}` | 新規。検証・既定値・スキーマ版 |
| `src/storage/NvsBackend.{h,cpp}` | 新規。名前空間 `launcher` の単一blob |
| `src/apps/SettingsScreen.{h,cpp}` | 新規。メニューと編集画面の状態機械 |
| `src/ui/SettingsLayout.h` | 新規。フィールド配置と当たり判定の共有 |
| `src/ui/SettingsView.{h,cpp}` | 新規。plan/paint |
| `src/hal/Hal.h`、`src/hal/M5Hal.{h,cpp}` | RTC・電池・輝度を追加。復帰時の輝度90固定を削除 |
| `src/power/PowerManager.h` | 消灯30秒を可変の期限へ |
| `src/ui/DisplayModel.h` | `ScreenId::Settings`、設定モデル、`rowCount`、実効輝度 |
| `src/app/ScreenManager.{h,cpp}` | 設定への委譲、行数の可変化、決定時の分岐整理 |
| `src/app/AppRuntime.{h,cpp}` | 輝度・消灯時間の適用、設定保存後の即時更新 |
| `src/ui/Renderer.{h,cpp}` | 設定レイヤーの計画・描画、行数の可変化 |
| `src/main.cpp` | `nvs_flash_init`、サービス生成、データ源の差し替え、輝度の初期適用 |
| `src/CMakeLists.txt` | `nvs_flash` を `REQUIRES` へ |
| `src/ui/fonts/GenShinGothicMedium28.vlw` | 追加文言で再生成 |
| `tools/test_runtime.py`、`tests/*.cpp` | 新規テストの追加、偽HALの拡張 |
| `src/ui/RenderDiagnostics.cpp` | 設定画面のピクセル比較、グリフ収録検査、失敗注入 |

## 検証と完了条件

### PC

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

| 層 | 確認内容 | 判定 |
|---|---|---|
| 時刻 | 月末、うるう日、年跨ぎ、JST日付境界、範囲外年、不実在日、書き込み失敗、再読み出し不一致 | 変換が一致し、失敗を成功と返さない |
| 設定 | 既定値、往復保存、未知の版、範囲外、load/save失敗 | 全消去なしで既定値動作。診断ログあり |
| 画面 | フィールド送り、候補循環、保存、キャンセル、ホーム破棄、輝度プレビュー復元、消灯時間反映 | 作業2・3の既存テストも合格を維持 |
| 描画 | 設定各画面の差分計画と全面描画のピクセル一致、容量超過と回復 | 不一致0 |
| ビルド | 3環境、4MiB上限、SDK・MultiFirm固定版・パーティション・host検査、診断文字列の環境分離 | 全環境成功 |

### 実機

導入は[作業1のinstall-host手順](../task1/product-build.md)に従う。通常uploadは使用しない。

1. 通常版で起動し、RTC読み出し結果・NVS初期化結果・適用輝度・消灯時間をログで確認する。
   時計に実時刻・実日付・実電池が出て、`--:--` や `--%` でないことを確認する。
2. 作業3から持ち越した未確認項目を併せて消化する。28pxフォントの見た目、行の高さ、
   円形端での省略、一覧のピクセル比較、描画時間への影響
   （[作業3の検証記録](../task3/task3-validation.md) の「実機未確認」）。
3. 設定メニュー→日時。物理A/Bとタッチの両方で値を変え、保存・キャンセル・ホームを確認する。
   月末・うるう日・不実在日の入力で保存が拒否されることを確認する。
4. 日時保存直後に時計が即時更新され、次の分境界でも更新されることを確認する。
5. 輝度のプレビューが即時反映され、キャンセルとホームで元へ戻ることを確認する。
   保存後に消灯→復帰しても保存値が使われることを確認する。
6. 消灯時間を15秒へ変更して実際に15秒で消灯すること、30秒へ戻せることを確認する。
7. 再起動して輝度・消灯時間が復元され、時計が再同期されることを確認する。
8. 電池表示が30秒間隔で更新され、充電の抜き差しで表示が変わることを確認する。
   USB抜き差しで不要な点灯・描画が起きないことを確認する（作業2・3の規約）。
9. 描画検証版で設定各画面の `mismatches=0` を確認する。無効RTC・保存失敗の注入で
   `--:--` と通知が出て、時刻を成功表示しないことを確認する。
10. 通常版へ戻し、診断ログが出ないことを確認する。

### 完了条件

月末・うるう日・JST日付境界・無効RTC・保存失敗を確認する。再起動後に設定を復元し、
キャンセルしたプレビューは元へ戻る。NVS異常でも他名前空間を消去しない。
性能の再測定と操作中30fps超の判定は作業3から引き続き作業7で行う。
未達は原因と対応を明記し、合格扱いにしない。

## 作業4に含めないもの

- ストップウォッチ（作業5）、スロット走査・外部起動（作業6）。
- 画面基底クラスの抽出（2つ目の実装が出た時点。実際には作業6で抽出した）。
- 設定メニューのアイコン資産。
- タイムゾーン設定UI、共有NVS、USB時刻設定、MultiFirm共有時刻API。
- DFS・light sleep・電力目標の評価（作業8）。
