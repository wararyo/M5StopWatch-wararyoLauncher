# 作業10: フォント資産の記録

作成日: 2026-09-26（10-0）。状態: 書体・サイズ・収録文字を確定。VLWの生成・組み込みは10-3で行う。
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
- `src/ui/graphics/fonts/OFL-D-DIN-PRO.txt` — 10-3でVLWをコミットするときに、配布物の`OFL-1.1.txt`（著作権表示を含む）から追加する。

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

## 6. 生成手順

10-3で`build_font.py`に`--output`・`--chars-file`を追加した後、実際のコマンドと生成物のSHA-256をここへ追記する。
既存の日本語資産は引数省略時の動作と`--check`で従来どおり再現できることを確認する。
