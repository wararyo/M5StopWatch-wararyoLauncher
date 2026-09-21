# 作業6：MultiFirmホスト連携の実装計画

作成日: 2026-09-22。設計の基準は [全体計画](../plan.md) の4・5.2・8.1・8.2章と作業6。
本書は実装前に合意した計画であり、実装・検証完了の記録ではない。
結果は [作業6の検証記録](task6-validation.md)、構成と手順は [MultiFirmホスト連携](multifirm.md) を参照。

## なぜ作業5より先にやるのか

[全体計画](../plan.md) の実装計画表は作業6の依存を「5」としているが、実装本体に作業5への依存はない。
依存しているのは完了条件の3項目だけで、その実体は `bootSlot` の shutdown コールバックに
計測停止を足すことと、確定画面の文言分岐である。

| 作業6の要素 | 作業5への依存 |
|---|---|
| MultiFirmAdapter・走査ワーカー | なし |
| スロット状態の一覧表示・詳細画面 | なし |
| BootCommitting・起動確定・失敗復帰 | なし |
| 検証中のホーム、遅延結果の無害化 | なし |
| 「計測中なら『計測を終了して起動』」（8.2節1） | あり |
| 「API失敗時は再起動せず計測が継続」（完了条件） | あり |
| 「成功時は計測を終了する」（完了条件） | あり |

よって作業6を先に実装し、上の3項目は作業5の完了時に接続して消化する。
そこまで全体計画の作業6チェックは閉じない。

副次的な利点として、作業6で2つ目の内蔵アプリ画面が出るので、
[作業4](../task4/settings.md) が作業5へ先送りしていた画面インターフェースの抽出をここで行える。
作業5は抽出済みの型に乗るだけになる。

## 到達点と範囲

起動すると時計が先に出て、その裏で `ota_1`〜`ota_3` を走査する。一覧の外部3行はスロット名に変わり、
起動できないスロットはグレーになる。行を開くと詳細画面で状態・バージョンが読め、
起動できるスロットだけが起動できる。起動に失敗したら理由を出してランチャーへ戻る。

含めないもの: ストップウォッチ（作業5）、走査結果のキャッシュ、手動再走査、`multifirm_nvs`、
MultiFirm共有API・経過時間復元・USB時刻設定、性能の再測定（作業7）、省電力（作業8）。

## 実装前に決めたこと

- **一覧の見せ方**: 名前のみ＋色で区別する。起動できるスロットはその名前、それ以外は
  「外部アプリN」のままグレー。状態語は詳細画面でのみ出す。
  全体計画5.2節の「行はアイコンとアプリ名だけ」「起動可否の理由文を常設しない」を保ち、
  8.1節の状態区別は詳細画面が受け持つ。行の構成・高さ・円弧配置は作業3のまま触らない。
- **計測連携**: 作業6では計測に一切触れない。フック（shutdownコールバックと文言の分岐点）だけ用意する。

## MultiFirm APIの制約（調査結果）

- `inspectSlot` / `scanSlots` は**戻り値だけで成否を判定してはいけない**。戻り値が非OKになるのは
  レイアウト不一致と不正indexのときだけで、個別スロットの失敗は `ESP_OK` ＋ `slot.state` に載る。
- 走査は完全同期・ブロッキング。1スロットあたりイメージ全体を2周読んでSHA-256を計算する。
  MultiFirmの実機実測（`docs/phase3-result.md`）で**スロット1/2/3が322/318/309ms、全体989ms**。
  このランチャーの実機（別のゲスト構成）では422/609/422ms・合計約1.45秒だった。
- `bootSlot` は `index` だけを取り、レイアウトとイメージを**再検証**してから
  `esp_ota_set_boot_partition` → shutdown → `esp_restart()`。再検証も約320ms。
  失敗時は `esp_err_t` を返して副作用なしで戻る。
  **shutdownは起動先設定に成功した後にだけ、呼び出したタスクで同期実行される**。
- `isMultiFirmLayout()` はフラッシュからパーティション表を読んでバイト完全一致を要求する。
  起動時に1回だけ評価して保持する。

## 実装順序

### 6-1. スロット情報の型とポート

MultiFirm・ESP-IDFの型をUI側へ漏らさないため、`Hal` と同じ流儀でポートを1枚挟む。

- `multifirm/SlotCatalog.h`: `SlotStatus`（MultiFirmの4状態＋`Scanning`・`Unsupported`）と
  3スロット分の名前・版・エラーを持つPOD。ESP非依存。
