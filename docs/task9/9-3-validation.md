# 作業9-3: 設定トップの共通リスト移行の検証

実施日: 2026-09-23。対象は[9-3の実装計画](plan-9-3.md)。
状態: ホストでの実装・検証完了。実機が必要な確認は既定どおり9-5へ引き継ぐ。

## 実装結果

| 配置 | 内容 |
|---|---|
| `features/settings/SettingsMenu.h`（新規） | 行ID `SettingsItem`（日時=1・輝度・消灯時間・情報・戻る）、IDから遷移先への対応 `settingsItemView`、行データとラベル生成 `buildSettingsMenuRows`、配置 `settingsMenuPlacement`（=`fullListPlacement`） |
| `features/settings/SettingsModel.h` | メニューの選択・スクロールを `ListState menu` として追加。`cursor` は編集/情報画面専用 |
| `apps/SettingsScreen.*` | 設定専用の `ListController` と入力用の行（IDのみ）を所有。メニュー中の入力は `handleMenu` で共通リストへ委譲し、決定した行IDから遷移先を引く。`menuCursor_` と添字→`SettingsView` の加算変換を削除 |
| `ui/SettingsLayer.*` | 設定専用の `ListView` と行・ラベル領域を所有。メニューはListView、編集/情報は既存の要素群をplan/paint。切替時に全面再描画と両履歴の無効化 |
| `ui/list/ListView.*` | `invalidate()` を追加。スロットのElement履歴と今回の可視行を忘れる（キャッシュは内容キーなので保持） |
| `ui/SettingsLayout.h` | 旧メニュー用の `settingsMenuRow`・`settingsMenuLabelX`・`SettingsMenuRows`・`SettingsHit::MenuRow` を削除。`settingsSlotCount(Menu)` は0 |
| `apps/AppScreen.h` | `active()` を追加（既定false） |
| `app/ScreenManager.*` | 期限・活動状態・更新を表示中の画面だけから取る。計測分類 `FrameActivity` を構成して `ScreenModel.activity` へ入れる |
| `app/FrameModel.h` | `FrameActivity`（Single・Transition・LauncherScroll・SettingsScroll・Stopwatch・SettingsSingle） |
| `ui/RenderDiagnostics.cpp` | 分類を `m.activity` から取る。設定のスクロール/単発を独立集計。設定リストのキャッシュ統計を出力。描画検証に設定メニューの項目を追加 |

### 画面と状態

| 境界 | 実装 |
|---|---|
| 設定へ入場 | `enter()` で `ListController::reset()`。先頭行選択・scroll 0（先頭行が中央） |
| メニュー→編集/情報 | `openItem()` で `finish()`。進行中のアニメーションは目標位置で完了し、ドラッグ・慣性・期限を解除 |
| 保存/キャンセル/情報から戻る | `openView(Menu)` はリストの状態に触れない。選択行とscrollがそのまま残る。ラベルは毎フレーム保存値から生成するため保存成功時だけ変わる |
| 戻る行 | `out.leave`。ScreenManagerが `exit()` し、ランチャー一覧の選択・位置は開いた時のまま |
| A+Bホーム | `exit()` で `reset()` と編集破棄（プレビューは開いている編集画面から導出するため同時に消える） |
| 再入場 | `enter()` で先頭へ初期化 |

入力の順序はランチャー一覧（`ScreenManager::handle`）と同じ: TouchStart（慣性停止）→ 停止後の解放 → Cancel → 縦ドラッグ → A → タップ/B。
横方向のドラッグは扱わない。端のジェスチャーはなく、先頭で下へ引いても `dragMove` のクランプと整列で先頭へ戻るだけ。

### 描画

- 設定行はアイコンなし（`icon=false`）。文字起点・円弧・行間・選択色（Lime）・省略幅は9-2の共通規則のまま。ダミーアイコンは作っていない。
- メニュー/編集・情報/非表示の3状態を `SettingsLayer` が記録し、変化したフレームで `FramePlan::forceFull()`、`ListView::invalidate()`、要素群のElementを初期化する。画面IDが変わらない切替もRendererの画面切替判定に依存しない。同じメニュー内の選択・スクロールは差分描画のまま。
- 1フレームにplanするのはメニューか要素群の一方のみ。FramePlanの最大登録数は変わらず25（文字盤5＋一覧8＋設定max(11, 8)＋トースト1、容量32）。
- 文字の省略結果・文字画像キャッシュはアプリ一覧とは別インスタンス（設定の `ListView`）で同じ経路を通る。画像は最大5行分で、一覧の8スロットとは独立。

### 期限・活動状態

