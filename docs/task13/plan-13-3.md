# 作業13-3: 歩数計画面と一覧

作成日: 2026-10-08。上位計画は[作業13の計画](plan.md)の1.3節・1.5節・2.6節。

- 一覧: `LaunchTargetId::Pedometer=6`をタイマーの次に置く。`IconId::Pedometer`を末尾に足し、`ic_pedometer.png`から資産を作り直す。色は`PedometerColors{0x2d87,AppIconWhite}`。
- 文言: 歩数計／Pedometer、今日の歩数／Steps today、歩／steps。`OK`は両言語とも英字。
- 画面: `PedometerScreen`はサービスを借り、開いたときと表示中は1秒ごとに読む。OK・A・Bで一覧へ戻る。IMUが読めなければ`--`。
- 描画: `PedometerLayer`は、アイコン・題・歩数（単位と組）・OKの4要素。寸法は`Steps.svg`の中心からの相対で拡大縮小する。OKの当たり判定はタイマーのSETと同じく下へ同じ高さだけ広げる。
- ホスト: `ScreenId`・`FrameLayer`・`FrameModel`・`FrameComposer`・`HostRenderer`・`ScreenManager`・`HostApplication`を接続する。
- フォント: 64pxの数字は`tools/fonts/numbers.txt`（`0-9 : - ,`）で作り直す（`time.txt`は時計の100px・120pxと共用のため触らない）。日本語の部分集合も作り直す。既存のグリフが変わらないことを確かめる。
- 確認: ホストテスト（書式、一覧の位置、画面の入退場・1秒の読み取り・OK・A・B・A+B、IMUなし）、3環境のビルド、画面写真での見た目、描画検証版の全件PASS。全面描画との画素比較の追加は13-4。
