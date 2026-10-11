# 15-2 検証記録: チュートリアルの画面・記録・ホストの接続

2026-10-10。[作業15の計画](plan.md) 5章の15-2。状態: 完了（描画は15-3）。

## 変更

| 対象 | 内容 |
|---|---|
| `storage/TutorialRecord`（新規） | キー `tutorial`、`[0] format 1`。`load()` が `Missing` のときだけ `due()`。未知の形式・長さ違い・読めない・未接続では出さない。`complete()` で1回書き、失敗なら `due()` のまま |
| `features/tutorial/TutorialScreen`・`TutorialModel`（新規） | 日時（`SettingsEditor`）と案内の2段階。保存できたら・キャンセルしたら案内へ。案内は押せるものがない。`wentHome()` で、案内の段階のときだけ記録を書く。`exit()` は段階を日時へ戻す（`wentHome()` の後に呼ばれる） |
| `Screen::wentHome()` | `ScreenManager::goHome()` が `leaveAll()`（各画面の `exit()`）より前に、表示中の画面へ呼ぶ。`present()`・画面自身の退場では呼ばない |
| `ScreenId::Tutorial`、`ScreenManager` | 所有・`bind()` で時計を、`bindTutorial()` で記録を渡す・`present()` で開ける・`leaveAll()`・モデルの合成 |
| `FrameModel::tutorial` | 表示モデル。描画（Layer・合成）は15-3で足すので、今は何も描かれない |
| `HostApplication` | `TutorialRecord` を所有し `bindRecords()` で読む。`begin()`（Runtimeを始めて、記録がなければチュートリアルを開く）を足し、`main.cpp` はこれを呼ぶ |
| `reservedRecordKey()` | `tutorial` を足した |

## 確認

| 項目 | 結果 | 記録 |
|---|---|---|
| ホストテスト（日本語・英語） | 全スイート成功。`tutorial_tests` を足した（記録、段階、完了と未完了、起動時） | [20261010-15-2-host-2.log](20261010-15-2-host-2.log) |
| テストが働くことの確認 | `goHome()` の `wentHome()` 呼び出しを消すと、`tutorial_tests` の完了の確認（193行）で失敗することを確かめ、元に戻した | ― |
| 製品版・英語版・描画検証版のビルド、`verify_build.py` | 成功 | [20261010-15-2-build-m5stopwatch.log](20261010-15-2-build-m5stopwatch.log)、[20261010-15-2-verify-m5stopwatch.log](20261010-15-2-verify-m5stopwatch.log)、[20261010-15-2-build-m5stopwatch-en.log](20261010-15-2-build-m5stopwatch-en.log)、[20261010-15-2-build-m5stopwatch-render-check.log](20261010-15-2-build-m5stopwatch-render-check.log) |

`tutorial_tests` の内容:

- 記録: 未接続・ない・ある・未知の形式・長さ違い・読めない・書けないときの `due()`。文字盤の保存キーとして拒否されること。
- 段階: 時刻が未設定なら日時から（初期値は `MinYear` の1月1日）、設定済み・時計なしなら案内から。記録がなければ開かない。
  B での増減と押し続け、不正な日付（2月31日）でその場に残ってトーストが出ること、保存で案内へ進んで RTC に書かれること、キャンセルで未設定のまま案内へ進むこと。
  案内でのタップ・A・B・B の押し続けが何もせず、期限も持たないこと。
- 完了: 案内からのスワイプと A+B で記録を1回書き、幕が出ること。日時の段階でのスワイプ・A+B、注意の要求（タイマーの `present()`）、書き込みの失敗では完了しないこと（失敗してもトーストは出ない）。
  打ち切られた後にもう一度開くと日時から始まること。ほかの画面からのホームでは記録に触れないこと。
- 起動時: 記録がなければ `begin()` の後にチュートリアルが開き、最初の `step()` の後も残ること。記録があるとき、記録を渡さないとき（描画検証版）は開かないこと。

## 実機

15-2の製品版は書き込んでいない。チュートリアルはまだ描画されないので、記録のない実機では黒い画面で起動する。
そこで A+B を使うと完了の記録が書かれ、15-4の初回起動の確認ができなくなる。実機は15-1の製品版のままにしてある。
描画は変えていない（`FrameModel` に項目を足しただけで、描画検証版はチュートリアルを開かない）ので、描画検証の実機の実行は15-3で行う。
