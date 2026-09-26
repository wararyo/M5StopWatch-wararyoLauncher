# 作業10: フォント資産の記録

作成日: 2026-09-26（10-0）。状態: 書体・サイズ・収録文字を確定。
更新日: 2026-09-26（10-3）。VLWを生成・組み込み、生成手順とハッシュを6節に記録。時刻の描き方とコロンの位置を7節に記録。
[作業10の計画](plan.md)3節と[10-3](plan-10-3.md)2節の「元ファイル・ライセンス・生成手順・サイズ・収録文字」の記録先。

## 1. 採用する書体（2026-09-26確定）

| 資産（予定名） | 用途 | 元ファイル | サイズ | 収録文字 |
|---|---|---|---:|---|
| `DDinProExpSemiBold100.vlw` | Digitalの時刻（`HH:mm`・`HH:mm:ss`とも） | `TTF/Exp/D-DIN-PRO-Exp-600-SemiBold.ttf` | 100px | `0-9 : -` |
| `DDinProExpBold28.vlw` | 日付など通常の文字。Digital・Forest共用 | `TTF/Exp/D-DIN-PRO-Exp-700-Bold.ttf` | 28px | 印字可能ASCII（U+0020〜U+007E） |
| `DDinProExpBold22.vlw` | 電池・バックグラウンド情報など小さな文字。両文字盤共用 | 同上 | 22px | 印字可能ASCII |
| `DDinProCondensedSemiBold120.vlw` | Forestの時刻（`HH:mm`・`HH:mm:ss`とも） | `TTF/Condensed/D-DIN-PRO-Condensed-600-SemiBold.ttf` | 120px | `0-9 : -` |

- サイズは`build_font.py`の`--size`と同じピクセルサイズ（ppem）。ポイントではない。
- 秒表示バリアントも時分と同じサイズを使う。別サイズの資産は作らない。実機の見た目が悪ければ見直す（暫定）。
- 時刻の`-`は無効時刻（`--:--`）用。
- 非ASCIIのラベルは既存の`GenShinGothicMedium28.vlw`と代替グリフで扱う（10-3）。D-DIN-PROに日本語はない。

## 2. 元ファイル

D-DIN-PRO: <https://github.com/CyberFei/D-DIN-PRO>。ZIP（`D-DIN-PRO-main`）をダウンロードして使用。
nameテーブルの版は`1.1.0`、著作権表示は`Copyright © CyberFei`。

| ファイル | SHA-256 |
|---|---|
| `TTF/Exp/D-DIN-PRO-Exp-600-SemiBold.ttf` | `cfaed3908f0d7a8404fd0463a9367f1cf5476d6861a825499d677ff8f0b5f06c` |
| `TTF/Exp/D-DIN-PRO-Exp-700-Bold.ttf` | `beeb7c102cd2f037fad3acfb4b0bccb1a7b920ff54d1b48079a3bdd822868d33` |
| `TTF/Condensed/D-DIN-PRO-Condensed-600-SemiBold.ttf` | `29e3dcf9e025e615077f5f72c904b2ff388b6d63909397b40cc8eb770c94dc5d` |

TrueTypeファイルは既存方針どおりリポジトリに含めない。生成物のVLWとライセンス文だけをコミットする。

既存の日本語資産の元ファイル（参考。作業3で導入、今回初めて記録）:

| ファイル | 版 | SHA-256 |
|---|---|---|
| `GenShinGothic-Medium.ttf`（genshingothic-20150607） | 1.002.20150607 | `772181b64f790984d71a6e23b32196efaf32e89b79756a341b1e2ee2028c09ce` |

## 3. ライセンス

どちらもSIL Open Font License 1.1。

| 条項 | 内容 | 対応 |
|---|---|---|
| 定義 | 形式の変換・グリフの削除も「Modified Version」 | VLWのサブセットは改変版として扱う |
| 第2条 | 再配布時は著作権表示とライセンス本文を各複製に含める | `src/ui/graphics/fonts/`に書体ごとのライセンス文を置く |
| 第3条 | 改変版は予約フォント名（RFN）を使えない。対象は利用者に示される主たるフォント名だけ | 下記 |
| 第5条 | 改変版もOFLのまま配布する | VLWはOFL。ファームウェア本体のライセンスには波及しない |

