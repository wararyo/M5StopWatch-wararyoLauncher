# 作業13-2 検証記録: PedometerService・保存・ホストの接続

2026-10-07〜08。計画は[plan-13-2.md](plan-13-2.md)。状態: 完了。4時の区切りと外部起動確定での保存は、ホストテストでのみ確認した（実機は13-5）。

## 1. 変更

| 場所 | 内容 |
|---|---|
| `src/services/PedometerService.*`（新規） | 日の番号（`pedometerDay()`・`pedometerDayStart()`）、合わせ直しの予定（`pedometerAlignmentLeft()`）、今日の歩数、区切りと読み直し、`ClockFollower` |
| `src/storage/PedometerRecord.*`（新規） | キー`pedometer`、9バイトの版付き記録。最後に保存できた値と違うときだけ書く |
| `src/host/HostRoutine.h`（新規） | 画面の外の処理の口。`begin()`・`service()`・`panelOff()`・`beforeClock()` |
| `src/features/pedometer/PedometerRoutine.*`（新規） | 起動時の引き継ぎ、消灯時と外部起動確定時の保存、時計の描画に相乗りした30秒ごとの読み取り。ログ`[Pedometer]` |
| `src/host/HostRuntime.*` | `bindRoutine()`。起動時・毎step（合わせ直しの後）・消灯時・時計の情報を集める前に呼び、期限を消灯中も待つ |
| `src/host/HostApplication.h` | 所有と接続。`bindSettings()`で時刻、`bindRecords()`で記録を渡す。`HostShutdown`が今日の歩数を保存する |
| `src/storage/WatchPreferences.cpp` | `pedometer`を予約キーにした |
| `src/CMakeLists.txt`、`tools/test_runtime.py` | ソースと`pedometer_tests`を追加 |
| `tests/timer_tests.cpp` | `HostShutdown`の引数の追加に合わせた |
| `docs/architecture.md` | 所有権、2.3節（歩数計と画面の外の処理）、6章の記録、7章の期限の表と`wait()` |

## 2. ホストテスト

`python tools/test_runtime.py`: 新しい`pedometer_tests`を含む全10スイートが日本語・英語でPASS（[ログ](20261007-13-2-host-1.log)）。

`pedometer_tests`の内容:

- 日の番号: JSTの03:59:59と04:00:00、23:59と0:00（同じ日）、月末・年末・閏日、日の始まりの逆算。
- 合わせ直しの予定: 24時間→10分、10分→1分、1分→なし。
- 記録: 符号化・復号、長さ・版の不一致、同じ値で書かないこと、失敗後の比較の相手、予約キー。
- 計数: 読み取り、カウンタが減ったとき（IMUのリセット）の引き継ぎ、読み取りの失敗、IMUがない場合。
- 再起動: 今日の記録の引き継ぎ、前日の記録は引き継がず次の保存で上書き、時刻が未設定なら引き継がず保存もしない。
- 消灯時の保存: 変わったときだけ書く、書き込み失敗の後の次の消灯で書き直す。
- 消灯中の区切り: 03:00から7,500ppmで進む時計で消灯したまま待ち、04:00（RTC基準で±1秒以内）に区切ること。
  途中の合わせ直しがちょうど2回（10分前と1分前）であること。点灯も描画もしないこと。区切る前の歩数が前日の分になること。
  合わせ直しを止める改変を一時的に入れると、このテストが失敗する（早く区切る）ことも確かめた（改変は戻した）。
- 読み直し: 区切りの時刻に読めないと1秒ごとに読み直し、3回目で最後に読んだ値で区切ること。2回目で読めた場合は、その値までを前日の分にすること。
- 時刻の変更: 手動設定で次の日・前の日へ変えると、その時点から新しい日。同じ日の中では区切らない。未設定から設定すると、それまでの歩数を保つ。
- 時計の読み取り: 1秒ごとに描く時計で25秒の間は読まないこと、30秒経ったら読むこと、消灯中の3時間は読まないこと。
- 外部起動: 一覧から外部アプリを起動確定すると、今日の歩数が保存されること。

## 3. ビルド

| 項目 | 結果 |
|---|---|
| 通常版 `m5stopwatch` | SUCCESS（[ログ](20261007-13-2-build-m5stopwatch.log)） |
| 描画検証版 `m5stopwatch-render-check` | SUCCESS（[ログ](20261007-13-2-build-render-check.log)） |
| `verify_build.py --environment m5stopwatch` | OK。ホスト1,294,832／4,194,304 bytes（[ログ](20261007-13-2-verify-m5stopwatch.log)） |

## 4. 実機

通常版を導入した（[導入](20261007-13-2-install-product.log)）。日時はJST 2026-10-07 23:49ごろ（日の番号20733）。

| 記録 | 結果 |
|---|---|
| [1回目の起動](20261007-13-2-boot-1.log) | `begin imu=ok dated=1 day=20733 steps=0 record=no`。消灯で`saved day=20733 steps=0`（記録がなかったため書いた） |
| [リセットなしの記録](20261007-13-2-walk.log) | 75秒間ログなし。消灯したままで点灯も手首のウェイクも起きなかったため、保存の機会がなかった（想定どおり） |
| [2回目の起動](20261007-13-2-boot-2.log) | `begin ... record=yes day=20733 steps=0`。足踏みの後の消灯で`saved steps=44`、ボタンで点灯して再び消灯して`saved steps=72` |
| [3回目の起動](20261007-13-2-boot-3.log) | `begin ... steps=72 record=yes day=20733 steps=72`。再起動の後も72から続いた。変化のない消灯では書かなかった（`saved`が出ない） |

## 5. 13-3以降へ

- 4時の区切り（設定で03:50にして消灯のまま待つ）と、外部アプリの起動確定での保存は、13-5で実機で確かめる。
- 画面での1秒ごとの読み取りは13-3で、文字盤の情報は13-4で足す。
