# 10-4: 背景復元と一覧の被覆

作成日: 2026-09-25。状態: 実装計画。上位仕様は[作業10](plan.md)。
依存は10-2の表示環境契約と10-3のDigital。Forest本体に先立ち、多色の診断背景で成立を確認する。

## 1. 採用する方式

現状のElementは旧矩形・新矩形・fingerprintから再描画を決め、重なる要素へ変更を伝播する。
Rendererは旧領域を黒で消す。全面背景を通常Elementにすると伝播で更新範囲が全面へ広がる。

作業10では、**変更した要素の旧・新矩形から更新領域を作り、その領域を背景から前景まで再描画する**方式を採用する。
画面転送が単一の外接矩形になる現状に合わせ、最初は更新領域も1つの外接矩形とする。
離れた変更がある場合に間の領域も描き直すが、複数矩形の分割・タイル化は今回の必須範囲にしない。

1. 全planで変更要素と旧・新矩形を収集する。
2. 変更要素の旧・新矩形、および明示的な背景変更領域の和をdamageとする。
3. damageを画面内へクリップし、空なら描画しない。
4. damage内の基底背景を復元する。
5. damageと交差する現存要素を、背景から前景へ順に再描画する。変更していない前景も対象。
6. StatsOverlayを既存の最前面規約で描き、1回転送する。

damageと交差した大きな背景や前景の矩形をdamageへ再加算しない。再描画はdamage内に必ずクリップする。
背景復元後の一度の合成でAAの縁を描き、前フレームの縁へ重ね続けない。

## 2. FramePlanの変更

`ui/rendering/Element.*`へ、変更判定と描画判定を分離する処理を追加する。

- addは従来どおりElement履歴と比較する。
- resolveでは変更要素からdamageを算出し、現在の矩形がdamageと交差する要素をpaint対象にする。
- `damage(Rect)`相当で背景構成の変化を明示できるようにする。背景変更の検出は機能レイヤーが行う。
- 全面再描画・容量超過では画面全体を描く。登録に失敗した要素も既存どおり描画対象にする。
- 消失要素は空の新矩形を登録し、旧矩形からdamageを生成する。
- 背景は通常の全面Elementではなく、レイヤーのpaintでdamageに応じて再生する。

移行途中は従来resolveを既定にした明示モードを設け、全レイヤーのクリップ移行後に新方式へ切り替える。
検証後は旧方式を削除し、恒久的に2つの描画エンジンを維持しない。

## 3. クリップ契約を全レイヤーへ適用

`RenderLayer::paint`へ`PaintContext`を渡す案とする。contextはdamage、画面領域を保持し、
`clip(gfx, elementRect, layerClip)`で3者の交差だけを設定する。空クリップでは描画を呼ばない。
WatchFace::paintもこのcontextを受け取る。局所クリップ解除後に描画を続ける場合はdamageへ戻す。

対象:

- `features/home/`とDigitalWatchFace。
- `features/launcher/AppListLayer`、`ui/list/ListView`（文字キャッシュ転送を含む）。
- `features/settings/SettingsLayer`、`features/stopwatch/StopwatchLayer`、`features/external/ExternalLayer`。
- `ui/overlays/ToastLayer`、`ui/overlays/StatsOverlay`。
- `host/HostRenderer`、`ui/rendering/Renderer`、描画診断の偽WatchFace等。

setClipRect・clearClipRect・fillScreen・pushSprite等を検索し、damageを広げたり無視したりする経路を点検する。
StatsOverlayはdamageに交差したとき自身の矩形を塗り直す既存例外として維持できるが、転送領域拡大を計測へ含める。
統計チップを無効にしたピクセル一致検証と、チップを有効にした目視・転送検証を分ける。

## 4. 一覧背景と可視性

`features/launcher/AppListBackgroundLayer`を追加し、FrameOrderをHome→AppListBackground→AppList→各画面→Toastにする。
背景色は選択中のWatchFaceのthemeから取得する。settings等の画面へこのテーマを暗黙に適用しない。

進捗pに対し一覧上端を`(1-p)*height`とし、その下を不透明背景で覆う。
前フレームの上端・色と現在値を比較してdamageを要求する。行も同じ上端・被覆クリップで描く。
背景が変わらず文字や行だけ動くフレームでは、一覧背景の全面を変更扱いにしない。

共通ListViewへ背景色を描画入力として渡し、以下へ反映する。

- 文字キャッシュの塗りつぶしとキャッシュキー。
- 直接描画の文字背景色。
- 円形アイコン外周のAA合成色。マスク内部は従来のアイコン色。

初期テーマは両文字盤とも黒。診断では非黒色を注入して黒い矩形や縁が残らないことを確認する。
任意の明るい背景に対する自動配色までを本作業へ含めず、将来の文字盤は既存前景色が読める背景を選ぶ契約とする。

## 5. 時計の移動を文字盤へ移す

FrameComposerと`features/launcher/AppListLayout.h`の`launcherHomeRegion`を見直す。
システムはviewport・一覧進捗・一覧で覆われない領域を渡し、homeのoffsetYを決めない。
Digitalは`-p*height`を自身で適用し、描画矩形を被覆クリップへ交差させる。Forestは静止する。

RuntimeとHostRendererは同じ合成関数で可視性を判断する。
一覧p=1または他画面表示中は時計期限なし。戻りでp<1になった最初のフレームに時計・情報を再取得する。
途中反転では現在の進捗を起点にし、文字盤選択でLauncherControllerの進捗をリセットしない。

## 6. 実装順序

1. FramePlanへ新方式を追加し、純粋な矩形計画テストを作る。
2. PaintContextを追加し、全paintをdamageとの交差へ移行する。まだ旧resolveで動作を確認する。
3. 診断用の固定多色背景を追加し、新方式へ切り替える。
4. 一覧背景レイヤー、ListViewの背景色とキャッシュ無効化を追加する。
5. FrameComposerからDigitalへ移動責務を移し、可視期限と接続する。
6. 全面描画との一致・既存画面の回帰を確認して旧resolveを削除する。

## 7. 検証と完了条件

ホストのui_testsで、旧領域のみ／新領域のみ／消失／離れた変更／AA分の外周／空クリップ／容量超過を検証する。
「小さな文字変更が背景と交差してもdamageが全面へ広がらない」を明示的にテストする。

実機の描画検証では次を全面描画と比較する。

- 背景上の数字の更新と削除、重なった不変前景、トーストの出現・消失。
- p=0、微小値、0.5、1直前、1、途中反転、ボタン開始とドラッグ開始。
- 黒／非黒一覧背景、キャッシュ有無・失敗、背景色変更後の文字キャッシュ。
- 設定メニュー・編集画面・外部詳細・ストップウォッチ、復帰、文字盤切替、FramePlan超過。

一覧スクロール・遷移・小さな数字更新の描画時間と実転送範囲を測定する。
全画面中間バッファを増やさず、既存未達を隠さず比較する。ログと結果は`10-4-validation.md`へまとめる。
全体計画への補足候補: 背景復元は全paintのクリップ規約変更を伴うこと、最初は単一damage矩形を使うこと。