RFNはD-DIN-PROが「D-DIN-PRO」、GenShinGothicは元の源ノ角ゴシックの「Source」。
VLWはフォント名の欄を持たず、`build_font.py`も書き込まない。時計の利用者にフォント名を示すこともない。
そのためファイル名は利用者に示されるフォント名にあたらないと解釈し、**元のフォント名が類推できる名前にする**（既存の`GenShinGothicMedium28.vlw`と同じ形）。
あわせて、この文書とライセンス文で「元書体から生成したサブセットであり、オリジナル版ではない」ことを示す。

ライセンス文の配置:

- `src/ui/graphics/fonts/OFL-GenShinGothic.txt` — 10-0で追加。作業3の導入時に不足していたもの。
  著作権表示は元フォントのnameテーブル（ID 0）から転記。M+ FONTS由来のグリフはM+ FONTS LICENSE（無条件の利用・改変・再配布を許可）で、同梱義務はない。
- `src/ui/graphics/fonts/OFL-D-DIN-PRO.txt` — 10-3で配布物の`OFL-1.1.txt`（著作権表示を含む）をそのまま複製して追加した（SHA-256 `4b9161aa1e9978d729be842940dd14d9b865d195066346c3907f764f05518fa3`、元ファイルと同一）。

## 4. 実測（10-0、`build_font.py`の`render`・`build`で生成して計測）

| 資産 | グリフ | VLW bytes | ascent / descent | 数字の高さ | 数字の送り幅 | `:` / `-` |
|---|---:|---:|---|---:|---|---|
| ExpSemiBold100 | 12 | 31,970 | 71 / 1 | 72 | 38〜58 | 22 / 43 |
| ExpBold28 | 95 | 25,562 | 22 / 5 | 20 | 11〜17 | 7 / 12 |
| ExpBold22 | 95 | 17,205 | 18 / 4 | 16 | 9〜13 | 5 / 9 |
| CondensedSemiBold120 | 12 | 33,663 | 84 / 1 | 85 | 34〜50 | 21 / 46 |

合計108,400 bytes。4MiBのホスト上限に対して容量の問題はない。

| 文字列の幅（px） | `00:00` | `00:00:00` | `--:--` | その他 |
|---|---:|---:|---:|---|
| ExpSemiBold100 | 246 | 380 | 194 | |
| ExpBold28 | 71 | 110 | 55 | `SEP 26 FRI` 143 |
| ExpBold22 | 57 | 88 | 41 | `100%` 57 |
| CondensedSemiBold120 | 217 | 336 | 205 | |

画面は466×466の円形。Digitalの`HH:mm:ss`（380px）は中央で左右約43pxの余白になる。情報欄により時刻を上下へずらすと円の弦が短くなるため、10-3で配置を確認する。

## 5. 数字の幅

- **数字がプロポーショナル（2026-09-26に解決）**: 両書体とも等幅数字（`tnum`）の機能・グリフを持たない（GSUBは`frac`・`sups`等のみ）。
  `1`などの狭い字を詰めたほうが美しいので、VLWは送り幅を変えずに生成する。揺れと再描画領域の増加は配置で抑える（ユーザー決定）。
  - `HH:mm`: HHを右揃え、mmを左揃え。`HH:mm:ss`: HHを右揃え、mmを中央揃え、ssを左揃え。
  - コロンの位置はバリアントごとに固定。DigitalとForestの両方に適用する。

桁の組の最大幅（送り幅の合計、px）:

| 資産 | HH（00〜23） | mm・ss（00〜59） | `--` | `:` | `HH:mm`固定幅 | `HH:mm:ss`固定幅 |
|---|---|---|---:|---:|---:|---:|
| ExpSemiBold100 | 76〜114（最大`08`） | 76〜115（最大`48`） | 86 | 22 | 251 | 388 |
| CondensedSemiBold120 | 68〜99（最大`04`） | 68〜100（最大`44`） | 92 | 21 | 220 | 341 |

インクの張り出しは10-3の配置計算で確認する。

## 6. 生成手順（10-3）

`build_font.py`に`--output`（書き出し・検査する資産）と`--chars-file`（収録する文字をUTF-8で列挙したファイル。改行以外のすべての文字）を追加した。
どちらも省略すると従来どおり日本語資産を作る。収録文字の一覧は`tools/fonts/`に置く。

