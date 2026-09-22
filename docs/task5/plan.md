# 作業5：ストップウォッチの実装計画

作成日: 2026-09-22。設計の基準は [全体計画](../plan.md) の4・5.1・5.3・6章と作業5。
本書は実装前に決めたことの記録であり、検証完了の記録ではない。
結果は [作業5の検証記録](task5-validation.md)、構成と手順は [ストップウォッチ](stopwatch.md) を参照。

## 到達点と範囲

一覧の先頭からストップウォッチを開き、開始・停止・再開・リセット・ラップを
物理ボタンとタッチの両方で操作できる。経過時間は `HH:MM:SS.cc` で40Hz更新する。
ホーム・消灯・他画面を挟んでも計測は続き、外部アプリを起動したときだけ黙って終わる。

UIは [M5StopWatch-UserDemo](../../M5StopWatch-UserDemo/main/apps/app_stopwatch) の
ストップウォッチを写す。含めないもの: 押下時のバネアニメーション、上端の弧に沿う現在時刻
（UserDemoの `ArcTopClock`）、ラップ一覧のスクロールと4件以上の保持、ラップの永続化、
計測中であることの時計・一覧への表示、経過時間の再起動をまたぐ復元（全体計画1.1節）。

あわせて、作業6が完了条件のうち計測に触れる2項目を作業5待ちで残しているので、これを消化する
（[作業6の検証記録](../task6/task6-validation.md) の「作業5の後に消化する項目」）。

## 全体計画の改訂

UserDemoのUIを写すと、全体計画のストップウォッチ仕様と3点食い違う。実装より先に
[全体計画](../plan.md) を改訂し、8.2節を改めたときと同じように理由を本文へ残した。

1. **ラップ記録なし → あり**（1章の機能表、5.3節、9章の検証表、作業5）。
   UserDemoではLAPボタンとラップ一覧が画面構成の中心にあり、外して真似る意味が薄いため。
2. **5.1節を、A＝次へ／B＝決定は既定であって内蔵アプリ画面はA/Bへ画面ごとの意味を
   割り当ててよい、という書き方へ改める**。ストップウォッチを例外として名指しせず、
   アプリ側の裁量として一般化した。短押しをリリースで確定すること、A+B連続600msがホームで
   あること、ホームを画面固有処理より優先することは例外なく全画面へ適用する、と但し書きを添えた。
3. **5.3節の「戻る」を削除**し、**表示更新周期を25ms（40Hz）と明記**した。
   A/BがSTART/STOPとLAPで埋まるので一覧へ戻る手段は持たず、A+Bのホームで時計へ戻る。

## 設計上の要点

### 差分描画とパネル背景

`FramePlan::resolve()` は矩形が交差する要素へ再描画を伝播する。466×330のパネルを1要素として
登録すると、1/100秒が変わるたびにパネルが再描画され、そこからラップ行・仕切り・ボタンへ連鎖して
実質全面描画が40Hzで走る。

そこで**パネルと仕切りは `FramePlan` の要素にせず、全面再描画のときだけ描く背景**とする。
消去は登録要素の旧矩形を黒で塗る処理しかないので、次の2つを守れば消し残しも黒穴も出ない。

- ストップウォッチの全要素は**箱を固定**する。内容が変わっても矩形は不変。
- 全要素は**自分の箱全体にパネル色を塗ってから**中身を描く。黒消去を同じフレームで覆う。

パネルに穴を開けうるのはトーストだけだが、`Renderer::draw` が `m.toast!=previousToast_` で
全面再描画にするため安全。画面切り替えも同様。この不変条件は描画検証版のピクセル比較
（差分描画と全面描画の一致）がそのまま検査する。

### 要素数

経過時間の「時分秒」と「1/100秒」を別要素に分けるのが肝で、40Hzで塗り直すのは1/100秒の
小さい矩形だけになる。時分秒2＋ボタン2＋ラップ行3で**7**。
同時最大は 時計5＋一覧行5＋トースト1＋max(設定11, ストップウォッチ7, 外部6) = **22**（容量32）で、
作業6から変わらない。

### 表示更新期限の経路

`AppRuntime::step()` は `nextDisplay_` を `model.transition<1` のときしか設定しない。
ストップウォッチ表示中は `transition==1` なので `INT64_MAX` になり、
`ScreenManager::nextUpdate()` は待機を起こすだけで再描画を起こさない。

`AppScreen` に `tick(TimeUs)`（既定は偽）を足し、`ScreenManager::update()` が
`now>=active_->nextUpdate()` のとき呼んで戻り値を `changed` に混ぜる。
既定実装が偽なので設定・外部アプリ画面は無変更。`StopwatchScreen::tick()` は経過時間を
再サンプルして期限を25ms進め、真を返す。画面OFF中は `AppRuntime` が `screens_.update()` も
`screens_.nextUpdate()` も参照しないので、期限ごと止まる（全体計画5.3節）。

### 計測値のサンプリング

