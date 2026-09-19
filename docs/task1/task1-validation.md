# 作業1 検証記録

実施日: 2026-09-20。製品構成の実装・PC検証とユーザーによる実機確認を終え、**作業1は完了**。
実機操作はユーザーが実施。[操作ログ](task1-validation.log)で更新と対象外領域の保持を確認し、
起動表示・起動ログ・5分以上の待機は下表のユーザー確認結果に基づく。

## ビルド条件と成果物

| 項目 | 確認値 |
|---|---|
| 環境 | Windows / PowerShell、PlatformIO Core 6.2.0 |
| PlatformIO環境 | ルートの `m5stopwatch` |
| プラットフォーム | espressif32 6.12.0 |
| IDF | framework-espidf 3.50500.0 / ESP-IDF 5.5.0 |
| ライブラリ | M5Unified 0.2.16、M5GFX 0.2.28（ビルド依存グラフで確認） |
| MultiFirm | f5a8fd88f50dfb50c6e6e1e82bccc491a936bd47 |
| ファームウェア | wararyoLauncher / 0.1.0-dev |
| バイナリ | `.pio/build/m5stopwatch/firmware.bin`、474,592 bytes |
| 上限 | 4,194,304 bytes、残り3,719,712 bytes |
| イメージヘッダ | ESP32-S3 / DIO / 80MHz / 16MB、付加SHA-256有効 |
| リンク時RAM使用量 | 18,476 bytes（実行時のヒープ使用量ではない） |
| 生成設定 | CPU 240MHz、CPU1、tick 1kHz、スタック8192 bytes、USB Serial/JTAG |
| メモリ設定 | Flash 16MiB、Octal PSRAM 80MHz、命令16KiB／データ64KiB／ライン64B |

検証対象バイナリのSHA-256:

```text
b1ab250734799bff01b9c8626b4a467cef08442aee4937a9a643bf772f0848cd
```

ビルド日時を含むため、再ビルドのSHA-256は変わり得る。導入するファイルは毎回検証する。
PlatformIOによるイメージ生成には同梱esptool 4.9.0を使用。実機更新用のMultiFirmツールは
別途esptool 4.12.0を要求する。実機操作ログでもPython 3.11.7 / esptool 4.12.0を確認した。

## PC上の結果

| 検証 | 結果 |
|---|---|
| 初回ビルド | ルートにSDK生成設定・成果物がない状態から成功。measurementの成果物は流用していない |
| 再ビルド | 成功。`firmware.bin` の更新時刻が変わらない状態でも `multifirm_check_size` が実行された |
| 単体テスト | `python -m unittest discover -s tests -v`：6件成功 |
| 容量境界 | 4MiB−1 byte・4MiBは許可、4MiB＋1 byteは拒否。欠損ファイルも失敗として扱う |
| 保護の順序 | 禁止ターゲットではビルドフック登録前に例外。容量検査には生成後フックとAlwaysBuildの両経路を登録 |
| 標準書き込みの拒否 | 実際の `pio run` でupload / uploadfs / uploadfsota / eraseの各指定を拒否 |
| 複数ターゲット | 実際の `pio run -t clean -t upload -t erase` で書き込みを拒否 |
| 通常ターゲット | build成功。clean動作確認。monitorは保護関数の許可を単体テストで確認し、実機モニターは未実行 |
| SDK設定 | `tools/verify_build.py` の全期待値と一致。PM・tickless idle・rollback・Secure Boot・Flash Encryptionは無効 |
| ホスト定義 | `MULTIFIRM_HOST=1` のコンパイル検査、ホスト実装のコンパイルとAPI呼び出しのリンクに成功 |
| レイアウト | 固定MultiFirmの正規CSVと製品CSVをIDFツールでバイナリ化し一致。生成済み `partitions.bin` とも完全一致 |
| イメージ検査 | 固定MultiFirmの `inspect --role host` で受け付け可能 |
| 更新計画 | `install-host` のローカル検査成功、書き込み予定はota_0のみ。実機レイアウトは未検証 |

標準書き込みの拒否試験には架空ポート `GUARD_TEST_NO_DEVICE` を明示し、終了コードが非0、
エラーが `MultiFirm host protection: blocked` であることを確認した。
容量境界試験は一時ファイルを使用し、実機には適用していない。

再現コマンドは [製品用手順](../product-build.md) を参照。上表は実機確認前のPC検証結果を保持している。

## 確認した制約

- PlatformIO Core 6.2.0はpreスクリプトより前に依存解決を行う。`clean` を同時指定すると
  ビルド成果物の削除も先行する。計画の「依存取得前」の実行順序は保証できない。
  保護スクリプト自体は依存ライブラリを読み込まず、機器への書き込みはその前に拒否する。
- M5Unifiedの `RTC_PowerHub_Class.cpp` に既存のmaybe-uninitialized警告がある。
  試作と同じ警告の非エラー化設定でビルド成功。依存ライブラリへの変更は行っていない。
- 起動ログ・描画・StopWatch認識・PSRAM実容量・使用したブートローダとの互換性は下表の実機確認で確認済み。
- 初回レイアウト導入・ブートローダ更新、ゲスト起動UIは今回の実装対象外。

## ユーザーによる実機確認欄

ユーザーが初回導入（initial）後にinstall-hostを実施した。
initialによる初期化と、その後のホスト更新における保持を区別して記録する。

| 項目 | 結果・記録先 |
|---|---|
| 試験日時／機体／COM／既存ブートローダ情報 | 2026-09-20 2:10ごろ |
| インストールしたホストのSHA-256 | b1ab250734799bff01b9c8626b4a467cef08442aee4937a9a643bf772f0848cd |
| 全体バックアップ／更新ログの保存先 | task1-validation.log |
| install-hostの読み戻し・イメージ検証 | 完了 |
| ゲスト3領域・NVS・multifirm_nvs・otadata保持 | initialでは初期化。その後のinstall-hostではゲスト領域のバックアップ一致と各設定領域の不変をログで確認 |
| ブートローダ・表・メタデータ保持 | initialでは更新・初期化。その後のinstall-hostではバックアップ一致をログで確認 |
| ota_0から起動しMultiFirm host readyを表示 | OK |
| 起動ログの版／認識番号30／表示サイズ／240MHz／Flash・PSRAM／キャッシュ | OK |
| 内部RAM・PSRAMの空きと最大連続領域 | internal total=386251 free=324947 largest=270336 bytes, psram total=8388608 free=7942728 largest=7864320 bytes |
| 5分以上の待機で再起動・WDTなし | OK |

作業1の「ゲスト領域を上書きしない更新経路」を確認済み。
今回は初期化後のゲスト領域での保持検証であり、実アプリを入れた各ゲストの起動・復帰と
ホスト更新後の保持は作業6で改めて確認する。
