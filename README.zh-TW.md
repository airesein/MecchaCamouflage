<p align="center">
  <img
    src="docs/assets/meccha-camouflage-readme-banner-v151-1600w.jpg"
    alt="Meccha Camouflage 示範"
    width="900"
  />
</p>

<p align="center">
  <a href="LICENSE.txt">
    <img
      src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg"
      alt="授權條款：GPL-3.0-or-later"
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
    alt="Meccha Camouflage 圖示"
    width="36"
  />
  Meccha Camouflage
</h1>

一款適用於 MECCHA CHAMELEON 的 Windows 桌面應用程式。

本專案是基於
[acentrist/MecchaCamouflage](https://github.com/acentrist/MecchaCamouflage)
開發的非官方修改版，與上游專案無隸屬關係，亦未獲得其認可。

## 功能

- **塗裝**：使用自訂顏色與材質塗裝玩家角色。
- **圖像塗裝**：將匯入的圖片塗裝到玩家角色上。
- **ESP**：在遊戲中顯示玩家位置與資訊。

## 下載

從
<a href="https://github.com/airesein/MecchaCamouflage/releases/latest">GitHub Releases</a>
下載最新的 <code>meccha-camouflage.exe</code>。

## 使用方式

1. 啟動 MECCHA CHAMELEON。
2. 啟動 `meccha-camouflage.exe`。

日誌寫入位置：

```text
%LOCALAPPDATA%\MecchaCamouflage\versions\<version>\logs\
```

若 Windows 彈出提示，請於啟動時允許 UAC，以加入下列 Microsoft Defender
排除項目：

```text
%LOCALAPPDATA%\MecchaCamouflage\
```

## 開發

```bash
git clone --recurse-submodules https://github.com/airesein/MecchaCamouflage.git
cd MecchaCamouflage
make run
```

## 安全性

請遵循 [SECURITY.md](SECURITY.md) 中的漏洞揭露流程。

## 授權條款

本專案依
[GNU 通用公共授權條款 v3.0 或更新版本](LICENSE.txt) 開源。

[GPL-3.0-or-later](LICENSE.txt) © Acentrist 與貢獻者。
