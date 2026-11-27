<p align="center">
  <img
    src="docs/assets/meccha-camouflage-readme-banner-v151-1600w.jpg"
    alt="Meccha Camouflage デモ"
    width="900"
  />
</p>

<p align="center">
  <a href="LICENSE.txt">
    <img
      src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg"
      alt="ライセンス: GPL-3.0-or-later"
    />
  </a>
</p>

<p align="center">
  <a href="README.md">English</a> |
  <a href="README.zh-CN.md">简体中文</a> |
  <a href="README.zh-TW.md">繁體中文</a> |
  <a href="README.ja.md">日本語</a> |
  <a href="README.ko.md">한국어</a>
</p>

<h1>
  <img
    src="resources/app-icons/icon.png"
    alt="Meccha Camouflage アイコン"
    width="36"
  />
  Meccha Camouflage
</h1>

MECCHA CHAMELEON 向けの Windows デスクトップアプリです。

本プロジェクトは
[acentrist/MecchaCamouflage](https://github.com/acentrist/MecchaCamouflage)
をベースにした非公式の改変版です。上流プロジェクトとは提携しておらず、承認も受けていません。

## 機能

- **ペイント**：好きな色とマテリアルでプレイヤーキャラクターを塗装します。
- **画像ペイント**：読み込んだ画像をプレイヤーキャラクターに塗装します。
- **ESP**：ゲーム内でプレイヤーの位置と情報を表示します。

## ダウンロード

<a href="https://github.com/airesein/MecchaCamouflage/releases/latest">GitHub Releases</a>
から最新の <code>meccha-camouflage.exe</code> をダウンロードしてください。

## 使い方

1. MECCHA CHAMELEON を起動します。
2. `meccha-camouflage.exe` を起動します。

ログは次の場所に書き込まれます：

```text
%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\logs\
```

Windows の確認が表示された場合は、起動時の UAC を承認して次の Microsoft
Defender の除外項目を追加してください：

```text
%LOCALAPPDATA%\MecchaCamouflage\
```

## 開発

```bash
git clone --recurse-submodules https://github.com/airesein/MecchaCamouflage.git
cd MecchaCamouflage
make run
```

## セキュリティ

脆弱性の報告は [SECURITY.md](SECURITY.md) の手順に従ってください。

## ライセンス

本プロジェクトは
[GNU General Public License v3.0 以降](LICENSE.txt) の下でオープンソースとして公開されています。

[GPL-3.0-or-later](LICENSE.txt) © Acentrist および貢献者。
