# 作業9-4: 構成・所有権・配置の整理の検証

実施日: 2026-09-23。対象は[9-4の実装計画](plan-9-4.md)。
状態: ホストでの実装・検証完了。実機が必要な確認は既定どおり9-5へ引き継ぐ。性能合格・実機確認済みとは扱わない。

## 実装結果

### 所有と依存

| 所有者 | 実装 |
|---|---|
| アプリ構成 `app/Application.h`（新規） | `StopwatchService`・`RuntimeSettings`・終了処理 `AppShutdown`・`ScreenManager`・`AppRuntime` を宣言順に所有し、参照で注入する。`bindSlots()` が `AppShutdown` を `SlotService` へ登録し、破棄時に解除する。ホストでもビルドできる（ハードウェア・表示・NVS・スロットはmainが作って渡す） |
| `main.cpp` | `M5Hal`・`AppRenderer`・データ源・スロット・NVS・`SettingsStore` を作り、`static Application` に渡す。以前スタック上にあった `AppRuntime`（`ScreenManager` を含む）は静的領域へ移った |
| `ScreenManager` | 現在画面、入退場、ホーム優先、`LauncherController` が決めた起動先の `enter()`、通知の寿命、`FrameModel` の合成。`BootShutdown` の実装・`StopwatchService` の所有・`stopwatch()`・`now_` を削除。`StopwatchService&` と `RuntimeSettings&` を借りる |
| `LauncherController`（`features/launcher/`、新規） | ホーム↔一覧の遷移補間、ドラッグの所有者決定（時計の引き上げ・一覧先頭の下引き・スクロール）、ランチャー専用 `ListController` と入力用の行。決定は `LauncherOutcome{open,target}`（`AppEntry`）で返し、画面を開かない。`suspend()`（画面を開く時: 動きを目標で完了、選択・位置を保持、期限なし）と `home()`（先頭へ初期化） |
| `HomeLayer`（`features/home/`、新規） | 文字盤の登録（4枠、重複ID拒否）・選択（失敗時は前の文字盤をキャッシュなしで復旧）・begin/end・次回期限。切替後の最初のplanで `forceFull()`。デジタル文字盤をメンバーに持ち、デストラクタで選択中の文字盤を `end()` する。外部登録の文字盤はHomeLayerより長く生存する必要があると明記 |
| `AppRenderer`（`app/`、新規） | `RenderPort` を実装。各レイヤーを所有し、フレームごとに `prepare()` で入力を固定してから描画順の配列を共通Rendererへ渡す。画面切替の全面再描画要求、統計チップの接続、描画時間の記録を担当 |
| 共通 `Renderer`（`ui/rendering/`） | `RenderLayer`（plan/paint）の固定長配列と任意の `FrameOverlay` を受け、plan→resolve→消去→背面順paint→overlay→endWrite。容量超過時の全面退避と報告は維持。ScreenId・機能・診断を参照しない |

`StopwatchService` はアプリの寿命で所有され、画面の退場・ホーム・消灯では停止しない。
外部起動確定時の停止は `AppShutdown::onBootCommitted()` が行い、停止時刻はコールバック時点の `Hal::now()`（単調時計）から取る。
以前の実装はUIが最後に見た時刻 `now_` を使っていた。起動失敗・画面退場ではこのコールバックに到達しない。
描画→`commitPendingBoot()` の順序、確定中のホーム抑止、消灯中も要求を取り残さない経路は変えていない。

統計表示は `SettingsScreen` が `ScreenOutcome::enableStats` で要求し、`ScreenManager` がアプリ所有の `RuntimeSettings` へ適用して描画要求を返す。
`SettingsScreen` の `RuntimeSettings*` と `stats()` は削除した。輝度プレビュー・保存は9-1の `EffectiveSettings` 経路のまま。

### フレームと描画構成