| 項目 | 実装 |
|---|---|
| `SettingsScreen::nextUpdate()` | メニュー中だけ `ListController::nextUpdate()`。編集・情報・退場後は `INT64_MAX` |
| `SettingsScreen::tick()` | メニュー中だけ `ListController::update(now)`。経過時間から位置を計算するため遅れても現在時刻の位置になる |
| `SettingsScreen::active()` | メニューのドラッグ/アニメーション（慣性・A整列）中だけtrue |
| `ScreenManager::active()` / `nextUpdate()` / `update()` | 内蔵画面が開いていればその画面だけ。ホーム/一覧ならランチャーの遷移・ドラッグ・一覧。隠れた一覧の期限・活動が設定へ混ざらない |

Runtime（GPIO通知・100ms追跡・USB期限・最低1tick待機・消灯中は表示期限を待たない）は変更していない。
活動状態はPowerManagerのActive/WatchIdleだけに使われ、無操作タイマー（`lastActivity_`）は入力でしか延長されない。

### 計測分類

構成側（`ScreenManager::activity()`）が画面種別＋操作種別を決めて `ScreenModel.activity` に入れ、RenderDiagnosticsはそれを読むだけにした。
設定リストの状態を一覧のモデルへコピーする実装はしていない。

| 分類 | 出力名 | 条件 |
|---|---|---|
| Transition | `transition-draw` / `-interval` | ホーム↔一覧の遷移率が0〜1の途中 |
| LauncherScroll | `scroll-draw` / `-interval` | 画面を開いていない時の一覧ドラッグ・アニメーション（過去の記録と比較できるよう名前を維持） |
| SettingsScroll | `settings-scroll-draw` / `-interval` | 設定メニューのドラッグ・慣性・A整列 |
| Stopwatch | `stopwatch-draw` / `-interval` | 計測中のストップウォッチ |
| SettingsSingle | `settings-single-draw` | 上記以外の設定画面のフレーム（編集画面の操作、静止中のメニュー、トースト） |
| Single | `single-draw` | その他 |

9-0の設定の単発選択（A約2回/秒、`single-draw` 平均10.157ms）は、メニューの見た目がスクロール方式に変わったため
Aでの選択変更が `settings-scroll` へ移る。9-5では `settings-scroll`（新設のスクロール）と `settings-single`（編集画面等の単発）を分けて比較する。
`[RenderDiag] settings_list_cache ...` を `list_cache` の次に出力する。

## 見た目の変更

設定トップはアプリ一覧と同じスクロール方式になった。

- 変更前: 5行を画面中央にまとめて固定配置（`scroll=2×行間` 相当）。スクロールなし、カーソルの色だけが動く。
- 変更後: 選択行を画面中央へスクロールする。入場時は「日時」が中央で、下に4行が円弧に沿って並ぶ。
  Aで次の行が中央へ180msで移動し、「戻る」から「日時」へ循環する。ドラッグ・慣性・タップでの決定は一覧と同じ。
- 行の文字位置（アイコンなしの起点）・色・フォント・省略幅は変更前と同じ共通規則。

実機での見た目の確認は9-5で行う（新しい見た目を新しい基準として評価する）。

## ホスト・ビルド検証

| 項目 | 結果 | 記録 |
|---|---|---|
| `python tools/test_runtime.py` | 6スイートPASS。設定スイートに行ID・操作・ラベル・Runtime連携を追加 | [ホストログ](20260923-9-3-host.log) |
| 追加テストの有効性 | `openItem()` の `finish()` を外すと設定スイートが失敗することを確認し、元に戻した | 一時ビルドのため記録なし |
| `pio run`（3構成） | 製品・測定・描画検証すべてSUCCESS（1回目で成功）。警告はM5GFX・M5Unifiedの既存のもののみで、`src/` からはなし | [製品](20260923-9-3-build-m5stopwatch.log)、[測定](20260923-9-3-build-m5stopwatch-measure.log)、[描画検証](20260923-9-3-build-m5stopwatch-render-check.log) |
| `tools/verify_build.py` | 3構成PASS | [製品](20260923-9-3-verify-m5stopwatch.log)、[測定](20260923-9-3-verify-m5stopwatch-measure.log)、[描画検証](20260923-9-3-verify-m5stopwatch-render-check.log) |
| `git diff --check` | PASS | 2026-09-23の最終確認 |
| 境界の静的確認 | `ui/list`・`ui/overlays`・`ui/graphics` に設定（Settings・features）への参照なし。`settingsMenuRow`・`SettingsMenuRows`・`MenuRow`・`menuCursor_` の残存なし | `grep` による確認 |

