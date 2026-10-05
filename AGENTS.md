# エージェント向け作業方針

## 全体の構造

[docs/architecture.md](docs/architecture.md) に、現行の全体構造がまとまっている。ディレクトリの役割、所有権と寿命、実行ループ、画面の契約、
差分描画（`FramePlan`）、時刻・設定、省電力と期限管理、変更後の確認方法を扱う。
コードを変更する前に関係する章を読む。設計を変えたら、architecture.md の該当箇所を同じ変更の中で更新する。

- 要件と合意事項の経緯は [docs/plan.md](docs/plan.md)、実機測定の結果は [docs/measurements.md](docs/measurements.md) にある。
- `docs/task*/` は過去の作業の計画と検証記録である。当時の証跡なので、現在の構成に合わせて書き換えない。
- `measurement/` は測定用の試作で、製品版（ルートの `src/`）とは別の構成である。
- 実機への書き込みは MultiFirm の `install-host` で行う（通常の `upload` は拒否される）。機器を書き換える操作なので、実行前にユーザーへ確認する。

## 新しいWatchFaceの作成を頼まれたら

契約の詳細は architecture.md の5章と [WatchFace.h](src/features/home/WatchFace.h) のコメントにある。ここでは依頼を受けてから完了までの進め方を示す。

### 1. 見た目と振る舞いを確かめる

画像・スケッチ・参考にする文字盤があれば受け取る。足りない点は、既存の文字盤に倣った案を添えて確認する。

- ID（英小文字。選択の保存値になるので、後から変えない）と、設定に出す表示名
- 表示する内容: 時・分・秒、日付、電池残量、バックグラウンド情報（ストップウォッチなど）の数
- 背景が黒か、全面を塗るか。アプリ一覧が重なるときの背景色（`listBackground()`）と、一覧の進捗に応じた動き
- 操作: 既存の文字盤は長押しで秒表示を切り替える。タップでアプリ一覧を開く領域を持つか（Digital の APPS）
- 再起動後も保持する状態（秒表示の有無など）があるか
- 新しいフォントや文字が要るか

### 2. 近い既存の文字盤を土台にする

| 文字盤 | 構成 | 土台にする場面 |
|---|---|---|
| Digital | `DigitalWatchFace` + `DigitalLayout.h` | 黒背景のデジタル表示 |
| Forest | `ForestWatchFace` + `ForestLayout.h` | 全面を塗る背景の上のデジタル表示 |
| Analog | `AnalogWatchFace`（`HandsFace` + `AnalogLayout.h`、`SolidBackdrop`） | 針の文字盤 |
| Noonish | `HandsFace` を継承し、背景（`Backdrop`）を差し替え | 針は同じで、背景だけが違う文字盤 |

配置・期限・長押しなど描画に依存しない部分は `*Layout.h` に置き、PC上のテスト（`tests/watchface_tests.cpp`）から検証できるようにする。
時刻の数字（`TimeDigits`）、電池とバックグラウンド情報の行（`InfoRow.h`、`InfoRowView`）は、新しく作る前に再利用できるか確かめる。

### 3. 実装する

`src/features/home/faces/` に置き、`WatchFace` の契約を守る。

- 描画資源（キャッシュ・フォント）は `begin()` / `end()` で確保・解放する。文字盤の状態は `end()` の後も保持する。
- `plan()` で要素の矩形と fingerprint を `FramePlan` に登録し、背景の変化は `FramePlan::damage()` で宣言する。
  `paint()` は `PaintContext` の範囲だけを描く。全面を不透明に塗るなら `opaqueArea()` を返す。
- `FramePlan::add()` が `-1` を返した要素も描けるようにする（容量超過時は全面再描画に切り替わる）。
- `nextUpdate()` は表示が変わる時刻だけを返す（分表示なら分の境界）。毎フレーム描画しない。
- RTC・NVS・画面管理を直接触らない。状態の保存は `storageKey()` と `FacePreferences` を使い、`handle()` の中でだけ行う。
- 表示するバックグラウンド情報は `backgroundInterest()` で宣言する。

### 4. 登録する

