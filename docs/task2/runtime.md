# 作業2：Runtime・入力・電源基盤

## 構成

`app_main` のCPU1タスクがAppRuntime、ScreenManager、InputController、PowerManagerを所有する。
M5Unified・I2C・描画は同じタスクだけから呼ぶ。作業1の起動診断後にRuntimeへ移行する。
HALに単調時刻・入力・USB・表示・待機を集約し、PCテストは偽HALで製品の状態管理コードを実行する。

AppRegistryは安定IDと明示的な外部スロット1〜3を持つ。5項目はすべて利用不可で登録し、
この段階では起動やゲスト走査を行わない。画面の契約は入場・退場・意味イベント・表示モデル・
更新期限・動作中状態。ホームをScreenManagerが先に処理し、一時的な選択とドラッグを破棄する。

InputControllerは短押しをリリースで確定する。同時押し群の短押し抑止と連続600ms計時を分離した。
片方を離すと計時をリセットし、ホーム発火後は両方離れるまで再発火しない。
タッチは単一接触。表示短辺/50（468pxなら9px）を超える移動でドラッグに切り替え、タップを抑止する。
ホームと復帰に使ったタッチは解放まで消費する。

PowerManagerは表示状態とUSB状態を別々に持つ。30秒無操作でsleepし、復帰時は輝度90に戻す。
ボタン・接触保持・リリースで期限を更新する。USB接続や描画更新だけでは期限を延長しない。
VBUSはStopWatchのPM1（0x6e、0x24/0x25）を100kHzで1秒ごとに読み、失敗は不明とする。
給電判定は4,000mV超。USBデータ接続はUSB Serial/JTAGのSOF受信判定で、ポートが開いているかとは異なる。
このAPIはtickごとの接続監視を追加する。電力への影響は作業7以降で測定する。

期限は64bitマイクロ秒。入力は10ms、USBは1秒、消灯は最終操作から30秒。
画面の絶対更新期限も待機に含め、消灯中は除外する。診断画面は時間による更新期限を持たない。
作業3のアニメーションはこの契約へ16ms刻みの期限を接続する。
実際に入力・USBを処理した時点で次の周期を現在時刻から設定し、過去の周期を再実行しない。
待機処理では未処理の期限を変更せず、期限超過時も最低1tickブロックして次のループで処理する。
FreeRTOSの待機はtick境界基準なので、マイクロ秒の絶対期限より少し早く復帰する場合もある。
通常版には意図的な負荷やアイドルフックを含めない。

## PCでの検証

```powershell
python tools/test_runtime.py
python -m unittest discover -s tests -v
pio run -e m5stopwatch
python tools/verify_build.py
pio run -e m5stopwatch-diagnostics
python tools/verify_build.py --environment m5stopwatch-diagnostics
```

PCテストにはC++17対応のg++が必要（WindowsではMSYS2 UCRT64）。一時フォルダーへテストをビルドする。
診断版も同じ書き込み保護・4MiB検査・依存版・SDK既定値を継承する。
通常版と診断版のビルドは順番に実行する。

## 実機手順

書き込みは[作業1のinstall-host手順](../task1/product-build.md)に従う。
通常版は `.pio/build/m5stopwatch/firmware.bin`、過負荷診断版は
`.pio/build/m5stopwatch-diagnostics/firmware.bin` を渡す。どちらもアプリ単体だけを更新する。

1. 通常版で起動ログの `layout=valid host=valid`、CPU240MHz、CPU1、表示サイズを確認する。
   起動後は `Launcher home` と `TASK 2 DIAGNOSTICS` が表示される。
2. ホームでA/Bの短押しまたはタップを行い、`Input check`へ移る。
   Aで `Test event` / `Back` を選択し、Bまたはタップで実行する。
   タップは画面全体で現在選択中の操作を実行する診断用操作。ドラッグはイベント表示だけを変更する。
3. A+Bを600ms保持してホームへ戻る。保持中にHome countが繰り返し増えないこと、
   片方解除・再保持で600msを数え直すこと、短い同時押しの解除で画面が動かないことを確認する。
4. ドラッグ中にA+Bでホームへ戻り、残りのタッチ移動・解放で確認画面が開かないことを確認する。
5. 両画面で無操作30秒後の消灯、保持中の非消灯を確認する。
   タッチ復帰で直前画面を維持し、指を離してから次の操作が有効になることを確認する。
   A/Bは復帰後のリリースで通常動作し、A+Bは復帰後600msでホームへ戻ることを確認する。
6. PC接続、給電専用USB、電池駆動でVBUS/USBの区別を確認する。
   USB接続中も消灯すること、消灯中のUSB抜き差しで点灯しないことを確認する。
   復帰時に最新のUSB状態が表示されることを確認する。
7. 過負荷診断版をinstall-hostで導入する。`[RuntimeDiag] enabled` が出ることを確認し、
   60秒以上操作を続ける。5秒ごとの `idle_cpu1` が常に正、WDTログ・再起動・操作不能がないことを確認する。
   この版は毎周期40msのbusy処理を注入するため、通常版の応答速度評価には使用しない。
8. 通常版へ戻し、起動時にRuntimeDiagログが出ないことを確認する。

診断画面は毎回黒で全面消去して描く。差分描画・FramePlan・ピクセル比較機能は作業3で導入する。
今回の画面の欠け・安全領域・残像は上記操作中に実機で確認し、ピクセル比較に合格した扱いにはしない。
実機結果は[検証記録](task2-validation.md)へ記入する。
