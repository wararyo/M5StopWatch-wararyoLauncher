# 10-2 検証記録: WatchFaceのイベント・入力・更新期限

実施日: 2026-09-26。[10-2計画](plan-10-2.md)の実装、ホスト検証、ビルド、実機の描画検証と操作確認を終え、**10-2を完了とした**。

## 1. 実装の要点

| 場所 | 内容 |
|---|---|
| `src/features/home/HomeInteraction.h`（新規） | Gfx非依存の`HomeEvent`（Tap／LongPress、ホーム原点の座標、時刻）、`HomeOutcome`（changed、`OpenAppList`）、`HomeControlPort`、`WatchEnvironment`（viewport・被覆されていないクリップ・一覧進捗）、`WatchChange`ビットと`watchChanges()` |
| `src/features/home/WatchFace.h` | `handle`・`update`・`backgroundInterest`・`listBackground`（一覧背景色、既定は黒）を追加。すべて既定実装つきで、タップ未実装の文字盤も成立する |
| `src/features/home/faces/DigitalLayout.h`（新規） | Digitalの配置（APPSの矩形を含む）と、描画を除く振る舞い`DigitalControl`（バリアント、タップ／長押しの意味、分／秒の期限、表示対象の情報）。ホストテストでも同じものを使う |
| `src/features/home/faces/DigitalWatchFace.*` | `DigitalControl`へ委譲。`HH:mm:ss`（バッファ12bytes）、無効時刻は`--:--:--`。時刻キャッシュはバリアント変更時に1回だけ作り直す（`HH:mm` 51,728 bytes、`HH:mm:ss` 78,864 bytes、内部RAM）。確保失敗は次の変更まで直接描画 |
| `src/features/home/HomeLayer.*` | 前フレームとの差分から変更ビットを作り、`update`→`plan`の順に渡す。選択・全面再描画（復帰）を`WatchSelected`／`WatchResumed`で伝える |
| `src/input/InputController.*` | `Gesture::LongPress`。Runtimeが渡す「ホーム静止中」を押下開始時にラッチし、600msで1回発火、以後は解放までタップ・ドラッグなし。しきい値超えは取消（同一サンプルではドラッグ優先）。静止ホームで始まったドラッグでない押下は、ホームが静止でなくなった時点で消費する（ボタンで一覧が開いた場合など） |
| `src/features/launcher/LauncherController.*` | APPSの固定領域（`appsTarget`）を削除。`atRest()`と`openList(now)`（表示中・遷移中の再要求では再開始しない）を追加 |
| `src/host/ScreenManager.*` | `bindHome`・`homeAtRest`。静止ホームのTap／LongPressだけを文字盤へ渡し、`OpenAppList`を`openList`で実行 |
| `src/host/HostRenderer.h`・`RenderPort.h` | `HostRenderer`が`HomeControlPort`を実装しHomeLayerへ委譲。`RenderPort::backgroundInterest`（既定は空） |
| `src/host/HostRuntime.cpp` | 入力へ`homeAtRest()`を渡す。表示期限に、文字盤が表示する情報のラベル期限を合成（可視の時計に限る） |
| `src/host/HostApplication.h`・`main.cpp` | `bindHome(renderer)` |

計画からの補足・決定:

- 表示対象の情報は、Digitalでは**まだ空**。情報欄は10-3で描くため、描かないラベルで起床しないようにした。10-3で`leadingItems(snapshot,2)`へ変える。Runtime側の合成はテスト用の文字盤で検証済み。
- 変更ビットは、Runtimeからの通知ではなく、HomeLayerが前フレームのスナップショットと比べて作る。非表示の間に起きた変化も、次に描くフレームでまとめて伝わる。Hubの`collect()`の変更ビットは使っていない（診断用に残す）。
- 「一覧開始・ホーム・画面切替は進行中のタッチを消費する」は、ホーム静止中に始まったドラッグでない押下に限った。ドラッグ中の押下まで消費すると、LauncherControllerのドラッグ状態が終わらなくなるため。ホーム（A+B）は従来どおりCancelで消費する。
- 一覧進捗は`WatchEnvironment::listProgress`で渡すが、`DrawRegion`のoffsetによる移動は維持した（計画5節。10-4で移す）。

## 2. ホスト検証

全7スイートPASS（[ログ](20260926-10-2-host-2.log)）。`-1`は追加前の既存テストの確認（[ログ](20260926-10-2-host-1.log)）。

