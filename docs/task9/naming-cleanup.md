# 9-4レビュー後: 名前と配置の整理

2026-09-24。9-4の責務分割を維持し、製品全体・画面・起動対象を名前でも区別する。
操作仕様、描画方式、所有権、保存形式、起動対象のID値は変更しない。

## 対応表

| 変更前 | 変更後 | 意味 |
|---|---|---|
| `src/app/` | `src/host/` | wararyoLauncher全体の構成・実行・画面管理 |
| `Application` | `HostApplication` | サービスと画面管理・実行部の所有と接続 |
| `AppRuntime` | `HostRuntime` | 全体の入力・電源・描画期限の実行 |
| `AppRenderer` | `HostRenderer` | 機能レイヤーの描画構成 |
| `AppShutdown` | `HostShutdown` | 外部起動確定時の全体終了処理 |
| `AppScreen` | `Screen` | 個別画面の共通契約 |
| `AppId` | `LaunchTargetId` | 内蔵機能・外部スロットの安定した起動先ID |
| `AppEntry` / `AppRegistry` | `LaunchEntry` / `LaunchRegistry` | 起動先の登録情報 |
| `appEntry()` | `launchEntry()` | 行IDに対応する起動先の検索 |
| `services/LauncherData.*` | `features/home/HomeDataSource.*` | 時計・電池情報をホームへ供給するアダプター |

`AppList*` はユーザーに見えるアプリ一覧、`ExternalAppScreen` は外部アプリの詳細画面を表すため維持する。
`features/launcher/LauncherController` はホームと一覧の遷移を担当し、製品全体を動かす `HostRuntime` と区別する。
`Screen` と `LaunchRegistry` はホストが機能へ提供する契約であり、共通UIはこれらに依存しない。

CMakeのソース一覧、ホストテスト、診断、資産生成ツールの参照、および現行計画を更新した。
過去の検証記録・生ログに含まれる旧名は、その時点の証跡として維持する。

## 検証

- ホストテスト: 全6スイート成功。`20260924-naming-host.log`。
- 製品・測定・描画検証構成のビルドと `verify_build.py`: 全3構成成功。
  - ビルド: `20260924-naming-build-retry.log`、製品再試行は `20260924-naming-build-product-j2.log`。
  - 生成物検証: `20260924-naming-verify-product.log`、`20260924-naming-verify-measure.log`、`20260924-naming-verify-render.log`。
- 初回はPlatformIOのホーム配下キャッシュへの書き込み権限で停止（`20260924-naming-build.log`）。権限を得て再実行した。
  続く製品ビルドはESP-IDFの `usb/enum.c.o` で詳細診断なしの `Error 1` となったが、ソース変更なし・並列数2で再試行して成功した。原因は未確定。
- 元のソースとの機械的照合: ソース・テスト・ツール123ファイルで予定した改名・パス更新以外の差分なし。
- `git diff --check`: 成功。共通UIから `host/`・`features/` へのinclude、およびソース・テスト・ツールの旧名・旧パス参照がないことを確認。
- 実機への書き込み・操作測定は実施しない。9-5の描画・性能確認は引き続き未実施。
