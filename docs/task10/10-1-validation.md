# 10-1 検証記録: バックグラウンド情報の契約と供給

実施日: 2026-09-26。[10-1計画](plan-10-1.md)の完了条件を確認し、**10-1を完了とした**。
ユーザー操作を要する項目はない（表示部品は10-3で追加するため、目視確認は対象外）。

## 1. 実装の要点

| 場所 | 内容 |
|---|---|
| `src/core/AppId.h` | `LaunchTargetId`を移し、値を明示（Stopwatch=0〜External3=4）。`LaunchRegistry.h`はincludeするだけ |
| `src/features/background/BackgroundInfo.h` | `BackgroundInfo`（期限・ID・48bytesラベル）、`BackgroundSnapshot`（4件）、`BackgroundInterest`、`BackgroundInfoProvider`。`leadingItems`・`nextChange`で表示対象の期限を求める |
| `src/features/background/BackgroundInfoHub.*` | 登録（同一ID・容量超過を拒否し`rejected()`で数える）、収集、ラベルの終端・UTF-8境界での切り詰め、変更ビット（Added／Removed／Relabeled）、`invalidate()`／`pending()` |
| `src/features/stopwatch/StopwatchBackgroundInfo.*` | Runningのみ。1時間未満`mm:ss`・周期1秒、以降`HH:mm`・周期1分。時は上限なし（`100:00`）。期限は`now+(q-e%q)`でINT64_MAXへ飽和 |
| `src/features/home/HomeModel.h` | `WatchData::background`（所有するスナップショット） |
| `src/host/HostApplication.h` | サービス→供給元→Hub→Runtimeの順に所有。構築時にストップウォッチ供給元を登録 |
| `src/host/HostRuntime.*` | Hubを任意で受け取る（未接続なら空）。**時計が見えるフレームだけ**収集し、`data.sample(now)`の結果へ合成。隠れている間は前回のコピーを渡す（描かれない）。`pending()`は時計が見えるときだけ再描画要因にする |

計画からの補足・決定:

- 供給元が`sample`で別のIDを書いても、Hubは登録時のIDで上書きする。変更検出をIDの有無で行えるようにするため。
- 表示対象期限は、まだ描画期限に含めていない（計画5節「まだ描かない情報の周期更新を有効化しない」）。10-2で`nextChange(snapshot, face.backgroundInterest())`をRuntimeの`nextDisplay_`へ合成する。
- `collect()`の変更ビットは10-2の`WatchChanges`へ渡す想定で、現時点のRuntimeは使っていない。
- 次のスナップショットはHub内の作業領域で組み立て、UIタスクのスタックに置かない。`WatchData`は約256bytes増えた（下のスタック余裕を参照）。

## 2. ホスト検証

`tests/background_tests.cpp`を追加し、`tools/test_runtime.py`へ登録した。全7スイートPASS（[ログ](20260926-10-1-host-2.log)）。
`-1`はGit BashにMSYS2のPATHがなくg++が無言で失敗したもの（[ログ](20260926-10-1-host-1.log)、コード起因ではない）。

| 計画6節の項目 | 検証 |
|---|---|
| 0件・1件・4件・登録超過・同じID | `collectsInRegistrationOrder` |
| 空文字列・UTF-8上限・未終端バッファ・壊れた末尾文字 | `labelsAreCheckedAndOwned`（47bytes保持、3byte文字の途中で切らず45bytes、`snprintf`の途中切断も補正） |
| 未知IDの保持・順序・欠けた供給元の後の順序 | `collectsInRegistrationOrder` |
| 0秒、59秒、59分59秒、1時間、99→100時間、長時間 | `stopwatchFormat`（`INT64_MAX`で`2562047788:00`） |
| 停止・再開・リセット、再開後の端数、ちょうど境界、遅延実行、期限の飽和 | `stopwatchProvider` |
| 壁時計変更 | `homeGetsTheLabelAfterTheScreenCloses`（`TimeService::save`後もラベル・期限は計測開始基準） |
| コピー後の供給元更新でフレームの文字列が変わらない | `labelsAreCheckedAndOwned` |
| 期限だけの変化は内容変更にしない | 同上（`BackgroundUnchanged`） |
| 画面退場・消灯後の継続、ホーム復帰で新しい文字列 | `homeGetsTheLabelAfterTheScreenCloses`。ストップウォッチ画面の25ms更新中は収集しない、消灯中600秒は`invalidate()`しても描画0回、復帰で10分超のラベル、停止後は0件 |
| 表示対象の期限 | `deadlinesFollowTheShownItems`（表示しない3件目の早い期限を含めない） |

## 3. ビルドと実機

| 構成 | ビルド | `verify_build.py` | イメージ | 静的RAM（10-0比） | SHA-256 |
|---|---|---|---:|---:|---|
| m5stopwatch | SUCCESS（[ログ](20260926-10-1-build-m5stopwatch-2.log)） | PASS（[ログ](20260926-10-1-verify-m5stopwatch.log)） | 1,081,408 | 39,452（+840） | `09c5044a6b428c880c21c757c584ceb629bd8c20c997fdcff189a3f03ace53a6` |
| m5stopwatch-measure | SUCCESS（[ログ](20260926-10-1-build-m5stopwatch-measure.log)） | PASS（[ログ](20260926-10-1-verify-m5stopwatch-measure.log)） | 1,085,696 | 56,596（+840） | `1523780b0acd8fb29cd1e06b7e2a91ca55590e4ed6e549b045a225d0e1c10dbe` |
| m5stopwatch-render-check | SUCCESS（[ログ](20260926-10-1-build-m5stopwatch-render-check.log)） | PASS（[ログ](20260926-10-1-verify-m5stopwatch-render-check.log)） | 1,081,280 | 56,276（+840） | `048a5a452be6246cbb79b8e9e62f528e53432fcf3c1ab0fd2912d8e0e8bc1796` |

初回の製品ビルドは`StopwatchBackgroundInfo.h`の`<cstddef>`不足で失敗した（[ログ](20260926-10-1-build-m5stopwatch.log)）。修正後に3構成を再ビルドした。
自作ファイル由来の警告はない。

実機（COM11、`install-host`で`ota_0`のみ更新。[描画検証版](20260926-10-1-install-render.log)、[測定版](20260926-10-1-install-measure.log)、[製品版へ復元](20260926-10-1-restore-product.log)）:

- 描画検証版: `[Verify] checks=369 mismatches=0 result=PASS`（[ログ](20260926-10-1-render-check.log)）。
- 測定版の静止（[ログ](20260926-10-1-measure-idle.log)）: 2本目の60秒窓で`loops=61 layouts=0 paints=0`（10-0の基準と同じ）。
  `stack_free=5700`、内部RAM空き199,579・最大連続147,456。
  スタックは起動と初回描画だけの値で、一覧スクロール等を含む9-5の5,404〜5,564とは条件が違う。操作を含む測定は10-6で行う。

## 4. 全体計画への補足候補（10-6で反映）

- `AppId`の実装名は`LaunchTargetId`（`src/core/AppId.h`）。値を明示し、配列位置をIDにしない。
- Hubは`HostApplication`が所有し、Runtimeが可視の時計のフレームでだけ収集して`WatchData`へ合成する。
- 非表示中の状態変更通知（`invalidate()`の保持）と表示期限（表示対象IDの`nextChangeAt`）は別の経路。