`ScreenManager::model() const` は `now` を受け取らない。`StopwatchScreen` は
`enter(now)` / `handle(e,now)` / `tick(now)` で経過時間を再サンプルして保持し、
`model()` は保持値を返すだけにする。同一フレームの入力データは不変という全体計画4.1節に沿う。
このために `AppScreen::enter()` へ `now` を足した。設定・外部アプリ画面は引数を捨てる。

### 作業6の残り2項目

`MultiFirmAdapter::shutdown` は起動先設定後・再起動前にUIタスクで同期実行される唯一の接続点で、
失敗時は呼ばれない。`SlotService` へ `BootShutdown` と `bindShutdown()` を足し、
`ScreenManager` が実装して `bindSlots()` で自分を登録する。`onBootCommitted()` は
`stopwatch_.stop()` を呼ぶ。`FakeSlotService` も成功時だけフックを呼ぶので、
成功・失敗の両方をPCで検証できる。

## UserDemoから写す値

`view/view.cpp` の実測値。468基準で `offsetPx` / `scaled` により比率化する。

| 部品 | 位置・寸法 | 色（元→RGB565） |
|---|---|---|
| 左ボタン(A) | 105×72 角丸34、中心(162, 91) | 0xB3CDFF → `0xb67f` |
| 右ボタン(B) | 105×72 角丸34、中心(306, 91) | START/RESET 0x9CF1B6 → `0x9f96`、STOP 0xFF9EAB → `0xfcf5` |
| ボタン文字 | 中央 | `blend_in_difference(bg,0x858585)` = `0x2a4f` / `0x1366` / `0x78c4` |
| パネル（背景） | y=148、466×330 角丸60 | 0x41484B → `0x4249` |
| 経過時間 | 中心 y=198 | 0xD8F2FF → `0xdf9f` |
| 仕切り（背景） | 160×4、中心(234, 248) | 0x58646A → `0x5b2d` |
| ラップ行 | 中心 y=290 / 338 / 386、左右56px余白 | `0xdf9f` |
| ラップなし表示 `-.-` | 1行目の位置 | 0x738086 → `0x7410` |

ラップ行は3行。UserDemoは4行目相当も置くが、y=434付近は円の弦が±142pxしかなく
`LAP n` のラベルが欠けるため、安全域（全体計画2.1節）に収まる3行に留める。

状態とボタンの対応はUserDemoと同一。Resetの左ボタンは押しても何も起きない点も含めて写す。

| 状態 | 左＝A | 右＝B |
|---|---|---|
| Reset | `LAP`（無効） | `START` |
| Running | `LAP` | `STOP` |
| Paused | `RESET` | `START` |

UserDemoが `0` を `O` に置換しているのはMaple Monoの字形都合なので真似ない。

## 実装順序

### 5-1. StopwatchService

- `services/Stopwatch.h`: 状態・ラップ行数・表示上限・更新周期・表示モデル・`formatStopwatch`。
  M5GFXにもHALにも依存しないので、表示モデルとして `ui/DisplayModel.h` から読める
  （`multifirm/SlotCatalog.h`・`storage/Settings.h` と同じ扱い）。
- `services/StopwatchService.{h,cpp}`: `start / stop / reset / lap / elapsed / state`。
  与えられた単調時刻だけを使い、壁時計・描画周期・保存に触れない（全体計画4章）。
  ラップは固定長配列で最新3件と通算番号を持つ。`std::vector` も動的確保も使わない。
  表示上限 `99:59:59.99` は `formatStopwatch` 側で止め、内部値は維持する。

確認: 累積、停止中のラップ拒否、連番、最新3件のリング、上限、同じ押下の反復をPCで検証する。

### 5-2. 画面

- `ui/DisplayModel.h` に `ScreenId::Stopwatch` と `ScreenModel::stopwatch`。
- `apps/StopwatchScreen.{h,cpp}`（`AppScreen` の3つ目の実装）。`enter`/`exit` は計測を触らない。
  A/Bとタップを上表へ割り当て、何も起きない押下は `changed` を返さない。`leave` は返さない。
- `app/ScreenManager.{h,cpp}`: `StopwatchService` と画面を所有し、`AppId::Stopwatch` で開く。
  `update()` に `tick` 経路を足す。`"準備中"` の分岐は**残す**（`available()` が偽で
  開けなかったときの退避）。これでフォント部分集合が変わらず、`.vlw` の再生成が要らない。

確認: 状態ごとのA/B、Resetでの左ボタン無効、タップの当たり判定、退場とホームで計測が続くこと、
Running時だけ期限が出ること、消灯中に期限も描画も出ないことをPCで検証する。

### 5-3. 描画

- `ui/StopwatchLayout.h`: 矩形と `hitStopwatch()`。描画とヒットテストで共有する。
- `ui/StopwatchLayer.{h,cpp}`: `plan` / `paint`。自画面でないときは何も登録しない。
  `frame.full()` のときだけパネルと仕切りを描く。経過時間の文字サイズは
  `textWidth("00:00:00")` から箱に合わせて求め、決め打ちの倍率を置かない。