- `ScreenModel : AppListModel` と `NavigationState : AppListModel` を廃止し、`FrameModel` に `screen`・`viewport`・`homeCount`・`launcher`（`AppListModel`）・`settings`・`external`・`stopwatch`・`toast`・`stats`・`activity` を明示的に合成した。
- `FrameModel` は状態だけを持つ。時計の描画領域・各レイヤーの表示可否は `app/FrameComposer.h` の `composeFrame()` 一か所で導出する。旧 `composeHomeRegion()` と `homeRegion` フィールドは削除し、描画検証の各ケースは状態を設定するだけになった。
- 時計の可視性は `clockVisible(frame)`（=時計のクリップが空でない）。Runtimeの表示期限もこれを使い、一覧の `transition` を直接解釈しない。開いた画面がある時は遷移率によらず時計を隠す（実運用では画面を開くと遷移率は1なので挙動は同じ）。
- 描画順は `FrameOrder`（ホーム→一覧→設定→外部→ストップウォッチ→トースト）。統計チップはレイヤーではなく、`Renderer` の転送前オーバーレイ（`FrameOverlay`）としてAppRendererが接続する。独自の領域復元（dirty矩形に触れた時だけpush）は維持。
- 画面が変わったフレームは `FrameComposer` が `changed` を立て、AppRendererが `Renderer::invalidate()` する（旧 `Renderer` 内の `previousScreen_` 判定を移動）。同じ画面内の切替（設定メニュー↔編集）は従来どおりSettingsLayer自身の `forceFull()`。
- 各レイヤー（HomeLayer・AppListLayer・SettingsLayer・ExternalLayer・StopwatchLayer・ToastLayer）は `RenderLayer` を実装し、`prepare()` で自分のモデルと表示領域・表示可否だけを受け取る。フォントは `begin()` で一度渡す。plan/paintの間にモデルは書き換わらない。毎フレームの `new`・`std::function` は使っていない（レイヤー配列はスタック上の固定長6）。
- 描画時間の終点は `Renderer::draw()` の戻り（endWrite後）。統計チップの記録と `[RenderDiag]` の分類はAppRenderer側（`recordRender(FrameActivity,...)`）で行う。

### ファイル配置（9-4e）

移動は動作変更と分けて行い、移動だけの状態でホスト6スイートと製品ビルドを確認した（下表）。

| 移動前 | 移動後 |
|---|---|
| `apps/AppScreen.h` | `app/AppScreen.h` |
| `apps/{External,Settings,Stopwatch}*Screen.*` | `features/{external,settings,stopwatch}/` |
| `ui/{External,Settings,Stopwatch}{Layer,Layout}.*` | `features/{external,settings,stopwatch}/` |
| `ui/WatchFace.h`、`ui/DigitalWatchFace.*` | `features/home/WatchFace.h`、`features/home/faces/` |
| `ui/{Renderer,Element}.*`、`ui/{Geometry,Viewport,Scale}.h` | `ui/rendering/` |
| `ui/{Text,VlwFont}.*`、`ui/fonts/*.vlw` | `ui/graphics/`、`ui/graphics/fonts/` |
| `ui/StatsOverlay.*` | `ui/overlays/` |
| `ui/RenderDiagnostics.*` | `app/`（アプリ側の診断アダプター） |
| `ui/icons/AppIcons.bin` | `features/launcher/icons/` |

CMakeのソース一覧と `EMBED_FILES`、`platformio.ini` の `board_build.embed_files`、`tools/test_runtime.py`、
`tools/build_font.py`・`tools/build_icons.py` の出力先、README、includeを同時に更新した。
埋め込みのリンカーシンボル（`_binary_GenShinGothicMedium28_vlw_start`・`_binary_AppIcons_bin_start` 等）はファイル名だけから決まるため変わらない。
`src/apps/`・`src/ui/fonts/`・`src/ui/icons/` は空になり削除した。
`build_font.py` は `src/` 以下の全ファイルから文字を収集するため、移動で収録文字は変わらない（新しい非ASCII文字は追加していない）。

### 依存の静的確認

| 確認 | 結果 |
|---|---|
| `ui/`（list・rendering・graphics・overlays）から `app/`・`features/`・AppRegistry・FrameModel・ScreenId・WatchFace への参照 | なし（`Gfx.h` のコメントにWatchFaceの語があるのみ） |
| `ui/rendering/Renderer` のinclude | `Element.h`・`Gfx.h` のみ |
| 機能間の相互include | なし。機能から `app/` へは `AppScreen.h`（画面契約）とランチャーの `AppRegistry.h` のみ |
| `ScreenManager` の所有 | `StopwatchService` はコンストラクタ引数の参照を `StopwatchScreen` へ渡すだけ。レイヤー・Renderer・BootShutdownへの参照なし |
| `WatchFace.h` のinclude元 | `HomeLayer.h`・`DigitalWatchFace.h` のみ |
| `InputController.h` のinclude元 | `AppScreen.h`（Events）・`LauncherController.h`（Events）・`Hal.h`（InputSnapshot）。いずれも入力型の利用で、`TimeUs` だけを得るためのincludeはない |
| 移行用の残存 | `ScreenModel`・`composeHomeRegion`・`NavigationState`・`homeRegion` フィールド・`now_`・`SettingsScreen::stats()`・`apps/` 参照なし |

