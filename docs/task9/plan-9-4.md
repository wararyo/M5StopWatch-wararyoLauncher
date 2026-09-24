# 9-4: 構成・所有権・配置の整理

2026-09-23。状態: 実装・ホスト検証済み。実機確認は9-5。前提: [9-2](plan-9-2.md)・[9-3](plan-9-3.md)。
結果は[9-4の検証記録](9-4-validation.md)。
[親計画](plan.md)の責務境界を完成させる。新機能・描画方式・操作仕様は増やさない。実機検証は9-5。

## 1. 依存と所有権の最終形

| 所有者 | 所有対象・役割 |
|---|---|
| アプリ初期化・構成部 | サービス、RuntimeSettings、画面、描画構成の寿命を確定し、参照を注入 |
| ScreenManager | 現在画面、入退場、ホームの優先処理、起動先への振り分け、通知の寿命 |
| LauncherController | ホーム↔一覧の遷移、ジェスチャーの振り分け、ランチャー専用ListController |
| HomeLayer/文字盤管理 | 文字盤の登録・選択・begin/end・キャッシュ・次回更新期限 |
| アプリの描画構成（例: HostRenderer） | RenderPortを実装。フレームを各機能の入力へ分配し、描画順・全面再描画条件を決定 |
| 共通Renderer | FramePlanの実行、全消去、順序付きpaint、転送。具体的な画面やアプリのIDを知らない |

アプリ構成は既存のmain.cppから必要な部分だけ切り出す。サービスごとの抽象層や依存注入フレームワークは作らない。
大きいキャッシュ・描画オブジェクトは既存同様スタック外へ置き、UIスタック8KiBを圧迫しない。

## 2. フレームと描画構成

`ScreenModel : AppListModel` と `NavigationState : AppListModel` を解消する。
全体モデルは `FrameModel` 等の名前で、Viewport、Home描画領域、一覧、設定、外部、計測、通知等を明示的に合成する。
更新期限のための時計可視性は構成側の一つの判断を使い、Runtimeが一覧のtransitionを直接解釈する依存をなくす。

モデルから描画領域を作る処理を一か所に集める。実運用と診断は同じ構成処理を使い、
呼び出し元が `composeHomeRegion()` を呼び忘れると時計が消えるような状態を残さない。
各Layerは9-1で導入した小さな入力を維持する。

共通Rendererへは、固定容量の順序付き描画対象（plan/paintの契約）と全面再描画要求を渡す。
具体的なインターフェースは最小限にし、毎フレームのnew・std::functionによる動的確保を使わない。
フレーム準備時に各レイヤーへ入力を固定し、FramePlanの解決中にモデルを書き換えない。
消えた対象の旧矩形を空として登録するか、構成変更として全面再描画・履歴無効化を行う。
FramePlan容量超過では登録できなかった対象もpaintされることを維持する。

描画順はホーム→一覧→内蔵画面→トースト→統計。StatsOverlayの独自領域復元は維持し、
アプリ構成から共通Rendererの転送前フックへ接続する。
描画時間の終点はendWriteによる転送完了後とし、統計記録と計測分類はアプリ側の診断アダプターで行う。
共通RendererからScreenIdや機能別RenderDiagnosticsへの依存を除く。

## 3. ホーム・一覧と文字盤

ScreenManagerからホーム/一覧のdrag、transition、補間時刻、端ジェスチャーの判断をLauncherControllerへ移す。
9-2のListControllerに機能固有のホーム判断を戻さない。画面を開く要求はLaunchTargetId/起動先として返し、
画面のenter/exitはScreenManagerが実行する。
退場時に表示用アニメーション期限を止め、選択・スクロールは戻り用に保持する。ホームでは初期化する。

文字盤の登録数・重複IDの拒否、選択失敗時に前の文字盤へ復旧する挙動を維持する。
文字盤切替は全面再描画を要求する。フレームごとの再初期化や全画面スプライトは追加しない。
文字盤管理が先に破棄されて借用ポインタが残らない所有順序を定義する。
9-1のWatchFace契約（DrawRegion/WatchDataのみ）は変えない。