- `multifirm/SlotService.h`: `requestScan` / `poll(SlotCatalog&)` / `boot(int,const char**)` の抽象。
  `boot` が同期なのは意図的（理由は6-2）。
- `multifirm/FakeSlotService.h`: 状態注入。PCテストと描画検証版だけが使う。

### 6-2. MultiFirmAdapterと走査ワーカー

- `begin()`: `isMultiFirmLayout()` を1回だけ評価。非対応ならタスクを作らず全スロットを非対応にする。
- `requestScan()`: 走査タスクを1本だけ作る（CPU0固定・優先度1・スタック8192）。
  UIタスクは `CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1` でCPU1に固定されているので干渉しない。
  `inspectSlot(1)`→`(2)`→`(3)` を順に呼び、1件終わるごとにミューテックス下でカタログを更新して
  世代を進め、`vTaskDelay(1)` を挟んでCPU0のアイドルへ実行機会を渡す。
  `scanSlots()` ではなく `inspectSlot` ループにするのは、結果を1件ずつUIへ返すためと、
  `std::vector<SlotInfo>` を毎回確保しないため。
- `poll(out)`: 世代を比較し、変わっていればミューテックスを短時間だけ取ってコピーする。
- `boot(slot,&message)`: **UIタスクから同期で呼ぶ**。shutdownコールバックは呼び出したタスクで
  実行されるので、止める対象を持つUIタスクが唯一の正しい呼び出し元。
  起動直前再検証の約320msだけブロックするが、この区間は入力を抑止していて直後に再起動する。
  WDTの5秒に対して余裕があり、**WDT無効化・タイムアウト延長はしない**（全体計画6.2節）。
  shutdownは当面ログのみ。**作業5でここに計測停止を足す**。

### 6-3. 一覧への反映

- `ScreenModel` に `rowDimmed[5]` を追加する。`names[5]` は既存の差し込み口をそのまま使う。
- `ScreenManager::setSlots()` でカタログを保持し、`model()` で外部3行に名前とdimを載せる。
- `Renderer::paintList` はdimのとき名前の色をグレーへ落とす。**フィンガープリントにdimを混ぜる**
  （混ぜ忘れると色が変わっても再描画されない）。アイコン円・マスク・行の高さ・円弧配置は触らない。
- 外部行は状態にかかわらず詳細画面へ入る。「準備中」トーストはストップウォッチ行にだけ残す。

### 6-4. 画面インターフェースの抽出と外部アプリ詳細画面

- `apps/AppScreen.h`: `ScreenOutcome` と `resize / available / enter / exit / handle / nextUpdate`、
  そして取り消せない区間を示す `exclusive()`。`SettingsScreen` をこの基底へ寄せ、
  `ScreenManager` は `AppScreen*` へ委譲する。表示モデルは型が違うのでインターフェースに入れない。
- `apps/ExternalAppScreen.{h,cpp}`: 位相 `Browsing / BootCommitting / BootFailed`。
  起動できるスロットのボタンは「起動」「キャンセル」、それ以外は「戻る」1つ。
  確定後は**ホームを含む全入力を捨てる**（8.2節3）。失敗後は理由を出してホームを再び受け付ける（8.2節4）。
  走査完了でボタン数が変わってもカーソルが範囲外にならないよう、表示モデル側で丸める。
- `ui/ExternalLayout.h` / `ui/ExternalLayer.{h,cpp}`: 設定と同じ流儀。
  題名・3行・ボタン2で容量6。スロット名と版は外部データなので `fitText` を通す。
- 起動の発火点: `AppRuntime::step()` で**描画を終えた後に** `commitPendingBoot()` を呼ぶ。
  これで「起動中」のフレームが必ず画面に出てからブロックに入る。APIは1回しか呼ばない。
- 検証中のホーム: `setSlots()` は一覧の行を更新するだけで、画面遷移も起動も起こさない。
  この性質は「ホーム後に結果を流し込む」PCテストで固定する。

### 6-5. 要素容量と描画

- 設定レイヤーと外部レイヤーを、**自分の画面でないときは何も登録せずに返す**形にする。
  画面切り替えはもともと全面再描画なので消し残しは出ない。
  同一画面内のビュー切り替えのための空矩形登録は残す。
- これで同時最大は 時計5＋行5＋トースト1＋max(設定11, 外部6) = **22**（容量32）。
  この変更なしでは30で、作業5のレイヤーを足した時点で溢れる。