既存の依存として `services/LauncherData.h` が `features/home/DisplayDataSource.h`（ホームのデータ源契約）を実装している（9-1から）。9-4の範囲外として変更していない。

## ホスト・ビルド検証

| 項目 | 結果 | 記録 |
|---|---|---|
| `python tools/test_runtime.py`（開始時） | 6スイートPASS | [基準](20260923-9-4-host-baseline.log) |
| 同（9-4a: LauncherController抽出後） | 6スイートPASS。既存テストは無変更 | [9-4a](20260923-9-4a-host.log) |
| 同（9-4e: ファイル移動のみ） | 6スイートPASS | [9-4e](20260923-9-4e-host.log) |
| `pio run -e m5stopwatch`（9-4e: ファイル移動のみ） | SUCCESS。`src/` からの警告なし | [移動後の製品ビルド](20260923-9-4e-build-m5stopwatch.log) |
| `python tools/test_runtime.py`（9-4b〜d後、追加テスト前） | 6スイートPASS | [9-4b〜d](20260923-9-4bcd-host.log) |
| 同（最終） | 6スイートPASS。ランチャー単体・フレーム構成・統計要求・起動確定時刻のテストを追加 | [最終ホストログ](20260923-9-4-host.log) |
| `pio run`（最終、3構成） | 製品・測定・描画検証すべてSUCCESS（1回目で成功）。警告はM5GFX・M5Unified・ESP-IDFの既存のもののみで、`src/` からはなし | [製品](20260923-9-4-build-m5stopwatch.log)、[測定](20260923-9-4-build-m5stopwatch-measure.log)、[描画検証](20260923-9-4-build-m5stopwatch-render-check.log) |
| `tools/verify_build.py` | 3構成PASS | [製品](20260923-9-4-verify-m5stopwatch.log)、[測定](20260923-9-4-verify-m5stopwatch-measure.log)、[描画検証](20260923-9-4-verify-m5stopwatch-render-check.log) |
| `git diff --check` | ソース・文書はPASS。生ログ（pio出力の行末空白）は証跡として手を加えていない | 2026-09-23の最終確認 |

| 構成 | 検証済みイメージ | 9-3比 | 静的RAM（ビルド表示） |
|---|---|---|---|
| 製品 | 1,066,480 bytes | +3,152 | 38,420 bytes（9-3: 36,924、+1,496） |
| 測定 | 1,070,800 bytes | +3,120 | 55,564 bytes |
| 描画検証 | 1,065,312 bytes | -576 | 55,252 bytes |

製品の静的RAMの増加のうち8 bytesは9-4a・移動の時点（36,932 bytes）、残りは `Application` の静的化による。
サイズはビルド結果であり、実機上の動作や描画速度の証明には用いない。
書き込み保護のテスト（`test_upload_guard.py`）は対象の `tools/upload_guard.py` を変更していないため再実行していない。
Git Bashから実行する場合はPATHにMSYS2 UCRT64の `bin` が必要。

## 追加・変更したホストテスト

- `tests/TestScreens.h`（新規）: `ScreenManager` が借りる `StopwatchService`・`RuntimeSettings` を基底で先に所有するテスト用の組み合わせ。単体の画面遷移テストは `TestScreens`、Runtimeを通すテストは `Application` で組み立てるよう置き換えた。期待する操作と遷移は変えていない。
- `ui_tests`:
  - `launcherController`: 時計のタップ領域、横ドラッグの無視、遷移途中のBで起動先を返すだけ（開かない）こと、`suspend()` で遷移完了・期限なし・一覧表示の保持、遷移途中の先頭下引きで時計へ戻る／短い引きで一覧へ戻る、スクロール中の退場で行と位置を保持し `home()` で先頭へ初期化。
  - `composition`: 遷移率0/0.5/1の時計の領域と一覧の入口の一致、画面を開くと遷移率によらず時計と一覧を隠し当該画面だけを表示、描画順（ホーム先頭・一覧が画面より背面・トースト最前面・重複なし）、画面切替時だけ `changed`（同一画面内のスクロール・編集画面・通知では立たない）、`ScreenManager` のフレームでの時計可視性。
  - 旧 `homeRegion` の確認は `composeFrame(model).home` へ置き換えた。