- [HomeLayer.h](src/features/home/HomeLayer.h) にメンバーを追加し、[HomeLayer.cpp](src/features/home/HomeLayer.cpp) の `begin()` で `registerFace()` する。登録順が設定での並び順になる。
- 登録数の上限を上げる。現在は上限4つに対して4つが登録済みである。
  [FaceSelection.h](src/features/home/FaceSelection.h) の `Capacity`（描画検証版の値も）と、[SettingsModel.h](src/features/settings/SettingsModel.h) の `SettingsFaceCapacity` を変える。
- [src/CMakeLists.txt](src/CMakeLists.txt) のソース一覧に `.cpp` を追加する。
- フォントを追加する場合は、`tools/fonts/` の文字リストを使い `tools/build_font.py` で作る。
  `platformio.ini` の `board_build.embed_files` と `src/CMakeLists.txt` の `EMBED_FILES` の両方に加える。
  TrueType フォントはリポジトリに含まれないので、ユーザーに入手元を確認する。

### 5. 確かめる

- `tests/watchface_tests.cpp` に配置・期限・長押し・保存のテストを足し、`python tools/test_runtime.py` を通す。
- [RenderDiagnostics.cpp](src/host/RenderDiagnostics.cpp) に、既存の文字盤の節に倣って、全面描画とのピクセル比較・文字盤の切り替え・キャッシュ無効時のチェックを足す。
- `pio run -e m5stopwatch` と `pio run -e m5stopwatch-render-check` を通し、`python tools/verify_build.py --environment <環境名>` で検証する。
- 見た目は、`LAUNCHER_RENDER_SHOTS` を付けた描画検証版の画面を `tools/render_shots.py` で PNG にして確認できる（手順はスクリプト冒頭）。

### 6. ドキュメントを更新する

- architecture.md の5章（選べる文字盤など）を実装に合わせる。
- README.md の文字盤の画像と「特徴」の文字盤の数を更新する。画像は `docs/Images/WatchFace/` に置く。

## 新しいアプリの作成を頼まれたら

### 1. 内蔵アプリか外部アプリかを決める

このランチャーの「アプリ」には二種類ある（architecture.md 3章の用語）。どちらで作るかで作業する場所が変わるので、最初にユーザーと決める。

| | 内蔵アプリ（Feature） | 外部アプリ（ゲスト） |
|---|---|---|
| 実体 | このリポジトリの `src/features/<name>/` | 別リポジトリの別ファームウェア。`ota_1`〜`ota_3` に入れる |
| 向くもの | 小さく軽い機能、時計に情報を出す機能、すぐ開きたい機能 | Wi-Fi・BLE・音声などを使う機能、既存のファームウェア、ランチャーの省電力方針（無線なし・期限駆動の描画）に合わないもの |
| 起動 | 一覧から即座に画面が切り替わる | 再起動して切り替わる。戻るときもランチャーが起動し直す |
| 制約 | ホストの容量4MiB、UIタスクのスタック8KiB、描画は M5GFX のみ | 1スロット 2,031,616 bytes まで |

外部アプリなら、このリポジトリのコードは変更しない。ゲストのリポジトリで標準のビルドを行い、MultiFirm のゲストライブラリ（A+B の長押しでランチャーへ戻る）を組み込み、
`multifirm.ps1 install --slot <1-3>` で書き込む（README の「導入方法」5）。外部スロットの数を増やすのはパーティション配置と MultiFirm の契約にかかわるので、ユーザーに相談する。

### 2. 内蔵アプリの仕様を確かめる

- 画面の構成と遷移。A/B の意味（既定は A＝次へ、B＝決定。画面ごとに決めてよく、ストップウォッチは左右の操作に割り当てている）。
  A+B の長押しでホームへ戻ることと、短押しをリリースで確定することは全画面で共通なので変えない。
- 画面を閉じても続く状態（計測・タイマーなど）があるか。あれば画面から分けてサービスに置く。
- 文字盤に出す情報（バックグラウンド情報）があるか。
- 保存する設定があるか。
- アイコン（44×44 のグレースケールマスク、原本は `icons/icons.ai`）と一覧の色。アイコンはデザインが要るので、ユーザーから受け取るか、仮のものを置くことを伝える。

### 3. ストップウォッチを手本に実装する

最も揃った例はストップウォッチである。画面（`StopwatchScreen`）、表示モデル・配置・描画（`StopwatchModel` / `StopwatchLayout` / `StopwatchLayer`）、
画面外でも続く状態（`services/StopwatchService`）、文字盤への情報供給（`StopwatchBackgroundInfo`）が揃っている。
手順は architecture.md 3章「内蔵機能を追加する手順」に従う。要点は次のとおり。