## 4. サービス・設定・外部起動

StopwatchServiceをScreenManagerのメンバーからアプリ全体の所有へ移し、StopwatchScreenへ注入する。
ScreenManagerはBootShutdownの実装をやめ、アプリ側の終了処理オブジェクトをSlotServiceへ登録する。
停止時刻は起動確定のコールバック時に単調時計から取得する。UIが最後に保持した古い時刻を暗黙に使わない。
ワーカーから画面や計測状態を直接変更せず、既存のUIタスク上の確定契約を維持する。

次の順序は変えない:

1. 外部起動の決定後、BootCommittingの表示を描画する。
2. Runtimeが描画後に起動要求を確定する（消灯中でも要求を取り残さない既存経路を維持）。
3. 起動が確定した時点だけ終了処理で計測を停止する。検証失敗や普通の画面退場では停止しない。

RuntimeSettingsもアプリ全体が所有する。SettingsScreenは統計有効化要求をScreenOutcomeへ返し、
アプリ側が適用してdirtyを立てる。RuntimeSettingsへの直接書き込みポインタとstats()の迂回取得を除く。
輝度プレビューと保存は9-1のEffectiveSettings経路を維持する。
SettingsStore/TimeService呼び出しを追加の汎用コマンド基盤へ置き換える作業は含めない。

## 5. ファイル配置と順序

- [x] **9-4a** LauncherControllerを抽出し、ScreenManagerのナビゲーションを整理。
- [x] **9-4b** HomeLayer/文字盤管理とHostRendererを用意し、共通Rendererへ機能非依存の対象だけを渡す。
- [x] **9-4c** 全体モデルを合成へ変更し、診断・Runtimeの可視性/活動状態/計測入力を同じ構成経路へ揃える。
- [x] **9-4d** StopwatchServiceとRuntimeSettingsの所有・操作要求・終了処理をアプリ側へ移す。
- [x] **9-4e** 残るScreen/Layer/Layoutを各features配下へ移し、共通部品をuiの下位フォルダへ整理。

ファイル移動と動作変更を分け（実施では9-4aの後に移動だけを先に行い、ビルドを確認してから9-4b〜dを進めた）、CMake・ホストテストのソース一覧・include・資産埋め込み・診断・文書を更新する。
フォントは共通graphics、ランチャー固有アイコンはlauncher側へ配置する。資産パス変更時は
PlatformIOとCMakeの埋め込み指定、および参照するリンカーシンボルを同時に更新する。
9-3が終わった互換関数・移行用include・古いDisplayModel参照を残さない。

## 6. 検証と完了条件

ホストで、ホーム優先、遷移途中の入力、両リストの退場/再入場と期限、設定のキャンセル、
画面退場・消灯中の計測継続、起動失敗と起動確定時だけの停止、統計有効化の持続を確認する。
構成処理のテストで文字盤の可視範囲と期限、描画順、構成切替の全面再描画要求を確認する。
M5GFXが必要な文字盤の寿命・描画は診断ケースを維持し、実機確認を9-5へ送る。

依存確認では `ui/list` と `ui/rendering` からLaunchRegistry・FrameModel・ScreenId・features・WatchFaceへの
includeがないこと、ScreenManagerがStopwatchService/具体的描画を所有しないことを確認する。
共通型取得だけのWatchFace/InputController依存も確認する。
製品・測定・描画検証の3構成をビルド・verify_buildし、ホストテスト結果とともに `9-4-validation.md` に記録する。

完了条件: フォルダだけでなく依存と所有が上記境界になり、既存操作・期限・計測のホスト回帰が通る。
親計画の責務表・構成・WatchFace契約を更新し、9-2/9-3の未実施実機項目を9-5のチェックリストへ集約する。
性能合格や実機確認済みとは扱わず、9-0の条件での測定を残す。手動操作は事前にユーザーへ所要時間と内容を案内する。
