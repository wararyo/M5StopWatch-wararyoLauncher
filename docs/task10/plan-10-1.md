# 10-1: バックグラウンド情報の契約と供給

作成日: 2026-09-25。状態: 実装計画。実装・検証は未着手。
上位仕様は[作業10](plan.md)。本書の型名・容量は実装案であり、ユーザー合意済みの表示仕様とは区別する。

## 1. 到達点と依存

アプリが作った識別情報・文字列を、画面の寿命から独立してホームへ供給できるようにする。
ストップウォッチはRunningのみ、1時間未満は`mm:ss`、以降は`HH:mm`。Paused / Resetは供給しない。
この段階では表示部品を追加せず、10-2でイベント・期限、10-3でDigitalの情報欄を接続する。
作業9後の構成を使い、StopwatchServiceの計測方式は変更しない。

## 2. 型と配置

| ファイル（新規は案） | 変更 |
|---|---|
| `src/core/AppId.h`（新規） | `LaunchTargetId`をこの依存の軽いヘッダーへ移す。名称は既存を維持 |
| `src/host/LaunchRegistry.h` | 上記型をincludeし、表示名・アイコン・起動先だけを保持 |
| `src/features/background/BackgroundInfo.h`（新規） | 情報・供給元の契約。M5GFX、HAL、画面に依存しない |
| `src/features/background/BackgroundInfoHub.*`（新規） | 固定容量の供給元登録、収集、比較、表示対象の期限集計 |
| `src/features/stopwatch/StopwatchBackgroundInfo.*`（新規） | const StopwatchService参照から情報生成 |
| `src/features/home/HomeModel.h` | `WatchData`に所有する情報スナップショットを追加 |
| `src/host/HostApplication.h` | サービス→供給元→集約→Runtimeの順で所有・参照注入 |
| `src/host/HostRuntime.*` | 描画用データへ情報を合成する接続点を用意 |

全体計画の`AppId`は概念名で、現在の実装名は`LaunchTargetId`である。二重のID体系や変換表は作らない。
値を明示し、登録配列の位置をIDにしない。将来のタイマー追加までTimerの本番登録は作らない。

契約案:

```cpp
struct BackgroundInfo {
    LaunchTargetId appId;
    char label[48];                 // UTF-8、終端を含む上限
    TimeUs nextChangeAt;            // INT64_MAXなら期限なし
};
struct BackgroundSnapshot {
    std::array<BackgroundInfo, 4> items;
    uint8_t count;
};
class BackgroundInfoProvider {
public:
    virtual ~BackgroundInfoProvider() = default;
    virtual LaunchTargetId id() const = 0;
    virtual bool sample(TimeUs now, BackgroundInfo& out) const = 0;
};
```

供給元4件、1件48bytesの文字列を初期容量とする。供給元登録は起動時のみ、参照先はHubより長寿命。
同じIDの二重登録と容量超過を拒否して診断する。情報なしは`false`、空ラベルも情報なしとして扱う。
Hubは終端・長さを検証し、過長文字列はUTF-8文字途中で切らない。未収録グリフの代替と画面幅への省略は描画側の責務。
固定バッファをコピーして同一フレーム中は不変とし、供給元の一時ポインターを残さない。
表示は最初の2件だが、契約を2件専用にはしない。フィルター後も供給元の登録順を維持する。

## 3. ストップウォッチの整形・期限

`elapsed(now)`の非負値から、秒未満・分未満を切り捨てて整形する。
1時間以上のHHは最低2桁とし、100時間では`100:00`とする。2桁へ丸めたり内部計測を飽和させたりしない。
TimeUsで表現できる時間を48bytes内に収める。表示幅の都合は文字盤側の省略で処理する。

更新周期を`q`（1秒または1分）、経過時間を`e`とすると、次回期限は`now + (q - e % q)`。
加算のオーバーフローはINT64_MAXへ飽和させる。ちょうど境界では次の境界を返し、期限超過時に過去フレームを再生しない。
1時間到達時に書式と周期を同時に切り替える。壁時計の時分秒は使わない。

## 4. Runtimeとの接続

現在はHomeDataSourceがmain側で構築され、StopwatchServiceはHostApplication内にある。
HomeDataSourceへサービス参照を逆向きに渡さず、Runtimeが`data.sample(now)`の結果へHubの情報を合成する方式とする。
HubはHostApplicationが所有しRuntimeへ注入する。未接続のテスト用Runtimeでは空スナップショットにする。

- 可視ホームを描く直前と、表示再開時に収集する。ストップウォッチ画面の25ms更新から独立させる。
- 現行の開始・停止・再開は画面操作によるdirtyとホーム復帰で最新値を取得できる。
  将来、画面外から状態変更する供給元には`invalidateBackground()`をUIタスク上で呼ぶ契約を用意する。
  消灯中はdirtyだけ保持し、ラベル生成のために周期起床しない。
- 「何の情報があるか」は収集時に把握し、文字盤が選んだ表示対象IDの期限だけを10-2で描画期限に含める。
  新しい情報の追加は状態変更通知で拾う。現在表示中の情報の期限だけを待って新規情報を見落とさない。
- 比較対象はID・ラベル・件数・順序。期限が先へ進んだだけでは内容変更イベントを発行しない。
- 新しい常駐タスク・毎秒の全供給元ポーリングは作らない。将来の通知スレッドはUIタスクへの通知経路を通す。

## 5. 実装順序

1. ID型を移し、既存ビルド・テストが通る状態を保つ。
2. 固定容量の型、Hub、偽供給元を追加する。
3. StopwatchBackgroundInfoを追加し、書式と期限を単体検証する。
4. HostApplicationの所有順とRuntimeへの注入、WatchDataへのコピーを接続する。
5. 10-2へ渡す表示対象期限APIと状態変更フックを用意する。まだ描かない情報の周期更新を有効化しない。

## 6. 検証と完了条件

`tests/background_tests.cpp`を追加し、`tools/test_runtime.py`のスイート・ソース一覧と`src/CMakeLists.txt`を更新する。

- 0件・1件・4件・登録超過・同じID・空文字列・UTF-8上限・未知IDの保持・順序。
- 0秒、59秒、59分59秒、1時間、99→100時間、長時間、停止・再開・リセット、壁時計変更。
- 再開後の端数を含む期限、ちょうど境界、遅延実行、期限の飽和。
- Hubのコピー後に供給元を更新してもフレームの文字列が変化しないこと。
- アプリ画面退場・消灯後もサービスは継続し、ホーム復帰時に新しい文字列を得ること。

ホスト検証と製品ビルドを通し、10-2がHALやアプリ画面に依存せず使えることを完了条件とする。
ログは`docs/task10/10-1-<構成>-<日時>.log`へ保存し、結果を`10-1-validation.md`へ記録する。
全体計画への補足候補: AppIdの実装名、Hubの合成場所、非表示中の状態変更通知と表示期限の区別。