- 文言はすべてASCII（`LAP` / `START` / `STOP` / `RESET` / `LAP n` / `-.-`）なので
  `tools/build_font.py` の再実行と `.vlw` の再コミットは不要。

確認: 差分描画と全面描画のピクセル一致、1/100秒だけの更新、トーストの重なりと消滅。

### 5-4. 作業6の完了条件の接続

`multifirm/SlotService.h` の `BootShutdown` / `bindShutdown`、`MultiFirmAdapter::shutdown` の
フック呼び出し、`FakeSlotService::boot` の成功時フック、`ScreenManager` の実装と登録。

確認: 起動失敗で計測がRunningのまま進み、起動成功で停止することをPCで検証する。

### 5-5. 検証とドキュメント

- `tests/stopwatch_tests.cpp` を追加し、`tools/test_runtime.py` と `src/CMakeLists.txt` へ
  新規ソースを登録する。`tests/multifirm_tests.cpp` へ5-4の2項目を足す。
- `ui/RenderDiagnostics.cpp` のピクセル比較へ、3状態×ラップ0〜3件、1/100秒だけを進めた連続フレーム、
  桁上がりと表示上限、トーストの重なりと消滅を足す。経過時間フォントの数字が等幅であることの
  検査も足す（等幅でないと固定箱の中で値が横に揺れる）。
- 本書、[ストップウォッチ](stopwatch.md)、[作業5の検証記録](task5-validation.md) を残し、
  [全体計画](../plan.md) と `README.md`、[作業6の検証記録](../task6/task6-validation.md) を更新する。

## 変更した主なファイル

| ファイル | 変更 |
|---|---|
| `docs/plan.md` | ラップあり・A/Bはアプリの裁量・戻るなし・25ms周期への改訂と理由 |
| `src/services/Stopwatch.h` | 新規。状態・定数・表示モデル・書式 |
| `src/services/StopwatchService.{h,cpp}` | 新規。単調時刻による累積とラップ |
| `src/apps/StopwatchScreen.{h,cpp}` | 新規。`AppScreen` の3つ目の実装 |
| `src/ui/StopwatchLayout.h` | 新規。配置とヒットテストの共有 |
| `src/ui/StopwatchLayer.{h,cpp}` | 新規。plan/paint と背景としてのパネル |
| `src/apps/AppScreen.h` | `enter(TimeUs)` へ変更、`tick(TimeUs)` を追加 |
| `src/ui/DisplayModel.h` | `ScreenId::Stopwatch`、`ScreenModel::stopwatch` |
| `src/app/ScreenManager.{h,cpp}` | サービス所有、行の分岐、`tick` 経路、`BootShutdown` 実装 |
| `src/app/AppRuntime.cpp` | `commitPendingBoot(now)` |
| `src/ui/Renderer.{h,cpp}`、`src/ui/Element.h` | レイヤーの接続、要素予算の内訳 |
| `src/multifirm/SlotService.h`、`MultiFirmAdapter.cpp`、`FakeSlotService.h` | shutdownフック |
| `src/CMakeLists.txt`、`tools/test_runtime.py` | 新規ソースの登録 |
| `tests/stopwatch_tests.cpp`、`tests/multifirm_tests.cpp` | 新規スイートと2項目 |
| `src/ui/RenderDiagnostics.cpp` | ピクセル比較と等幅数字の検査 |

`platformio.ini` と `partitions.csv` は変更しない。`checkLayout()` がパーティション表の
バイト完全一致を要求するため、`partitions.csv` は1行も触らない。
`src/ui/fonts/GenShinGothicMedium28.vlw` も触らない。

## 検証と完了条件

PC検証の一覧と実機手順は [ストップウォッチ](stopwatch.md)、結果は
[作業5の検証記録](task5-validation.md) にある。

完了条件: 時刻設定の前後変更で計測値が飛ばない。ホームや消灯を挟んでも計測が継続し、
再開とリセットが正しい。長時間値・表示上限・表示更新性能を確認する。
あわせて作業6の残り2項目（API失敗時の計測継続、成功時の計測終了）を消化し、作業6を完了扱いにする。
性能の再測定と操作中30fps超の判定は作業7で行う。未達は原因と対応を明記し、合格扱いにしない。

## 作業5に含めないもの

- 押下時のバネアニメーション。`Events` は短押しをリリース時にしか出さず、押下保持を
  画面へ渡す経路がない。入れるには入力層の変更と40Hzの追加アニメーションが要る。
- 上端の弧に沿う現在時刻、ラップ一覧のスクロールと4件以上の保持、ラップの永続化。
- 計測中であることの時計・一覧への表示。静止時に1フレームも描かない性質を壊すため。
- 電源断・再起動・外部アプリ切り替えをまたぐ経過時間の復元（全体計画1.1節）。
- 作業7の基準測定と作業8の省電力化。