- [core/AppId.h](src/core/AppId.h) の `LaunchTargetId` に新しい明示値を割り当てる。既存の値は変えず、再利用もしない。
  [LaunchRegistry](src/host/LaunchRegistry.h) に項目を足す（並び順が一覧の順になる）。
- `ScreenId`、`ScreenManager`（所有・初期化・`launch()`・ホームでの `exit()`・モデル合成）、`FrameModel`・`FrameComposer`・`HostRenderer` を接続する。Registry への追加だけでは動かない。
- アイコンは `IconId` と `tools/build_icons.py` の `ICONS` を対応させ、`python tools/build_icons.py` で再生成する。`IconId` の既存の順序は変えない。
- 画面に出す固定の文字列は [i18n/Strings.h](src/i18n/Strings.h) に日本語と英語の両方を足し、`text::` で参照する（英語版は `m5stopwatch-en`）。
- 画面に新しい日本語の文字を出すときは、埋め込みフォントの部分集合を作り直す（`tools/build_font.py`。`src/` 内の非ASCII文字を自動で集める）。
  TrueType フォントはリポジトリに含まれないので、ユーザーに用意してもらう。作り直すまで、収録外の文字は `?` で表示される。
- 文字盤に情報を出す場合は `BackgroundInfoProvider` を実装し、`HostApplication` で `BackgroundInfoHub` に登録する（上限は `BackgroundCapacity` の4つ）。
- 画面を閉じていても装着者に知らせる場合（満了の通知など）は `AttentionSource` を実装し、`HostApplication` で `HostRuntime::bindAttention()` に登録する（上限4つ、手本は `TimerAttention`）。
  点灯・画面の切り替え・押下中の操作の破棄・振動はホストが行い、要求元は「今の要求」を返すだけにする（architecture.md 2.1）。
- 表示の更新は `nextUpdate()` の期限で要求する。毎ループの描画、busy wait、定期タイマー、毎フレームの I2C 読み出しを足さない。
- `src/CMakeLists.txt` と `tools/test_runtime.py` のソース一覧に `.cpp` を足す。テストスイートを足したら `test_runtime.py` のスイート一覧にも加える。

### 4. 確かめる

- `tests/` に入退場・ホームへの復帰・更新期限・サービスの寿命のテストを足し、`python tools/test_runtime.py` を通す。
- 描画を足したら、`RenderDiagnostics.cpp` に全面描画とのピクセル比較を足し、`m5stopwatch-render-check` でも確かめる。
- `pio run -e m5stopwatch` と `python tools/verify_build.py` を通す。

### 5. ドキュメントを更新する

- architecture.md の変えた箇所（`LaunchRegistry` の項目、所有権、期限の表など）。
- README.md の「特徴」の内蔵アプリ。

## ログの保存と取得

コンテキストの消費を抑えるため、ビルド・テスト・実機シリアル等の大量のログは、まずファイルに保存し、必要な行だけを取得する。

- 標準出力・標準エラーはログファイルへ直接リダイレクトする。`Tee-Object` 等で全ログをツール出力にも流さない。
- 実機ログ取得ツールがファイル保存と標準出力の両方を行う場合は、標準出力を抑制するか別ファイルへ保存する。同じファイルへの二重書き込みは避ける。
- 完了確認では終了コードと成功・失敗の要約だけを取得する。リダイレクト後も終了コードを保持する。
- 内容の確認には `rg` を使い、エラー・警告・検証結果・性能集計など、目的に必要な行だけを抽出する。
- 原因調査が必要な場合だけ、該当箇所の前後を範囲を限定して読む。ログ全体の表示や、同じログの繰り返し取得は避ける。
- 生ログは検証証跡として保持する。既存ログを不用意に上書きせず、作業・構成・測定区間を識別できる名前で保存する。
- 実行中の進捗確認でも全ログを取り込まず、プロセスの状態や必要な末尾数行を確認する。

PowerShellでの例（ログの保存先ディレクトリは事前に作成する）:

```powershell
pio run -e m5stopwatch *> docs/task9/build.log
$buildExitCode = $LASTEXITCODE
Write-Output "Build exit code: $buildExitCode"
rg -n -m 20 'error:|fatal error:|FAILED|SUCCESS' docs/task9/build.log
exit $buildExitCode
```