- `settings_tests::statisticsRequest`: `SettingsScreen` 単体が情報画面の操作で `enableStats` を返すだけで、戻るボタンでは返さないこと。`ScreenManager` 経由でアプリの `RuntimeSettings` に反映され、フレームとホーム後も持続すること。
- `multifirm_tests`:
  - `successfulBootEndsTheMeasurement` を `Application` 経由に変更。起動確定をUIが最後に見た時刻より250ms後に行い、計測値が確定時点の単調時計で止まることを確認。
  - `shutdownIsTheApplicationsAlone`（新規）: 起動できない外部詳細の出入りと起動失敗で計測が止まらないこと。

追加テストの有効性は、次の一時的な改変でそれぞれテストが失敗することを確認し、元に戻した（一時ビルドのため記録なし）。

| 改変 | 失敗したテスト |
|---|---|
| `composeFrame` で開いた画面が時計を隠さない | `ui_tests::composition` |
| `ScreenManager` が `enableStats` を適用しない | `settings_tests::statisticsAction` |
| `AppShutdown` がコールバック時刻より250ms前で止める | `multifirm_tests::successfulBootEndsTheMeasurement` |
| `LauncherController::suspend()` が一覧の動きを完了しない | `ui_tests::launcherList` |

## 挙動の扱い

見た目・操作・期限・計測の仕様は変えていない。既存テストの期待値は変更せずに通る。構造の変更に伴う細部は次のとおり。

- 起動確定時の計測停止時刻が、UIが最後に処理した時刻から、コールバック時点の単調時計に変わった。描画から確定までの数十ms分だけ停止時刻が後ろになる（計画4節の要求どおり）。
- 文字盤が一つも選択できない状態（選択失敗かつ前の文字盤の復旧も失敗）では、以前は何も描画しなかったが、今は時計以外のレイヤーを描画する。初期化で選択に失敗した場合は従来どおり起動を中止する。
- 統計チップの計測開始は、統計表示が有効な時だけ取っていた時刻を毎フレーム取るようにした（区間は同じ）。
- 開いた画面がある時の時計の描画領域は、空のクリップ `{0,0,468,0}` から `{}` になった。どちらも描画・消去の対象がなく、画面切替のフレームは全面再描画なので画素は変わらない。
- `Application` を静的領域に置いたため、静的RAMが1,488 bytes増え（上表のRAM使用量）、UIタスクのスタックから同程度が外れた。スタック残量は9-5の `[RenderDiag] stack_free` で確認する。

## 9-5への引き継ぎ

9-4では実機への書き込み・画素比較・目視・性能/メモリ測定を実施していない。9-5で少なくとも次を確認する（[親計画](plan.md)の9-5チェックリストへ集約済み）。

- 描画検証版の全チェックPASS。今回は各レイヤーの入力の渡し方と描画順の組み立てを変えたため、9-0の234件・9-2・9-3の追加項目をすべて再実行する。特に `alternate-face`・`overlap-foreground`・`digital-restored`（文字盤切替の全面再描画がHomeLayerの `forceFull()` 経由になった）、`capacity-overflow`・`capacity-recovery`、`wake-invalidate`、各画面の `*-left` と再入場。
- 文字盤の登録・選択失敗時の復旧・キャッシュの確保と解放（`[Verify] lifecycle internal_free_before/after`）。
- 統計チップの表示と、dirty矩形外でpushしないこと（計測値への影響がないこと）。
- 起動中表示→起動確定の順序と、起動確定での計測停止（実機のMultiFirm経由）。
- `[RenderDiag] stack_free`・内部RAM/PSRAMの空きと最大連続領域を9-0と同条件で比較し、`Application` の静的化による差を記録する。