| 計画6節の項目 | 検証 |
|---|---|
| 599／600／601ms、1回発火、微小移動、しきい値越え・戻り、同一サンプルの競合、ホーム同時成立、解放時タップ抑止、復帰タッチ | `runtime_tests: longPress` |
| 他画面での長い押下が従来どおりTap、ホームへ途中復帰した押下、一覧開始後の解放、引き上げドラッグの継続 | 同上 |
| 静止保持で期限に到達（10ms追従）、APPS上の長押しで一覧を開かない、APPSタップで一覧、一覧上の長押しは文字盤に届かない | `runtime_tests: homeGestures` |
| 時分→秒→時分で期限が切り替わり、覆われた時計は起床しない | 同上（秒表示は1秒に1回、分表示・被覆中は0回） |
| APPS内外、未実装タップ（ポート未接続）、遷移中・一覧表示中・他画面のタップ抑止、A/B | `ui_tests: homeInput`、`launcherController` |
| バリアントと期限、無効時刻、タップの意味、配置の縮尺 | `ui_tests: digitalControl` |
| 状態変化と文字列変化、期限だけの変化 | `ui_tests: watchChangeBits` |
| 表示対象の情報の期限、表示しない文字盤・被覆・消灯で起床しない、無効RTCでも情報の期限は有効 | `background_tests: labelsWakeOnlyAFaceThatShowsThem` |

テストの感度: Runtimeの情報期限の合成を外すと`background_tests`が、押下の消費を外すと`runtime_tests`が失敗することを確認した（変更は戻した）。

## 3. ビルド

| 構成 | ビルド | `verify_build.py` | イメージ | 静的RAM（10-1比） | SHA-256 |
|---|---|---|---:|---:|---|
| m5stopwatch | SUCCESS（[ログ](20260926-10-2-build-m5stopwatch-3.log)） | PASS（[ログ](20260926-10-2-verify-m5stopwatch-3.log)） | 1,083,040 | 39,812（+360） | `69f3b2fb8852cc7973d526a2762e4a1495a017d6119349f1f9f6b3c0fd55393f` |
| m5stopwatch-measure | SUCCESS（[ログ](20260926-10-2-build-m5stopwatch-measure.log)） | PASS（[ログ](20260926-10-2-verify-m5stopwatch-measure.log)） | 1,087,376 | 56,956（+360） | `92a4552a7f01d6332c8f2b7236e1b8542e8ac573a1f3153925a264c6355f18a6` |
| m5stopwatch-render-check | SUCCESS（[ログ](20260926-10-2-build-m5stopwatch-render-check-2.log)） | PASS（[ログ](20260926-10-2-verify-m5stopwatch-render-check-2.log)） | 1,083,616 | 56,636（+360） | `589198059e8483e0f1be8c7ac4d2b34f01a84b83c2f7586f5fa8e95c63d3fe74` |

- 製品構成の1回目は、別構成のビルド成果物を参照する`WinError 2`で失敗した（[ログ](20260926-10-2-build-m5stopwatch.log)）。コードに変更なく再実行で成功した（環境起因）。
- 描画検証版は秒表示のケースを加えて再ビルドした（`-2`）。製品版は`#ifdef`外に差分がなく、再ビルド後もハッシュは同じ。
- 自作ファイル由来の警告はない（警告は依存ライブラリのみ）。

## 4. 実機（描画検証）

描画検証版に秒表示のケースを追加した: `seconds`（分→秒の切替とキャッシュ再確保）、`second-tick`、`second-narrow`（プロポーショナルな数字の幅変化）、`seconds-transition`、`seconds-unknown`、`minutes-again`。

- 結果: `[Verify] checks=375 mismatches=0 result=PASS`（[ログ](20260926-10-2-render-check-2.log)）。10-1の369件＋6件。
- 秒表示のキャッシュは`bytes=78864`で確保できた。バリアント切替16回の前後で内部RAMの空きは`202879`で不変。
- 検証後、製品版を導入した（[ログ](20260926-10-2-install-product.log)）。

## 5. 実機での操作確認（2026-09-26、ユーザー確認済み）

- [x] ホームの時計を約0.6秒押し続けると、指を離す前に`HH:mm:ss`へ切り替わる。もう一度で`HH:mm`へ戻る。
- [x] APPSの上で長押ししても、離したときに一覧が開かない。
- [x] APPSのタップで一覧が開き、APPS以外のタップでは何も起きない。上スワイプ・A/B・A+Bホームは従来どおり。
- [x] 秒表示で秒が毎秒進み、ちらつき・残像がない。
- [x] 設定やストップウォッチの画面で、ボタンを長めに押しても従来どおり操作できる（長押しに化けない）。
- [x] 一覧で行を長めに押しても、従来どおりタップとして開く。

6項目すべて問題なし。長押しの600ms（計画3.1節の暫定値）は操作感がちょうどよいとの評価で、このまま採用する。

バリアントの保存は10-5のため、再起動すると`HH:mm`へ戻る（現時点の仕様どおり）。

## 6. 全体計画への補足候補（10-6で反映）

- 長押しはホーム静止中の押下に限る（押下開始時にラッチ）。
- 画面制御と文字盤の境界はGfx非依存の`HomeControlPort`。文字盤の振る舞いを描画から分けると、ホストで本物の判定を検証できる。
- 10-4まで一覧遷移はDrawRegionのoffsetによる移動を維持する。