- フォント: 追加文言ぶん `tools/build_font.py` を再実行して `.vlw` をコミットし直す。
  `RenderDiagnostics.cpp` の `covered()` にも同じ文言を並べる。

### 6-6. 検証とドキュメント

- `tests/multifirm_tests.cpp` を追加（偽 `SlotService` 相手）。一覧の名前差し替えとdim、
  各状態の詳細、起動可否、確定中の入力抑止、失敗からの復帰、
  **ホーム後に届いた結果が遷移も起動も起こさないこと**、走査完了時のカーソル、
  Runtimeの2つの順序（初回描画→走査要求、確定フレーム→起動）。
- `MultiFirmAdapter.cpp` はESP専用なのでPCテストのソースに入れず、`src/CMakeLists.txt` にだけ足す。
- 描画検証版: 詳細画面の各状態と位相、一覧のdim、長すぎる名前と未収録文字をピクセル比較へ追加する。
  実機で作れない状態（破損・読み取り失敗・非対応・起動失敗）は `FakeSlotService` から注入する。
  実物の破損イメージやパーティション表の破壊は実機で作らない。

## 変更した主なファイル

| ファイル | 変更 |
|---|---|
| `src/multifirm/SlotCatalog.h` | 新規。ESP非依存のスロット状態POD |
| `src/multifirm/SlotService.h` | 新規。抽象ポート |
| `src/multifirm/MultiFirmAdapter.{h,cpp}` | 新規。CPU0の走査ワーカーと同期`boot`、shutdownフック |
| `src/multifirm/FakeSlotService.h` | 新規。テスト・診断版の状態注入 |
| `src/apps/AppScreen.h` | 新規。`ScreenOutcome` と画面インターフェース |
| `src/apps/ExternalAppScreen.{h,cpp}` | 新規。詳細・起動確定・失敗復帰の状態機械 |
| `src/ui/ExternalLayout.h`、`src/ui/ExternalLayer.{h,cpp}` | 新規。配置・当たり判定とplan/paint |
| `src/apps/SettingsScreen.{h,cpp}` | `AppScreen` 継承へ。`SettingsOutcome`→`ScreenOutcome` |
| `src/ui/SettingsLayer.cpp` | 自画面でないときは登録しない |
| `src/ui/DisplayModel.h` | `ScreenId::External`、`ExternalModel`、`rowDimmed[5]` |
| `src/ui/ListLayout.h`、`src/ui/SettingsLayout.h` | `offsetPx` を共有ヘッダへ移動 |
| `src/app/ScreenManager.{h,cpp}` | `setSlots`、`AppScreen*` 委譲、外部行の決定、`commitPendingBoot` |
| `src/app/AppRuntime.{h,cpp}` | `bindSlots`、毎stepの`poll`、初回描画後の走査要求、描画後の起動 |
| `src/ui/Renderer.{h,cpp}` | 行名のグレー表示、`ExternalLayer` の接続、詳細表示中の行の消去 |
| `src/main.cpp` | `MultiFirmAdapter` 生成と `begin`。描画検証版は `FakeSlotService` |
| `src/CMakeLists.txt` | 新規ソース3本 |
| `src/ui/fonts/GenShinGothicMedium28.vlw` | 追加文言で再生成（326→337グリフ） |
| `src/ui/RenderDiagnostics.cpp` | 詳細画面のピクセル比較、グリフ収録検査、状態注入 |
| `tools/test_runtime.py`、`tests/multifirm_tests.cpp` | 新規スイートの追加 |

`platformio.ini` と `partitions.csv` は変更しない。`checkLayout()` がパーティション表の
バイト完全一致を要求するため、`partitions.csv` は1行も触らない。

## 検証と完了条件

PC検証の一覧と実機手順は [MultiFirmホスト連携](multifirm.md)、結果は
[作業6の検証記録](task6-validation.md) にある。

完了条件: 正常・空き・破損・読み取り失敗・非対応表の表示、API失敗時に再起動しないこと、
各ゲストへの起動と時計画面への復帰、ホスト更新後のゲスト・設定保持を確認する。
ストップウォッチ前提の3項目は作業5の後に消化する。未達は原因と対応を明記し、合格扱いにしない。

## 作業5へ渡すもの

- `apps/AppScreen.h` の画面インターフェース（`StopwatchScreen` は3つ目の実装として乗るだけ）
- `MultiFirmAdapter::shutdown`（計測停止を足す唯一の場所。UIタスクで同期実行される）
- `ExternalLayer` の確定ボタン文言の分岐点
- `FramePlan` の空き容量（同時最大22／容量32）
