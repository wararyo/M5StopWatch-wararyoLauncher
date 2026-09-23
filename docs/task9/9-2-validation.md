# 作業9-2: 共通リストとトースト抽出の検証

実施日: 2026-09-23。対象は[9-2の実装計画](plan-9-2.md)。
状態: ホストでの実装・検証完了。実機が必要な確認は既定どおり9-5へ引き継ぐ。

## 実装結果

| 配置 | 内容 |
|---|---|
| `ui/graphics/Gfx.h`、`IconBitmap.h` | 描画面の型とアイコンの汎用ビュー。`WatchFace.h`・`Text.h`・`VlwFont.h`・各Layerはここから `Gfx` を得る |
| `ui/list/ListModel.h` | `RowId`、`ListRow`（ID・ラベル・任意アイコン・色・薄い表示・決定可否）、件数付き `ListRows`、`ListState`、`ListDecision` |
| `ui/list/ListLayout.h` | 旧 `ui/ListLayout.h` を移し、`ListPlacement`（DrawRegion＋scroll）を入力にした。可視範囲・ヒットテスト・最大可視行数・表示スロット数を追加 |
| `ui/list/ListController.*` | 選択・A循環・B/タップ決定・ドラッグ・慣性（Hermite）・慣性停止後の決定抑止・整列・退場・期限。行数と行IDは入力 |
| `ui/list/ListView.*` | 表示スロット、Element履歴、省略結果の保持、文字画像キャッシュ、スロット不足時の直接描画 |
| `ui/overlays/ToastLayer.*` | トーストの矩形・省略・Element・plan/paint。表示/変更/消去時の全面再描画も内容比較で自身が要求 |
| `features/launcher/AppListLayer.*` | AppRegistryとスロット名から行を組み立ててListViewへ渡す |
| `features/launcher/AppListRows.h`、`AppListLayout.h`、`AppIcons.*` | 行ID（=AppId）、スロット反映、アイコン色、遷移率からの配置、ホームのタップ領域、IconIdからの埋め込み画像解決（旧 `ui/IconSet.*`） |

- `Renderer` から行配列・一覧描画・トーストの矩形/文字整形・AppRegistry参照を除いた。所有するのは `AppListLayer` と `ToastLayer` で、描画順（文字盤→一覧→内蔵画面→トースト→統計）は不変。
- `ScreenManager` の一覧ドラッグ・慣性・停止処理を `ListController` へ移した。ホーム↔一覧の遷移補間と、ドラッグ開始時の所有者決定（時計/一覧先頭の下引き/スクロール）は `ScreenManager` に残した。
- `AppListModel` は `ListState`・遷移率・行ごとの名前と薄い表示を持つ。固定5件と最大添字4は共通部品とScreenManagerから除き、件数は `AppRegistry.size()` 由来の入力になった。起動先は選択添字ではなく行IDから引く。
- `FramePlan::forceFull()` を追加した。トーストの出現/消失とリストの直接描画退避が、plan後に全面再描画を要求する。
- 設定トップは9-3まで `settingsMenuRow()` の互換関数を残したが、中身は共通 `layoutListRow()`（アイコンなし）への委譲で、円弧計算の複製はない。見た目と操作は変わらない。

### 容量

- 表示スロット数 `ListVisibleSlots=8`。正方形パネルでは行間84px・行高71pxに対し468px内の最大可視行は7（400/466/468pxで確認）。行 `i` はスロット `i % 8` を使う。
- FramePlanの最大登録数は 文字盤5＋リスト8＋トースト1＋設定11＝25（容量32）。非表示のリストも旧矩形を消すため8件を空で登録する。
- 可視行がスロット数を超えた場合は全面描画にし、可視行をすべてキャッシュなしで直接描画する。次のフレームも全面描画にして記録外の行を消す。FramePlanの容量超過とは別経路。

### キャッシュ

| 項目 | 内容 |
|---|---|
| 省略結果 | スロットごと。キーはRowId・元文字列の内容（95 bytesまで保持、超える場合は毎回再整形）・フォント・倍率・利用可能幅。位置は含まない |
| 文字画像 | スロットごと、表示行のみ。RGB565（16bit）の `M5Canvas`、PSRAM。キーは省略後の文字列・文字色・フォント・倍率。背景は黒固定 |
| 最大量 | 468pxで幅 ≤ 338+8px、高さ = 行高34+12px → 1スロット最大31,832 bytes、8スロットで最大254,656 bytes。実際は文字幅分のみ確保 |
| 確保 | 内容またはサイズが変わった時だけ。同サイズは同じバッファに再描画。選択色の変化は再描画のみ |
| 失敗 | 直接描画へ退避し、同じキーは再確保しない（キーが変わるまで） |
| 無効化・解放 | フォント・倍率の変更はキーで無効化。`releaseCache()` で全解放。非表示中も保持し、再入場で再利用 |

画素一致の根拠: M5GFXのVLW描画は背景色指定時に下地を読まず、`drawString` の位置計算は与えた座標からの差分のみで平行移動に不変、パネルはRGB565のフレームバッファ、グリフの左はみ出しは最大2pxで余白4px以内。ただしこれは設計上の根拠で、実機の画素比較は未実施。

アイコン合成画像のキャッシュは追加していない（計画どおり9-5の内訳測定で判断）。

## 挙動の扱い

アプリ一覧の見た目・操作・起動先は維持した。既存のnavigation/flickテストは期待値を変えずに通る。
構造の変更に伴い、次の細部だけ変わる。