| ファイル | 内容 | SHA-256 |
|---|---|---|
| `tools/fonts/time.txt` | `0123456789:-` | `3917e74c24915c96fd8937978fbac77c712b666b2abf4161e8465c212ac76ae5` |
| `tools/fonts/ascii.txt` | U+0020〜U+007E | `01dbc9a42e2c64d4ecbeb1925bebffadb1c1174a3f6695a3897853cb1135e7b7` |

`<D-DIN>`は`D-DIN-PRO-main/TTF`、`<GenShin>`は`GenShinGothic-Medium.ttf`（2節）。freetype-py 2.13.2（10-0と同じ）。

```sh
python tools/build_font.py --source <D-DIN>/Exp/D-DIN-PRO-Exp-600-SemiBold.ttf --size 100 \
    --chars-file tools/fonts/time.txt --output src/ui/graphics/fonts/DDinProExpSemiBold100.vlw
python tools/build_font.py --source <D-DIN>/Exp/D-DIN-PRO-Exp-700-Bold.ttf --size 28 \
    --chars-file tools/fonts/ascii.txt --output src/ui/graphics/fonts/DDinProExpBold28.vlw
python tools/build_font.py --source <D-DIN>/Exp/D-DIN-PRO-Exp-700-Bold.ttf --size 22 \
    --chars-file tools/fonts/ascii.txt --output src/ui/graphics/fonts/DDinProExpBold22.vlw
python tools/build_font.py --source <D-DIN>/Condensed/D-DIN-PRO-Condensed-600-SemiBold.ttf --size 120 \
    --chars-file tools/fonts/time.txt --output src/ui/graphics/fonts/DDinProCondensedSemiBold120.vlw
python tools/build_font.py --source <GenShin>          # 日本語資産（引数省略時の従来動作）
```

各コマンドに`--check`を付けると、コミット済みの資産が元ファイルと収録文字から作り直した結果と一致するか検査する。
10-3の完了時点で5資産とも一致した（[ログ](20260926-10-3-font-check-final.log)）。

| 資産 | グリフ | bytes | SHA-256 |
|---|---:|---:|---|
| `DDinProExpSemiBold100.vlw` | 12 | 31,970 | `19b44192162c6e11c088aaa7f5b3bae820d4905c9d41ff01e81c22a877e7f458` |
| `DDinProExpBold28.vlw` | 95 | 25,562 | `41556fa3b3b8eb3e07cf8da2cdc9067014031b356978f9f43d0dab15dc53226e` |
| `DDinProExpBold22.vlw` | 95 | 17,205 | `6da732877d4b9ea4bbb1a24a3b74d6e97b33bd33652c4ad0e01ac1f1f6475853` |
| `DDinProCondensedSemiBold120.vlw` | 12 | 33,663 | `fdbf09203d4ee3985c6462320dbff2506127e826f37130355a7700d76aa9b389` |
| `GenShinGothicMedium28.vlw` | 347 | 175,599 | `f7687e8a5214eeea3d8b90b85c90aab4a41e5639e494c85756bb0fd9c3b63d2d` |

bytesは4節の実測と一致する。日本語資産は[10-0の所見1](baseline.md)を「未収録の字を収録して再生成」で解消した。
引数省略時の従来動作のまま作り直すと、既存の340字は寸法・画素とも差0で、増えたのは`src/`にだけある7字
（描画検証の6字と、情報欄の検証ラベル「計測中」の「測」）（[比較ログ](20260926-10-3-font-genshin.log)）。

## 7. 描き方（10-3）

- 時刻の数字（`DDinPro*SemiBold*`）はLovyanGFXのフォントとして読み込まない。`VLWfont::drawChar`は1文字ごとにグリフ全体をスタックへ複製する（`alloca(幅×高さ)`、100pxの数字で約3.6KB）。
  UIタスク（8KiB）で試したところスタック余裕が5,700→2,676 bytesに落ちたため、`ui/graphics/VlwGlyphs`で資産の中のビットマップを直接指し、`pushGrayscaleImage`で描く。
  画素はLovyanGFXで描いた場合とバイト単位で一致した（描画検証の画像で確認）。スタック余裕は5,412 bytesに戻った。
- 日付・電池・APPS・情報ラベル（28px・22px）は通常のVLWフォントとして描く（1文字数百bytes）。
- D-DIN-PROのコロンは数字の中心より約11px低い。Digitalでは**8px上げて描く**（2026-09-26、画像を見てユーザーが決定）。持ち上げた後も時刻のascent（71px）に収まる。