検証済みイメージのサイズは製品1,063,328 bytes（9-2比+176）、測定1,067,680 bytes（+400）、描画検証1,065,888 bytes（+2,304）。
サイズはビルド結果であり、実機上の動作や描画速度の証明には用いない。
書き込み保護のテスト（`test_upload_guard.py`）は対象の `tools/upload_guard.py` を変更していないため再実行していない。

追加したホストテスト（`tests/settings_tests.cpp`）:

- `menuRows`: 行の順序・ラベル（保存値入り）・アイコンなし・決定可・IDの一意性、IDから遷移先への対応（戻るだけ遷移先なし）、入力用はIDのみでラベル空。
- `menuList`:
  - 入場時の先頭・静止・期限なし。Aで次行へ選択、16ms期限、途中位置、180ms後に整列・期限解除。遅れたフレームは現在時刻の位置へ。末尾から先頭への循環。
  - 先頭での下ドラッグはホームへ戻らず先頭へ収束。縦ドラッグで決定しない。横ドラッグは扱わない。
  - Aのスクロール中のヒットテスト（現在位置で描いた行矩形に当たる）とタップ決定。整列中のTouchStartは慣性ではないので止めない。
  - 慣性中のTouchStartで停止し期限なし。その後のタップ・ドラッグで決定しない。
  - 整列中のタップで編集画面を開くとアニメーション完了・期限なし・活動なし。編集中のA/Bは編集のもの。キャンセルで同じ行・同じscrollへ復帰。A途中からBで情報を開き、戻るで同じ行・位置へ復帰。
  - スクロール中のホームで設定の活動・期限が残らない。Aの途中で「戻る」を決定しても期限が残らず、一覧は開いた時の行・位置のまま。一覧先頭の下スワイプではホームへ戻る。
  - 計測分類（SettingsSingle・SettingsScroll・Single）。
- `menuLabels`: 輝度の保存失敗で編集画面と値が残りラベルは保存値のまま、再試行の成功でラベル更新と元の行への復帰。消灯時間のキャンセルでラベル・実効値が変わらない。
- `runtimeMenuScroll`（AppRuntime＋HALスタブ）: メニューのスクロールが期限だけで連続描画される（8フレーム以上）、その間PowerManagerがActive、終了後はWatchIdle・描画停止・待機が期限なしの1秒（USB周期）。消灯中は古い期限で描画も短い待機もしない。タッチ復帰の1フレームで最終位置へ収束し、期限を残さない。

既存テストはメニューの選択を `settings.cursor` から `settings.menu.selection` へ読み替え、`settingsSlotCount(Menu)` の期待値を0にした。操作手順と期待する遷移は変えていない。

## 描画検証版への追加（9-5で実機実行）

`runRepaintCheck` の設定メニュー部分を差し替えた。すべて差分描画と全面描画の画素比較（`check`）、または文字画像と直接描画の比較（`direct`）。

- `settings-menu`・`settings-menu-image`: 各行を選択・中央にした状態。
- `settings-menu-scroll`: 選択2のまま、行間の途中を含むscroll 0〜336を29px刻み。
- `settings-menu-toast-on/overlap/off`: メニュー上のトースト出現・スクロール・消失。
- `settings-menu-long`・`settings-menu-image-long`・`settings-menu-empty`・`settings-menu-restored`: 省略が必要な長い日本語＋未収録文字、空ラベル、復元（`SettingsLayer::menuLabelForTest`）。
- `settings-menu-mid`・`settings-menu-to-view`・`settings-view-to-menu`: 行間の途中位置からの輝度/情報画面への切替と復帰。
- `settings-menu-left`・`settings-menu-return`・`settings-menu-reentry`: 途中位置での退場・同位置での復帰・先頭での再入場。
- 既存の編集画面・情報・トースト・`settings-left` 以降の項目は維持。

## 9-5への引き継ぎ

実機への書き込み・画素比較・目視・性能/メモリ測定は9-3では実施していない。9-5で少なくとも次を確認する。

- 描画検証版の全チェックPASS（上記の設定メニュー項目を含む）。設定リストの文字画像の画素一致。
- 設定トップの新しい見た目（中央寄せのスクロール、アイコンなしの文字起点、長い値のラベル、トースト、編集画面との往復で消し残しがないこと）の目視。
- `settings-scroll`（A整列・ドラッグ・慣性）の描画時間・実fps・フレーム間隔と、`settings-single`（編集画面の操作）の単発描画時間・入力遅延。9-0の設定単発選択10.157msとは操作の性質が変わった点を明記して比較する。
- `settings_list_cache` の確保量と、一覧↔設定の往復・設定の再入場でメモリが減り続けないこと。
- 設定スクロール中の消灯・復帰、スクロール中のA+Bホーム、「戻る」後の一覧の静止（描画停止）。