- 一覧から画面を開く時、進行中のスクロール・ホーム→一覧の遷移は目標位置で即時完了する。以前は隠れた一覧が最大180ms背後で描画を続け、遷移途中なら開いた画面の下に時計が見えることがあった。戻った時の位置は同じ。
- ホームへ戻る遷移中（180ms以内）にホームでA/Bを押した場合、以前は一覧スクロールがその位置で止まったが、今は選択行への整列を続ける。
- トーストによる全面再描画の判定を、文字列ポインタの比較から内容の比較にした。現行の通知はすべて固定文字列なので結果は同じ。
- 描画診断のモード分類は一覧の `dragging/animating` を使う。ホームで下方向に引いて遷移率0のままのフレームは、以前はスクロールに、今は単発に分類される。

## ホスト・ビルド検証

| 項目 | 結果 | 記録 |
|---|---|---|
| `python tools/test_runtime.py`（9-2a/9-2c時点） | 6スイートPASS | [9-2a](20260923-9-2a-host.log)、[9-2c](20260923-9-2c-host-2.log) |
| 同（最終） | 6スイートPASS。共通リストの配置・操作、ランチャー退場、FramePlanの全面化を追加 | [最終ホストログ](20260923-9-2e-host-2.log) |
| `python -m unittest discover -s tests -p test_upload_guard.py` | 6テストPASS | [書き込み保護ログ](20260923-9-2-upload-guard.log) |
| `pio run`（9-2b・9-2d時点） | 製品・描画検証SUCCESS | [製品](20260923-9-2b-build-product.log)、[描画検証](20260923-9-2b-build-render.log) |
| `pio run`（最終） | 3構成SUCCESS。測定版は2回ESP-IDF内部（wpa_supplicant、libesp_common.a）でメッセージなしに失敗し、3回目にPowerShellから成功 | [製品](20260923-9-2e-build-m5stopwatch.log)、[測定](20260923-9-2e-build-m5stopwatch-measure-retry2.log)、[描画検証](20260923-9-2e-build-m5stopwatch-render-check.log)、失敗: [1](20260923-9-2e-build-m5stopwatch-measure.log)・[2](20260923-9-2e-build-m5stopwatch-measure-retry.log) |
| `tools/verify_build.py` | 3構成PASS | [製品](20260923-9-2-verify-m5stopwatch.log)、[測定](20260923-9-2-verify-m5stopwatch-measure.log)、[描画検証](20260923-9-2-verify-m5stopwatch-render-check.log) |
| `git diff --check` | PASS | 2026-09-23の最終確認 |
| 境界の静的確認 | `ui/list`・`ui/overlays`・`ui/graphics` にAppRegistry・ScreenId・SlotStatus・SettingsView・FrameModel・WatchFace・features参照なし。Rendererに `fitText`・行配列・トースト矩形・AppRegistry参照なし | `rg` による確認 |

途中、`tests/runtime_tests.cpp` の旧includeでコンパイルに失敗し修正した（ログは修正後の再実行で上書き）。
Git Bashから実行する場合はPATHにMSYS2 UCRT64の `bin` が必要（ないとg++が無言で失敗する）。

追加したホストテスト:

- 配置: 件数0/1/2/5/7/12/40 × 400/466/468px × 遷移率 × スクロール。可視範囲が描画矩形と一致、可視行数がスロット数以下、描画矩形の中心と右端でのヒットテスト、アイコンなしの文字起点。
- 操作: 空（選択なし・決定なし・期限なし）、1件（スクロールなし）、7件のA循環と各行のタップ、連打の再目標、決定不可行、ドラッグのクランプと非決定、慣性中タッチの停止と解放時の非決定、スクロール中のヒットテスト、退場（finish）と取消（cancel）、行の並べ替え・削除時の選択追従、所有者への引き渡し。
- ランチャー: アニメーション中に画面を開いた時の即時完了と期限解除、遷移途中からの起動、縦スクロール後に決定しないこと、ホームでの初期化。
- FramePlan: 登録後の `forceFull()` で全要素がpaintされること。

検証済みイメージのサイズは製品1,063,152 bytes（9-1比+4,192）、測定1,067,280 bytes、描画検証1,063,584 bytes。
サイズはビルド結果であり、実機上の動作や描画速度の証明には用いない。

## 9-5への引き継ぎ

実機への書き込み・画素比較・性能/メモリ測定は9-2では実施していない。描画検証版に次の診断を追加した。

- `list-image*`: 文字画像ありの全面描画と、画像なし（グリフ直接描画）の全面描画の画素比較。各選択行、薄い表示＋スロット名、長い日本語＋未収録文字、空文字、内容変更、同名別ID、設定からの再入場、確保失敗中、復帰後。
- `list-content*`・`list-same-name`・`list-empty-name`: 差分描画と全面描画の比較。
- `list-scroll-cached`: 2回目のスクロール掃引で省略・画像描画・確保が増えないこと。
- `list-slot-reuse`・`list-slot-restored`: スロット3個に制限し、遷移率とスクロールを掃引して再利用と直接描画退避を比較。
- `list-alloc-fail*`: 確保失敗時の直接描画と、同じキーを再確保しないこと。
- 終了時に `[Verify] list cache bytes/allocations/failures/fits/renders`、測定版の定期出力に `[RenderDiag] list_cache ...` を出す。

9-5では少なくとも次を確認する。

- 描画検証版の全チェックPASS（9-0の234件に上記が加わる）。文字画像が直接描画と一致しない場合は、`ListView` の `imagesEnabled_` を既定falseにして原因を調べる。
- 一覧の見た目（長い名前、日本語、薄い表示、トースト、遷移中）の目視と、9-0と同条件での描画時間・実fps・フレーム間隔・入力遅延。文字画像の効果は直接描画との比較で内訳を取る。
- 内部RAM/PSRAMの空き・最大連続領域と、一覧↔各画面の往復でメモリが減り続けないこと。
- 30fps超・定常33.3ms以内の達否は、共通化の完了とは分けて記録する。
