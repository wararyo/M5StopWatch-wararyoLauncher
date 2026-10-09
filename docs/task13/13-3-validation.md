# 作業13-3 検証記録: 歩数計画面と一覧

2026-10-08。計画は[plan-13-3.md](plan-13-3.md)。状態: 完了。全面描画との画素比較の追加は13-4で行う。

## 1. 変更

- 一覧: `LaunchTargetId::Pedometer=6`、タイマーの次に「歩数計」（`Pedometer`）、`IconId::Pedometer`（末尾に追加）、色`PedometerColors`（`#28B33D`の地に白）。
- 文言: `text::Pedometer`（歩数計／Pedometer）、`text::StepsToday`（今日の歩数／Steps today）、`text::StepsUnit`（歩／steps）。
- `src/features/pedometer/`: `PedometerModel.h`（3桁区切りの書式`formatSteps()`）、`PedometerLayout.h`、`PedometerScreen.*`、`PedometerLayer.*`。
- ホスト: `ScreenId::Pedometer`、`FrameLayer::Pedometer`、`FrameModel::pedometer`、`FrameComposer`、`HostRenderer`、`ScreenManager::bindPedometer()`、`HostApplication`。
- `RenderDiagnostics.cpp`: 画面写真（`LAUNCHER_RENDER_SHOTS`）に歩数計の4枚を足した。
- 一覧の並びを前提にしていた既存のテスト（`runtime_tests`・`ui_tests`・`settings_tests`）を、設定の行をIDで探す形などに直した。
- `docs/architecture.md`: Registryの項目、描画の順序と下地、2.3節に画面と描画を追記。

## 2. 資産

| 資産 | 元ファイル | 内容 | bytes | SHA-256 |
|---|---|---|---:|---|
| `DDinProExpSemiBold64.vlw`（作り直し） | `D-DIN-PRO-Exp-600-SemiBold.ttf`（SHA-256 `cfaed390…`、作業10の記録と一致） | 64px、`tools/fonts/numbers.txt`（`0-9 : - ,`）、13グリフ | 13,708 | `7c55de492a3a52e4da1285fe2ac0ee602dec14425b10f8e5c56142a9fc651738` |
| `GenShinGothicMedium28.vlw`（作り直し） | `GenShinGothic-Medium.ttf`（SHA-256 `772181b6…`、同上） | 355グリフ（「今」「数」「歩」を追加） | 181,471 | `9de2c56726d71d846196d3d4f3ff30873b58ec490a407520da23f4dc91b29919` |
| `AppIcons.bin`（作り直し） | `icons/*.png` | 7アイコン（末尾に`ic_pedometer.png`） | 13,564 | `5e65c88c6575bca5d1d3f9a4f97c9c5aad136a88f3d6fa2e219040bd022c57fb` |

```sh
python tools/build_font.py --source <D-DIN>/Exp/D-DIN-PRO-Exp-600-SemiBold.ttf --size 64 \
    --chars-file tools/fonts/numbers.txt --output src/ui/graphics/fonts/DDinProExpSemiBold64.vlw
python tools/build_font.py --source <GenShin>/GenShinGothic-Medium.ttf
python tools/build_icons.py
```

作り直す前の版とグリフごとに比べ、既存のグリフは寸法・画素とも差0、増えたのは上の文字だけだった（[フォントのログ](20261008-13-3-fonts.log)、[アイコンのログ](20261008-13-3-icons.log)。アイコンは既存の6個のマスクがバイト単位で一致）。
64pxの組は`,`がベースラインの下へ出るので、descentが1から11になった。タイマーは「0」の高さと文字幅だけで配置しているため影響しない（描画検証で確認、4節）。

## 3. ホストテスト

`python tools/test_runtime.py`: 全10スイートが日本語・英語でPASS（[ログ](20261008-13-3-host-8.log)）。途中の失敗は、一覧の並びを前提にした既存のテストと、自分で書いた切り詰めのテストの期待値の誤りで、[1](20261008-13-3-host-1.log)〜[7](20261008-13-3-host-7.log)回目のログに残した。

`pedometer_tests`に足したもの:

- 書式: 0、999、1,000、12,345、123,456、1,234,567、4,294,967,295、IMUなしの`--`、小さい領域で溢れないこと。
- 一覧: タイマーの直後、ID 6、アイコンと色。OK・当たり判定・歩数・題の箱が円の内側にあること。
- 画面: 一覧から開くと開いた時点の歩数を出し、表示中は1秒以内に新しい歩数を描くこと。A・B・OKのタップで一覧へ戻り、題のタップでは戻らないこと。
  OKの当たり判定の下の延長部でも戻ること。A+Bでホームへ戻ること。閉じた後は期限を持たないこと。
- IMUなし: 開けて`--`を出すこと。サービスがなければ開けないこと。

## 4. ビルド・描画検証

| 項目 | 結果 |
|---|---|
| 通常版 `m5stopwatch` | SUCCESS（[ログ](20261008-13-3-build-m5stopwatch-3.log)）。`verify_build.py` OK、ホスト1,303,184／4,194,304 bytes（[ログ](20261008-13-3-verify-m5stopwatch-2.log)） |
| 英語版 `m5stopwatch-en` | SUCCESS（[ログ](20261008-13-3-build-m5stopwatch-en-2.log)） |
| 描画検証版 `m5stopwatch-render-check` | SUCCESS（[ログ](20261008-13-3-build-m5stopwatch-render-check-2.log)） |
| 画面写真版 | [ビルド](20261008-13-3-build-render-shots.log)、[導入](20261008-13-3-install-render-shots.log)、[ログ](20261008-13-3-render-shots.log)。981件・不一致0件 PASS（12-4の976件から一覧の行の増加分）。59枚（12-4の55枚＋歩数計4枚）。写真は[13-3-shots](13-3-shots/) |

最初のビルドは、`PedometerLayer.h`に`Viewport`のインクルードがなく失敗した（描画レイヤーはホストテストでコンパイルされないため、そこで気づかなかった）。直して3環境とも成功した。

画面写真（[pedometer](13-3-shots/pedometer.png)、[pedometer-long](13-3-shots/pedometer-long.png)、[pedometer-none](13-3-shots/pedometer-none.png)、[pedometer-row](13-3-shots/pedometer-row.png)）は、
[デザイン](../Images/Pedometer/Steps.png)と同じ配置で描けている。7桁（`1,234,567 歩`）も円の内側に収まる。下端の赤い線は、パネルが描かない行を読み出したもので、以前の写真と同じ。

## 5. 実機

通常版を入れ直し、起動を確認した（[導入](20261008-13-3-install-product.log)、[起動ログ](20261008-13-3-product-boot.log)）。
`[Pedometer] begin ... steps=72 record=yes day=20733`（13-2の記録を引き継いだ）、アイコン7個、`DDinProExpSemiBold64 ... descent=11`を読み込んだ。

- [ ] 実機での操作（一覧から開く、OK・A・Bで戻る、歩きながら数が増える）は、ユーザーの立ち会いで確認する。
