# 製品用ビルド・導入・復旧（作業1）

ルートの構成はMultiFirm v1の `ota_0` 用です。現在は起動確認画面だけを表示します。
時計・アプリ一覧・ゲスト起動は後続作業です。実機検証はユーザーが行います。
検証結果は [作業1の検証記録](task1/task1-validation.md) に記録しています。

`measurement/` は別の単体測定用プロジェクトです。そのupload手順をMultiFirm機へ適用しないでください。
製品構成では `upload` / `uploadfs` / `uploadfsota` / `erase` を拒否し、解除設定は設けていません。
保護はローカルのpreスクリプトで実行し、取得済みライブラリに依存しません。
PlatformIO Core 6.2.0は依存解決や `clean` をpreスクリプトより先に実行する場合があります。
拒否するのは機器への書き込み処理であり、依存取得やビルド成果物の削除の前とは限りません。

## ビルドとPC上の検証

リポジトリのルートで実行します。PlatformIO Core、Git、Pythonを使用します。

```powershell
pio run -e m5stopwatch
python -m unittest discover -s tests -v
python tools/verify_build.py
```

`verify_build.py` はSDK設定、MultiFirmコミット、正規CSVと生成パーティション表、
4MiB上限を検査し、固定依存内のツールで `inspect --role host` と `install-host` の
計画表示を実行します。実機には接続しません。Python標準ライブラリだけで実行できます。
ビルドログの依存グラフでM5Unified 0.2.16、M5GFX 0.2.28、MultiFirm sha.f5a8fd8を確認します。

通常の再ビルドでも、バイナリを再生成しなくても容量検査が走ります。
生成済みSDK設定は既定値より優先されます。設定を変更した場合は、クリーンビルドして
`verify_build.py` で生成設定を確認してください。

```powershell
pio run -e m5stopwatch -t clean
# ルートの生成設定だけを削除（存在する場合）。measurement/には触れません。
Remove-Item -LiteralPath ./sdkconfig.m5stopwatch -ErrorAction SilentlyContinue
pio run -e m5stopwatch
python tools/verify_build.py
```

成果物は `.pio/build/m5stopwatch/firmware.bin`。更新に渡すのはこのアプリ単体です。
ビルドで生成されるブートローダやパーティション表は、通常更新では書き込みません。
PlatformIO冒頭のハードウェア表示はアプリ容量上限の4MBを表示する場合があります。
物理Flash設定は16MBで、生成SDK設定・イメージ検査・実機ログで確認します。

## MultiFirm v1導入済み試験機のホスト更新

以下はユーザーが実行する実機手順です。シリアルモニターを閉じ、COM番号は対象機に置き換えます。
読み取りコマンドでも接続時に機器がリセットされます。

ツールもファームウェアと同じ固定コミットを使います。ビルド後の依存内に含まれています。
Python 3.11とesptool 4.12.0の環境を、MultiFirm付属手順に従って準備します。

```powershell
$multifirm = (Resolve-Path .pio/libdeps/m5stopwatch/M5StopWatch-MultiFirm).Path
py -3.11 -m venv "$multifirm/tools/.venv"
& "$multifirm/tools/.venv/Scripts/python.exe" -m pip install -r "$multifirm/tools/requirements.txt"

# ローカル検査と計画表示。機器には接続しない。
& "$multifirm/tools/multifirm.ps1" inspect .pio/build/m5stopwatch/firmware.bin --role host
& "$multifirm/tools/multifirm.ps1" install-host .pio/build/m5stopwatch/firmware.bin

# 機器の検証・全体バックアップ・書き込み前確認。
& "$multifirm/tools/multifirm.ps1" status --port COMxx --verify
& "$multifirm/tools/multifirm.ps1" backup --port COMxx
& "$multifirm/tools/multifirm.ps1" install-host .pio/build/m5stopwatch/firmware.bin --port COMxx --check-device

# ota_0だけを更新し、読み戻しと対象外領域の保持を確認。
& "$multifirm/tools/multifirm.ps1" install-host .pio/build/m5stopwatch/firmware.bin --port COMxx --execute
pio device monitor -e m5stopwatch --port COMxx
```

バックアップと実行ログの保存先はコマンド出力で確認し、バックアップはビルドディレクトリ外にも保管します。
`install-host` は現状と一致する全体バックアップを確認し、なければ新しく取得します。
`ota_0` を消去・更新し、ゲスト、NVS、multifirm_nvs、otadata、ブートローダ、表などの保持を検査します。
ランチャーはNVS初期化・消去、メタデータ更新、ゲスト走査を行いません。

更新はotadataを保持するため、選択済みゲストが起動する場合があります。
その場合はゲストの既存のホーム復帰操作、または下記の `recover` でホストへ戻します。
最小ホストにはまだゲストを選んで起動するUIがありません。

確認画面の `MultiFirm host ready` と起動ログを確認します。ログには版・IDF・認識ボード番号・
表示サイズ・CPU・メモリ・実行パーティションが出ます。
ボードがStopWatch（固定ライブラリでは認識番号30）として認識され、CPU 240MHz、Flash 16MiB、PSRAMが利用可能、
キャッシュ16/64KiB・64B、`running=ota_0 address=0x20000 size=0x400000`、
`layout=valid host=valid` であることを確認してください。画面サイズは検出値を使用します。
5分以上待機し、再起動・WDTがないことも確認します。

## 初回導入・復旧

- 非対応レイアウトの機体には `install-host` を実行しても導入できません。
  初回導入とブートローダ変更は [MultiFirmの公式手順](https://github.com/wararyo/M5StopWatch-MultiFirm/blob/f5a8fd88f50dfb50c6e6e1e82bccc491a936bd47/tools/README.md)
  の `initial` を使用します。これはNVSとゲストを消去する別作業です。
  本作業では製品ビルドを `initial --host-build` に渡す経路を検証していません。
  既存の検証済み導入構成でMultiFirm v1を導入した後、このホストを更新してください。
- 正常なゲストから操作で戻れない場合は、`multifirm.ps1 recover --port COMxx --execute`。
  ホストと表を検証したうえでotadataだけを消去します。
- ホストが破損している場合、`recover` だけでは直りません。保存しておいた正常なホストの
  `.bin` を `install-host` で再インストールします。
- 書き込み途中で失敗した場合はツールのログに従い、同じ更新の再実行またはMultiFirm側の
  バックアップ復元手順を使用します。通常uploadや全Flash消去で復旧しないでください。

作業1は2026-09-20にPC上の検証とユーザーによる実機確認を終え、完了しました。
